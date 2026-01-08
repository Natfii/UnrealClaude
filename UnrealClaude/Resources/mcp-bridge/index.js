#!/usr/bin/env node

/**
 * UnrealClaude MCP Bridge
 *
 * This bridges Claude Code's MCP protocol to the UnrealClaude plugin's HTTP REST API.
 * The plugin runs an HTTP server on localhost (default port 3000) with editor manipulation tools.
 *
 * Environment Variables:
 *   UNREAL_MCP_URL - Base URL for Unreal MCP server (default: http://localhost:3000)
 *   MCP_REQUEST_TIMEOUT_MS - HTTP request timeout in milliseconds (default: 30000)
 *   INJECT_CONTEXT - Enable automatic context injection on tool calls (default: false)
 */

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";

// Dynamic context loader for UE 5.7 API documentation
import {
  getContextForTool,
  getContextForQuery,
  listCategories,
  getCategoryInfo,
  loadContextForCategory,
} from "./context-loader.js";

// Configuration with defaults
const CONFIG = {
  unrealMcpUrl: process.env.UNREAL_MCP_URL || "http://localhost:3000",
  requestTimeoutMs: parseInt(process.env.MCP_REQUEST_TIMEOUT_MS, 10) || 30000,
  injectContext: process.env.INJECT_CONTEXT === "true", // Auto-inject context on tool responses
};

/**
 * Structured logging helper - writes to stderr to not interfere with MCP protocol
 */
const log = {
  info: (msg, data) => console.error(`[INFO] ${msg}`, data ? JSON.stringify(data) : ""),
  error: (msg, data) => console.error(`[ERROR] ${msg}`, data ? JSON.stringify(data) : ""),
  debug: (msg, data) => process.env.DEBUG && console.error(`[DEBUG] ${msg}`, data ? JSON.stringify(data) : ""),
};

/**
 * Fetch with timeout using AbortController
 */
async function fetchWithTimeout(url, options = {}) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), CONFIG.requestTimeoutMs);

  try {
    const response = await fetch(url, {
      ...options,
      signal: controller.signal,
    });
    return response;
  } finally {
    clearTimeout(timeout);
  }
}

/**
 * Fetch tools from the UnrealClaude HTTP server
 */
async function fetchUnrealTools() {
  try {
    const response = await fetchWithTimeout(`${CONFIG.unrealMcpUrl}/mcp/tools`);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    return data.tools || [];
  } catch (error) {
    if (error.name === "AbortError") {
      log.error("Request timeout fetching tools", { url: `${CONFIG.unrealMcpUrl}/mcp/tools` });
    } else {
      log.error("Failed to fetch tools from Unreal", { error: error.message });
    }
    return [];
  }
}

/**
 * Execute a tool via the UnrealClaude HTTP server
 */
async function executeUnrealTool(toolName, args) {
  const url = `${CONFIG.unrealMcpUrl}/mcp/tool/${toolName}`;
  try {
    const response = await fetchWithTimeout(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(args || {}),
    });

    const data = await response.json();
    log.debug("Tool executed", { tool: toolName, success: data.success });
    return data;
  } catch (error) {
    const errorMessage = error.name === "AbortError"
      ? `Request timeout after ${CONFIG.requestTimeoutMs}ms`
      : error.message;
    log.error("Tool execution failed", { tool: toolName, error: errorMessage });
    return {
      success: false,
      message: `Failed to execute tool: ${errorMessage}`,
    };
  }
}

/**
 * Check if Unreal Editor is running with the plugin
 */
async function checkUnrealConnection() {
  try {
    const response = await fetchWithTimeout(`${CONFIG.unrealMcpUrl}/mcp/status`);
    if (response.ok) {
      const data = await response.json();
      return { connected: true, ...data };
    }
    return { connected: false, reason: `HTTP ${response.status}` };
  } catch (error) {
    const reason = error.name === "AbortError" ? "timeout" : error.message;
    return { connected: false, reason };
  }
}

/**
 * Convert Unreal tool parameter schema to MCP tool input schema
 */
function convertToMCPSchema(unrealParams) {
  const properties = {};
  const required = [];

  for (const param of unrealParams || []) {
    const prop = {
      type: param.type === "number" ? "number" :
            param.type === "boolean" ? "boolean" :
            param.type === "array" ? "array" :
            param.type === "object" ? "object" : "string",
      description: param.description,
    };

    if (param.default !== undefined) {
      prop.default = param.default;
    }

    properties[param.name] = prop;

    if (param.required) {
      required.push(param.name);
    }
  }

  return {
    type: "object",
    properties,
    required: required.length > 0 ? required : undefined,
  };
}

