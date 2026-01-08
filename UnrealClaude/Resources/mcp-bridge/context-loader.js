/**
 * UnrealClaude Dynamic Context Loader
 *
 * Loads UE 5.7 context files based on:
 * 1. Tool names (automatic injection)
 * 2. Query keywords (explicit request)
 *
 * Context files are stored in ./contexts/*.md
 */

import { readFileSync, existsSync } from "fs";
import { join, dirname } from "path";
import { fileURLToPath } from "url";

const __dirname = dirname(fileURLToPath(import.meta.url));
const CONTEXTS_DIR = join(__dirname, "contexts");

/**
 * Context configuration
 * Maps categories to files and matching patterns
 */
const CONTEXT_CONFIG = {
  animation: {
    files: ["animation.md"],
    // Tool name patterns that trigger this context
    toolPatterns: [/^anim/, /animation/, /state_machine/],
    // Keywords in user queries that trigger this context
    keywords: [
      "animation",
      "anim",
      "state machine",
      "blend",
      "transition",
      "animinstance",
      "montage",
      "sequence",
      "blendspace",
    ],
  },
  blueprint: {
    files: ["blueprint.md"],
    toolPatterns: [/^blueprint/, /^bp_/],
    keywords: [
      "blueprint",
      "graph",
      "node",
      "pin",
      "uk2node",
      "variable",
      "function",
      "event graph",
    ],
  },
  slate: {
    files: ["slate.md"],
    toolPatterns: [/widget/, /editor.*ui/, /slate/],
    keywords: [
      "slate",
      "widget",
      "snew",
      "sassign",
      "ui",
      "editor window",
      "tab",
      "panel",
      "sverticalbox",
      "shorizontalbox",
    ],
  },
  actor: {
    files: ["actor.md"],
    toolPatterns: [/spawn/, /actor/, /move/, /delete/, /level/],
    keywords: [
      "actor",
      "spawn",
      "component",
      "transform",
      "location",
      "rotation",
      "attach",
      "destroy",
      "iterate",
    ],
  },
  assets: {
    files: ["assets.md"],
    toolPatterns: [/asset/, /load/, /reference/],
    keywords: [
      "asset",
      "load",
      "soft pointer",
      "tsoftobjectptr",
      "async",
      "stream",
      "reference",
      "registry",
      "tobjectptr",
    ],
  },
  replication: {
    files: ["replication.md"],
    toolPatterns: [/replicate/, /network/, /rpc/],
    keywords: [
      "replicate",
      "replication",
      "network",
      "rpc",
      "server",
      "client",
      "multicast",
      "onrep",
      "doreplifetime",
      "authority",
    ],
  },
};

// Cache for loaded context files
const contextCache = new Map();

/**
 * Load a context file from disk (with caching)
 */
function loadContextFile(filename) {
  if (contextCache.has(filename)) {
    return contextCache.get(filename);
  }

  const filepath = join(CONTEXTS_DIR, filename);
  if (!existsSync(filepath)) {
    console.error(`[ContextLoader] Context file not found: ${filepath}`);
    return null;
  }

  try {
    const content = readFileSync(filepath, "utf-8");
    contextCache.set(filename, content);
    return content;
  } catch (error) {
    console.error(`[ContextLoader] Error loading ${filename}:`, error.message);
    return null;
  }
}

/**
 * Get context category from tool name
 * @param {string} toolName - The MCP tool name (without unreal_ prefix)
 * @returns {string|null} - Category name or null
 */
export function getCategoryFromTool(toolName) {
  const lowerName = toolName.toLowerCase();

  for (const [category, config] of Object.entries(CONTEXT_CONFIG)) {
    for (const pattern of config.toolPatterns) {
      if (pattern.test(lowerName)) {
        return category;
      }
    }
  }

  return null;
}

/**
 * Get context categories from a query string
 * @param {string} query - User query or search string
 * @returns {string[]} - Array of matching category names
 */
export function getCategoriesFromQuery(query) {
  const lowerQuery = query.toLowerCase();
  const matches = [];

  for (const [category, config] of Object.entries(CONTEXT_CONFIG)) {
    for (const keyword of config.keywords) {
      if (lowerQuery.includes(keyword.toLowerCase())) {
        if (!matches.includes(category)) {
          matches.push(category);
        }
        break; // One match per category is enough
      }
    }
  }

  return matches;
}

/**
 * Load context content for a specific category
 * @param {string} category - Category name (animation, blueprint, etc.)
 * @returns {string|null} - Combined context content or null
 */
export function loadContextForCategory(category) {
  const config = CONTEXT_CONFIG[category];
  if (!config) {
    return null;
  }

  const contents = [];
  for (const file of config.files) {
    const content = loadContextFile(file);
    if (content) {
      contents.push(content);
    }
  }

  return contents.length > 0 ? contents.join("\n\n---\n\n") : null;
}

/**
 * Load context for a tool call (automatic injection)
 * @param {string} toolName - Tool name (without unreal_ prefix)
 * @returns {string|null} - Context to inject or null
 */
export function getContextForTool(toolName) {
  const category = getCategoryFromTool(toolName);
  if (!category) {
    return null;
  }

  return loadContextForCategory(category);
}

/**
 * Load context based on a query (explicit request)
 * @param {string} query - User query
 * @returns {{ categories: string[], content: string }|null} - Matched contexts
 */
export function getContextForQuery(query) {
  const categories = getCategoriesFromQuery(query);
  if (categories.length === 0) {
    return null;
  }

  const contents = [];
  for (const category of categories) {
    const content = loadContextForCategory(category);
    if (content) {
      contents.push(content);
    }
  }

  return {
    categories,
    content: contents.join("\n\n---\n\n"),
  };
}

/**
 * List all available context categories
 * @returns {string[]} - Array of category names
 */
export function listCategories() {
  return Object.keys(CONTEXT_CONFIG);
}

/**
 * Get metadata about a category
 * @param {string} category - Category name
 * @returns {object|null} - Category metadata
 */
export function getCategoryInfo(category) {
  const config = CONTEXT_CONFIG[category];
  if (!config) {
    return null;
  }

  return {
    name: category,
    files: config.files,
    keywords: config.keywords,
    toolPatterns: config.toolPatterns.map((p) => p.toString()),
  };
}

/**
 * Clear the context cache (useful for hot-reloading)
 */
export function clearCache() {
  contextCache.clear();
}

export default {
  getCategoryFromTool,
  getCategoriesFromQuery,
  loadContextForCategory,
  getContextForTool,
  getContextForQuery,
  listCategories,
  getCategoryInfo,
  clearCache,
};
