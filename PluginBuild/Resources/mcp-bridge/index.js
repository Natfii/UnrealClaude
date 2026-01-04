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
 */

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";

// Configuration with defaults
const CONFIG = {
  unrealMcpUrl: process.env.UNREAL_MCP_URL || "http://localhost:3000",
  requestTimeoutMs: parseInt(process.env.MCP_REQUEST_TIMEOUT_MS, 10) || 30000,
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

// Create the MCP server
const server = new Server(
  {
    name: "unrealclaude",
    version: "1.1.0",
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

  // Convert to MCP format
  const mcpTools = unrealTools.map((tool) => ({
    name: `unreal_${tool.name}`,
    description: `[Unreal Editor] ${tool.description}`,
    inputSchema: convertToMCPSchema(tool.parameters),
  }));

  // Add status tool
  mcpTools.unshift({
    name: "unreal_status",
    description: `Check Unreal Editor connection status. Currently: CONNECTED to ${status.projectName || "Unknown Project"} (${status.engineVersion || "Unknown"})`,
    inputSchema: {
      type: "object",
      properties: {},
    },
  });

  log.info("Tools listed", { count: mcpTools.length, connected: true });
  return { tools: mcpTools };
});

// Handle call_tool request
server.setRequestHandler(CallToolRequestSchema, async (request) => {
  const { name, arguments: args } = request.params;

  // Handle status check
  if (name === "unreal_status") {
    const status = await checkUnrealConnection();
    if (status.connected) {
      return {
        content: [
          {
            type: "text",
            text: JSON.stringify({
              connected: true,
              project: status.projectName,
              engine: status.engineVersion,
              tools: status.toolCount,
              message: "Unreal Editor is connected and ready for commands.",
            }, null, 2),
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

  return {
    content: [
      {
        type: "text",
        text: JSON.stringify(result, null, 2),
      },
    ],
    isError: !result.success,
  };
});

// Start the server
async function main() {
  const transport = new StdioServerTransport();
  await server.connect(transport);
  log.info("UnrealClaude MCP Bridge started", {
    version: "1.1.0",
    unrealUrl: CONFIG.unrealMcpUrl,
    timeoutMs: CONFIG.requestTimeoutMs
  });
}

main().catch((error) => {
  log.error("Fatal error", { error: error.message, stack: error.stack });
  process.exit(1);
});