/**
 * Convert Unreal tool annotations to MCP annotations format
 */
function convertAnnotations(unrealAnnotations) {
  if (!unrealAnnotations) {
    // Default annotations for tools without explicit annotations
    return {
      readOnlyHint: false,
      destructiveHint: true,
      idempotentHint: false,
      openWorldHint: false,
    };
  }
  return {
    readOnlyHint: unrealAnnotations.readOnlyHint ?? false,
    destructiveHint: unrealAnnotations.destructiveHint ?? true,
    idempotentHint: unrealAnnotations.idempotentHint ?? false,
    openWorldHint: unrealAnnotations.openWorldHint ?? false,
  };
}

// Create the MCP server
const server = new Server(
  {
    name: "unrealclaude",
    version: "1.2.0",
  },
  {
    capabilities: {
      tools: {},
    },
  }
);

// Cache for tools (refreshed on each list request)
let cachedTools = [];

// Handle list_tools request
server.setRequestHandler(ListToolsRequestSchema, async () => {
  // Check connection first
  const status = await checkUnrealConnection();

  if (!status.connected) {
    log.info("Unreal not connected", { reason: status.reason });
    // Return a status tool when not connected
    return {
      tools: [
        {
          name: "unreal_status",
          description: "Check if Unreal Editor is running with UnrealClaude plugin. Currently: NOT CONNECTED. Please start Unreal Editor with UnrealClaude plugin enabled.",
          inputSchema: {
            type: "object",
            properties: {},
          },
        },
      ],
    };
  }

  // Fetch tools from Unreal
  const unrealTools = await fetchUnrealTools();
  cachedTools = unrealTools;

  // Convert to MCP format with annotations
  const mcpTools = unrealTools.map((tool) => ({
    name: `unreal_${tool.name}`,
    description: `[Unreal Editor] ${tool.description}`,
    inputSchema: convertToMCPSchema(tool.parameters),
    annotations: convertAnnotations(tool.annotations),
  }));

  // Add status tool with read-only annotations
  mcpTools.unshift({
    name: "unreal_status",
    description: `Check Unreal Editor connection status. Currently: CONNECTED to ${status.projectName || "Unknown Project"} (${status.engineVersion || "Unknown"})`,
    inputSchema: {
      type: "object",
      properties: {},
    },
    annotations: {
      readOnlyHint: true,
      destructiveHint: false,
      idempotentHint: true,
      openWorldHint: false,
    },
  });

  // Add UE context tool for querying API documentation
  mcpTools.push({
    name: "unreal_get_ue_context",
    description: `Get Unreal Engine 5.7 API context/documentation. Use when you need UE5 API patterns, examples, or best practices. Categories: ${listCategories().join(", ")}. Can also search by query keywords.`,
    inputSchema: {
      type: "object",
      properties: {
        category: {
          type: "string",
          description: `Specific category to load: ${listCategories().join(", ")}`,
        },
        query: {
          type: "string",
          description: "Search query to find relevant context (e.g., 'state machine transitions', 'async loading')",
        },
      },
    },
    annotations: {
      readOnlyHint: true,
      destructiveHint: false,
      idempotentHint: true,
      openWorldHint: false,
    },
  });

  log.info("Tools listed", { count: mcpTools.length, connected: true });
  return { tools: mcpTools };
});

