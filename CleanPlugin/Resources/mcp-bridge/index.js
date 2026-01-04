#!/usr/bin/env node

/**
 * UnrealClaude MCP Bridge
 *
 * This bridges Claude Code's MCP protocol to the UnrealClaude plugin's HTTP REST API.
 * The plugin runs an HTTP server on localhost:3000 with editor manipulation tools.
 */

import { Server } from "@modelcontextprotocol/sdk/server/index.js";
import { StdioServerTransport } from "@modelcontextprotocol/sdk/server/stdio.js";
import {
  CallToolRequestSchema,
  ListToolsRequestSchema,
} from "@modelcontextprotocol/sdk/types.js";

const UNREAL_MCP_URL = process.env.UNREAL_MCP_URL || "http://localhost:3000";

/**
 * Fetch tools from the UnrealClaude HTTP server
 */
async function fetchUnrealTools() {
  try {
    const response = await fetch(`${UNREAL_MCP_URL}/mcp/tools`);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    return data.tools || [];
  } catch (error) {
    console.error("Failed to fetch tools from Unreal:", error.message);
    return [];
  }
}

/**
 * Execute a tool via the UnrealClaude HTTP server
 */
async function executeUnrealTool(toolName, args) {
  try {
    const response = await fetch(`${UNREAL_MCP_URL}/mcp/tool/${toolName}`, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(args || {}),
    });

    const data = await response.json();
    return data;
  } catch (error) {
    return {
      success: false,
      message: `Failed to execute tool: ${error.message}`,
    };
  }
}

/**
 * Check if Unreal Editor is running with the plugin
 */
async function checkUnrealConnection() {
  try {
    const response = await fetch(`${UNREAL_MCP_URL}/mcp/status`);
    if (response.ok) {
      const data = await response.json();
      return { connected: true, ...data };
    }
    return { connected: false };
  } catch {
    return { connected: false };
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

    if (param.default) {
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
    version: "1.0.0",
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
  console.error("UnrealClaude MCP Bridge started");
  console.error(`Connecting to Unreal at: ${UNREAL_MCP_URL}`);
}

main().catch((error) => {
  console.error("Fatal error:", error);
  process.exit(1);
});
