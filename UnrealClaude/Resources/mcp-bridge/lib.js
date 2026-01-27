/**
 * UE5 MCP Server - Extracted Library Functions
 *
 * Pure/testable functions extracted from index.js.
 * Functions that previously read from the module-scoped CONFIG closure
 * now accept those values as explicit parameters.
 */

/**
 * Structured logging helper - writes to stderr to not interfere with MCP protocol
 */
export const log = {
  info: (msg, data) => console.error(`[INFO] ${msg}`, data ? JSON.stringify(data) : ""),
  error: (msg, data) => console.error(`[ERROR] ${msg}`, data ? JSON.stringify(data) : ""),
  debug: (msg, data) => process.env.DEBUG && console.error(`[DEBUG] ${msg}`, data ? JSON.stringify(data) : ""),
};

/**
 * Fetch with timeout using AbortController
 * @param {string} url - URL to fetch
 * @param {object} options - fetch options
 * @param {number} timeoutMs - timeout in milliseconds
 */
export async function fetchWithTimeout(url, options = {}, timeoutMs = 30000) {
  const controller = new AbortController();
  const timeout = setTimeout(() => controller.abort(), timeoutMs);

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
 * Fetch tools from the Unreal HTTP server
 * @param {string} baseUrl - Unreal MCP server base URL
 * @param {number} timeoutMs - request timeout in milliseconds
 */
export async function fetchUnrealTools(baseUrl, timeoutMs) {
  try {
    const response = await fetchWithTimeout(`${baseUrl}/mcp/tools`, {}, timeoutMs);
    if (!response.ok) {
      throw new Error(`HTTP ${response.status}: ${response.statusText}`);
    }
    const data = await response.json();
    return data.tools || [];
  } catch (error) {
    if (error.name === "AbortError") {
      log.error("Request timeout fetching tools", { url: `${baseUrl}/mcp/tools` });
    } else {
      log.error("Failed to fetch tools from Unreal", { error: error.message });
    }
    return [];
  }
}

/**
 * Execute a tool via the Unreal HTTP server
 * @param {string} baseUrl - Unreal MCP server base URL
 * @param {number} timeoutMs - request timeout in milliseconds
 * @param {string} toolName - name of the tool to execute
 * @param {object} args - tool arguments
 */
export async function executeUnrealTool(baseUrl, timeoutMs, toolName, args) {
  const url = `${baseUrl}/mcp/tool/${toolName}`;
  try {
    const response = await fetchWithTimeout(url, {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify(args || {}),
    }, timeoutMs);

    const data = await response.json();
    log.debug("Tool executed", { tool: toolName, success: data.success });
    return data;
  } catch (error) {
    const errorMessage = error.name === "AbortError"
      ? `Request timeout after ${timeoutMs}ms`
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
 * @param {string} baseUrl - Unreal MCP server base URL
 * @param {number} timeoutMs - request timeout in milliseconds
 */
export async function checkUnrealConnection(baseUrl, timeoutMs) {
  try {
    const response = await fetchWithTimeout(`${baseUrl}/mcp/status`, {}, timeoutMs);
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
 * @param {Array} unrealParams - array of parameter descriptors from Unreal
 */
export function convertToMCPSchema(unrealParams) {
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
 * @param {object} unrealAnnotations - annotation object from Unreal
 */
export function convertAnnotations(unrealAnnotations) {
  if (!unrealAnnotations) {
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