// Handle call_tool request
server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

  // Handle UE context request
  if (name === "unreal_get_ue_context") {
    const { category, query } = args || {};

    let result = null;
    let matchedCategories = [];

    // If specific category requested
    if (category) {
      const content = loadContextForCategory(category);
      if (content) {
        result = content;
        matchedCategories = [category];
      } else {
        return {
          content: [
            {
              type: "text",
              text: `Unknown category: ${category}. Available categories: ${listCategories().join(", ")}`,
            },
          ],
          isError: true,
        };
      }
    }
    // If query provided, search by keywords
    else if (query) {
      const queryResult = getContextForQuery(query);
      if (queryResult) {
        result = queryResult.content;
        matchedCategories = queryResult.categories;
      } else {
        return {
          content: [
            {
              type: "text",
              text: `No context found for query: "${query}". Try categories: ${listCategories().join(", ")}`,
            },
          ],
          isError: false,
        };
      }
    }
    // No params - list available categories
    else {
      const categoryList = listCategories().map((cat) => {
        const info = getCategoryInfo(cat);
        return `- **${cat}**: Keywords: ${info.keywords.slice(0, 5).join(", ")}...`;
      });

      return {
        content: [
          {
            type: "text",
            text: `# Available UE 5.7 Context Categories\n\n${categoryList.join("\n")}\n\nUse \`category\` param for specific context or \`query\` to search by keywords.`,
          },
        ],
      };
    }

    log.info("UE context loaded", { categories: matchedCategories });

    return {
      content: [
        {
          type: "text",
          text: `# UE 5.7 Context: ${matchedCategories.join(", ")}\n\n${result}`,
        },
      ],
    };
  }

  // Handle status check
  if (name === "unreal_status") {
    const status = await checkUnrealConnection();
    if (status.connected) {
      // Get tools and categorize them
      const unrealTools = await fetchUnrealTools();
      const categories = {};
      const brokenTools = [];

      for (const tool of unrealTools) {
        // Categorize by tool name prefix
        let category = "utility";
        if (tool.name.startsWith("blueprint_")) category = "blueprint";
        else if (tool.name.startsWith("anim_blueprint")) category = "animation";
        else if (tool.name.startsWith("asset_")) category = "asset";
        else if (tool.name.startsWith("task_")) category = "task_queue";
        else if (tool.name.includes("actor") || tool.name.includes("spawn") || tool.name.includes("move") || tool.name.includes("level")) category = "actor";

        categories[category] = (categories[category] || 0) + 1;

        // Check for tool issues (missing required fields)
        if (!tool.description || tool.description.length < 5) {
          brokenTools.push({ name: tool.name, issue: "missing description" });
        }
      }

      // Check context system status
      const contextCategories = listCategories();
      const testContext = loadContextForCategory("animation");
      const contextStatus = testContext
        ? `OK (${contextCategories.length} categories: ${contextCategories.join(", ")})`
        : "FAILED - context files not loading";

      // Build response - only include broken tools if there are any
      const response = {
        connected: true,
        project: status.projectName,
        engine: status.engineVersion,
        context_system: contextStatus,
        tool_summary: categories,
        total_tools: unrealTools.length,
        message: "Unreal Editor connected. All tools operational.",
      };

      if (brokenTools.length > 0) {
        response.broken_tools = brokenTools;
        response.message = `Unreal Editor connected. ${brokenTools.length} tool(s) have issues.`;
      }

      return {
        content: [
          {
            type: "text",
            text: JSON.stringify(response, null, 2),
          },
        ],
      };
    } else {
      return {
        content: [
          {
            type: "text",
            text: JSON.stringify({
              connected: false,
              reason: status.reason,
              message: "Unreal Editor is not running or UnrealClaude plugin is not enabled. Please start Unreal Editor with the plugin.",
            }, null, 2),
          },
        ],
        isError: true,
      };
    }
  }

  // Strip "unreal_" prefix to get actual tool name
  if (!name.startsWith("unreal_")) {
    return {
      content: [
        {
          type: "text",
          text: `Unknown tool: ${name}`,
        },
      ],
      isError: true,
    };
  }

  const toolName = name.substring(7); // Remove "unreal_" prefix
  const result = await executeUnrealTool(toolName, args);

  // Build response text
  let responseText = result.success
    ? result.message + (result.data ? "\n\n" + JSON.stringify(result.data, null, 2) : "")
    : `Error: ${result.message}`;

  // Optionally inject relevant UE context based on tool name
  if (CONFIG.injectContext && result.success) {
    const context = getContextForTool(toolName);
    if (context) {
      responseText += `\n\n---\n\n## Relevant UE 5.7 API Context\n\n${context}`;
      log.debug("Injected context for tool", { tool: toolName });
    }
  }

  // Build response with both text content and structured data
  const response = {
    content: [
      {
        type: "text",
        text: responseText,
      },
    ],
    isError: !result.success,
  };

  // Add structured content for programmatic access (if data exists)
  if (result.data) {
    response.structuredContent = {
      success: result.success,
      message: result.message,
      data: result.data,
    };
  }

  return response;
});

// Start the server
async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);

  // Verify context system is working
  const categories = listCategories();
  const testContext = loadContextForCategory("animation");
  const contextStatus = testContext ? `OK (${categories.length} categories loaded)` : "FAILED";

  log.info("UnrealClaude MCP Bridge started", {
    version: "1.2.0",
    unrealUrl: CONFIG.unrealMcpUrl,
    timeoutMs: CONFIG.requestTimeoutMs,
    contextInjection: CONFIG.injectContext,
    contextSystem: contextStatus,
    contextCategories: categories,
  });
}

main().catch((error) => {
  log.error("Fatal error", { error: error.message, stack: error.stack });
  process.exit(1);
});
