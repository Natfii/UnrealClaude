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
 * Sleep for a given number of milliseconds.
 * @param {number} ms - milliseconds to sleep
 */
export function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

/**
 * Execute a tool via Unreal's async task queue (task_submit → poll task_status → task_result).
 * Falls back to synchronous executeUnrealTool() if task_submit fails.
 *
 * @param {string} baseUrl - Unreal MCP server base URL
 * @param {number} timeoutMs - per-request HTTP timeout in milliseconds
 * @param {string} toolName - name of the tool to execute
 * @param {object} args - tool arguments
 * @param {object} [options]
 * @param {function} [options.onProgress] - callback({progress, total, message})
 * @param {number}   [options.pollIntervalMs=2000] - poll interval
 * @param {number}   [options.asyncTimeoutMs=300000] - overall async timeout (5 min)
 */
export async function executeUnrealToolAsync(baseUrl, timeoutMs, toolName, args, options = {}) {
  const {
    onProgress,
    pollIntervalMs = 2000,
    asyncTimeoutMs = 300000,
  } = options;

  // Step 1: Submit task
  let taskId;
  try {
    const submitResponse = await fetchWithTimeout(`${baseUrl}/mcp/tool/task_submit`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        tool_name: toolName,
        params: args || {},
        timeout_ms: asyncTimeoutMs,
      }),
    }, timeoutMs);

    const submitData = await submitResponse.json();
    if (!submitData.success || !submitData.data?.task_id) {
      log.debug("task_submit failed or no task_id, falling back to sync", { tool: toolName });
      return executeUnrealTool(baseUrl, timeoutMs, toolName, args);
    }
    taskId = submitData.data.task_id;
    log.debug("Task submitted", { tool: toolName, taskId });
  } catch (error) {
    log.debug("task_submit error, falling back to sync", { tool: toolName, error: error.message });
    return executeUnrealTool(baseUrl, timeoutMs, toolName, args);
  }

  // Step 2: Poll for completion
  const deadline = Date.now() + asyncTimeoutMs;
  let pollCount = 0;

  while (Date.now() < deadline) {
    await sleep(pollIntervalMs);
    pollCount++;

    let statusData;
    try {
      const statusResponse = await fetchWithTimeout(`${baseUrl}/mcp/tool/task_status`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ task_id: taskId }),
      }, timeoutMs);
      statusData = await statusResponse.json();
    } catch (error) {
      log.error("task_status poll failed", { taskId, error: error.message });
      continue;
    }

    const taskStatus = statusData.data?.status || statusData.status;
    const progress = statusData.data?.progress ?? pollCount;
    const total = statusData.data?.total ?? 0;
    const progressMessage = statusData.data?.progress_message || `Polling... (${pollCount})`;

    // Send progress notification
    if (onProgress) {
      onProgress({ progress, total, message: progressMessage });
    }

    // Check for terminal states
    if (taskStatus === "completed" || taskStatus === "failed" || taskStatus === "cancelled") {
      // Step 3: Retrieve result
      try {
        const resultResponse = await fetchWithTimeout(`${baseUrl}/mcp/tool/task_result`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ task_id: taskId }),
        }, timeoutMs);
        const resultData = await resultResponse.json();
        log.debug("Task completed", { tool: toolName, taskId, status: taskStatus });
        return resultData;
      } catch (error) {
        log.error("task_result fetch failed", { taskId, error: error.message });
        return {
          success: false,
          message: `Task ${taskStatus} but failed to retrieve result: ${error.message}`,
        };
      }
    }
  }

  // Async timeout exceeded
  return {
    success: false,
    message: `Task timed out after ${asyncTimeoutMs}ms (task_id: ${taskId})`,
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
