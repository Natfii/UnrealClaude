# UnrealClaude

**Claude Code CLI integration for Unreal Engine 5.7** - Get AI coding assistance with built-in UE5.7 documentation context directly in the editor.

> **Windows Only** - This plugin uses Windows-specific process APIs to execute the Claude Code CLI.

## Overview

UnrealClaude integrates the [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) directly into the Unreal Engine 5.7 Editor. Instead of using the API directly, this plugin shells out to the `claude` command-line tool, leveraging your existing Claude Code authentication and capabilities.

<img width="507.5" height="443.5" alt="Screenshot 2026-01-03 202730" src="https://github.com/user-attachments/assets/abe11687-67e0-4e7e-a626-47f3df178cf9" />

**Key Features:**
- **Native Editor Integration** - Chat panel docked in your editor
- **UE5.7 Context** - System prompts optimized for Unreal Engine 5.7 development
- **MCP Server** - Model Context Protocol server for external tool integration
- **Blueprint Editing** - Create and modify Blueprints, Animation Blueprints, state machines (Few bugs still, don't rely on fully)
- **Asset Management** - Search assets, query dependencies and referencers
- **Async Task Queue** - Long-running operations won't timeout (WIP)
- **Script Execution** - Claude can write, compile (via Live Coding), and execute scripts with your permission
- **Session Persistence** - Conversation history saved across editor sessions
- **Project-Aware** - Automatically gathers project context (modules, plugins, assets) and is able to see editor viewports
- **Uses Claude Code Auth** - No separate API key management needed

## Prerequisites

### 1. Install Claude Code CLI

```bash
npm install -g @anthropic-ai/claude-code
```

### 2. Authenticate Claude Code

```bash
claude auth login
```

This will open a browser window to authenticate with your Anthropic account (Claude Pro/Max subscription) or set up API access.

### 3. Verify Installation

```bash
claude --version
claude -p "Hello, can you see me?"
```

## Installation

### Option A: Copy to Project Plugins (Recommended)

Prebuilt binaries for **UE 5.7 Win64** are included - no compilation required.

1. Download or clone this repository
2. Copy the `UnrealClaude` folder to your project's `Plugins` directory:
   ```
   YourProject/
   ├── Content/
   ├── Source/
   └── Plugins/
       └── UnrealClaude/
           ├── Binaries/Win64/    # Prebuilt binaries
           ├── Source/
           ├── Resources/
           ├── Config/
           └── UnrealClaude.uplugin
   ```
3. **Install MCP Bridge dependencies** (required for Blueprint tools and editor integration):
   ```bash
   cd YourProject/Plugins/UnrealClaude/Resources/mcp-bridge
   npm install
   ```
4. Launch the editor - the plugin will load automatically

### Option B: Engine Plugin (All Projects)

Copy to your engine's plugins folder:
```
C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace\UnrealClaude\
```

Then install the MCP bridge dependencies:
```bash
cd "C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace\UnrealClaude\Resources\mcp-bridge"
npm install
```

### Building from Source

If you need to rebuild (different UE version, modifications, etc.):
```bash
# From UE installation directory
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="PATH\TO\UnrealClaude.uplugin" -Package="OUTPUT\PATH" -TargetPlatforms=Win64
```

## Usage

### Opening the Claude Panel

 Menu → Tools → Claude Assistant

<img width="580" height="340" alt="{778C8E0B-C354-4AD1-BBFF-B514A4D5FC16}" src="https://github.com/user-attachments/assets/2087ef40-9791-4ad9-933b-2c64370344e8" />


### Example Prompts

```
How do I create a custom Actor Component in C++?

What's the best way to implement a health system using GAS?

Explain World Partition and how to set up streaming for an open world.

Write a BlueprintCallable function that spawns particles at a location.

How do I properly use TObjectPtr<> vs raw pointers in UE5.7?
```

### Input Shortcuts

| Shortcut | Action |
|----------|--------|
| `Enter` | Send message |
| `Shift+Enter` | New line in input |
| `Escape` | Cancel current request |

## Features

### Session Persistence

Conversations are automatically saved to your project's `Saved/UnrealClaude/` directory and restored when you reopen the editor. The plugin maintains conversation context across sessions.

### Project Context

UnrealClaude automatically gathers information about your project:
- Source modules and their dependencies
- Enabled plugins
- Project settings
- Recent assets
- Custom CLAUDE.md instructions

### MCP Server

The plugin includes a Model Context Protocol (MCP) server that exposes editor functionality to external tools. Available MCP tools:

#### Actor Tools

| Tool | Description |
|------|-------------|
| `get_level_actors` | List all actors in the current level |
| `spawn_actor` | Spawn a new actor by class |
| `delete_actors` | Delete actors by name |
| `move_actor` | Move/rotate/scale actors |
| `set_property` | Set actor properties |

#### Blueprint Tools

| Tool | Description |
|------|-------------|
| `blueprint_query` | Query Blueprint information (list, inspect, get_graph) |
| `blueprint_modify` | Modify Blueprints (create, add/remove variables and functions, add/delete nodes, connect/disconnect pins, set pin values) |

The `blueprint_modify` tool supports:
- **Level 2**: Create Blueprints, add/remove variables and functions
- **Level 3**: Add single or batch nodes to graphs
- **Level 4**: Connect/disconnect pins, set default pin values

All modifications auto-compile the Blueprint after changes and return detailed compile results including errors and warnings.

#### Animation Blueprint Tools

| Tool | Description |
|------|-------------|
| `anim_blueprint_modify` | Modify Animation Blueprints (state machines, states, transitions, conditions, anim nodes) |

The `anim_blueprint_modify` tool supports comprehensive state machine editing:

**State Machine Operations:**
- `create_state_machine` - Create new state machine in an Animation Blueprint
- `get_state_machine` - Get detailed state machine info
- `get_info` - Get Animation Blueprint structure overview
- `set_entry_state` - Set the default entry state

**State Operations:**
- `add_state` - Create a new state in a state machine
- `remove_state` - Delete a state

**Transition Operations:**
- `add_transition` - Create a transition between two states
- `remove_transition` - Delete a transition
- `set_transition_duration` - Set blend duration
- `set_transition_priority` - Set evaluation priority

**Transition Condition Operations:**
- `add_condition_node` - Add logic nodes (TimeRemaining, Greater, Less, And, Or, Not, GetVariable)
- `delete_condition_node` - Remove condition node
- `connect_condition_nodes` - Connect condition nodes together
- `connect_to_result` - Connect condition output to transition result
- `add_comparison_chain` - Add GetVariable → Comparison → Result chain (auto-ANDs with existing)
  - Supports Float, Integer, Boolean, Byte, and Enum variable types
  - Auto-detects variable type and creates appropriate comparison node

**Bulk Transition Conditions (NEW):**
- `setup_transition_conditions` - Set up conditions for multiple transitions using pattern matching

The bulk operation supports flexible pattern matching:
- **Exact match**: `"from": "Idle"` matches only "Idle"
- **Wildcard**: `"*"` matches any, `"Attack_*"` matches Attack_1, Attack_2, etc.
- **Regex**: `"^Attack_\\d+$"` for advanced patterns
- **Array list**: `["Idle", "Walk", "Run"]` matches any listed state

**Example - Setup conditions for multiple transitions:**
```json
{
  "operation": "setup_transition_conditions",
  "blueprint_path": "/Game/Characters/ABP_Character",
  "state_machine": "Locomotion",
  "rules": [
    {
      "match": { "from": "*", "to": "Walk" },
      "conditions": [
        {"variable": "Speed", "comparison": "Greater", "value": "10"}
      ],
      "logic": "AND"
    },
    {
      "match": { "from": "Attack_*", "to": "*" },
      "conditions": [
        {"type": "TimeRemaining", "comparison": "Less", "value": "0.1"}
      ]
    }
  ]
}
```

**Batch Operations:**
Use `operation: "batch"` with an `operations` array to execute multiple operations with a single compile:
- Supports: `add_state`, `remove_state`, `add_transition`, `remove_transition`, `set_transition_duration`, `set_state_animation`, `add_comparison_chain`, `add_condition_node`, `connect_condition_nodes`, `connect_to_result`

**Animation Assignment:**
- `set_state_animation` - Assign AnimSequence, BlendSpace, BlendSpace1D, or Montage to states
- `find_animations` - Search for compatible animation assets
- `connect_state_machine_to_output` - Connect State Machine to AnimGraph Output Pose

**Introspection & Visualization:**
- `get_transition_nodes` - List all nodes in transition graph(s) with pins
- `inspect_node_pins` - Get detailed pin info for a node
- `set_pin_default_value` - Set pin default value with type validation
- `validate_blueprint` - Return compile errors with full diagnostics
- `get_state_machine_diagram` - Generate ASCII diagram and enhanced JSON for state machine visualization

**Smart Pin Type Detection:**
- When connecting pins, the system automatically detects type mismatches (e.g., integer variable → float comparison)
- Comparison nodes are automatically recreated with the correct type (Integer, Float, Boolean, Byte, Enum)
- No manual type specification needed - the system infers from connected variable types

All modifications auto-compile the Animation Blueprint after changes and return structured compile results with error details for easy debugging.

#### Asset Tools

| Tool | Description |
|------|-------------|
| `asset_search` | Search for assets by class, path, or name with pagination |
| `asset_dependencies` | Get all assets that an asset depends on |
| `asset_referencers` | Get all assets that reference a specific asset |

The `asset_search` tool supports:
- **Class filtering**: Find all Blueprints, Materials, Textures, etc.
- **Path filtering**: Search within specific content folders
- **Name patterns**: Match assets by substring
- **Pagination**: Handle large result sets with limit/offset

#### Async Task Queue

For long-running operations that might timeout, use the async task system:

| Tool | Description |
|------|-------------|
| `task_submit` | Submit any tool for async background execution |
| `task_status` | Poll task status (pending/running/completed/failed) |
| `task_result` | Get full result of a completed task |
| `task_list` | List all tasks with queue statistics |
| `task_cancel` | Cancel a pending or running task |

**Async Workflow Example:**
```
1. task_submit(tool_name="asset_search", params={class_filter:"Blueprint"}, timeout_ms=300000)
   → Returns: {task_id: "abc123...", status: "pending"}

2. task_status(task_id="abc123...")
   → Returns: {status: "running", progress: 50}

3. task_result(task_id="abc123...")
   → Returns: {success: true, data: {...}}
```

Task queue features:
- **Configurable timeout**: Default 2 minutes, customizable per-task
- **Concurrent execution**: Up to 4 tasks run in parallel
- **Automatic cleanup**: Old results cleared after 5 minutes
- **Cancellation support**: Cancel pending or running tasks

#### Utility Tools

| Tool | Description |
|------|-------------|
| `run_console_command` | Execute Unreal console commands |
| `get_output_log` | Read recent output log entries with optional filtering |
| `capture_viewport` | Capture viewport screenshot |
| `execute_script` | Execute Python/BP scripts with permission |

<img width="707" height="542" alt="{AB6AC101-4A4C-4607-BFB6-187D49F5E65B}" src="https://github.com/user-attachments/assets/e0c2e398-8fcd-4ac6-ade7-d50870215ec1" />


The MCP server runs on port 3000 by default and starts automatically when the editor loads.

## Configuration

### Custom System Prompts

You can extend the built-in UE5.7 context by creating a `CLAUDE.md` file in your project root:

```markdown
# My Project Context

## Architecture
- This is a multiplayer survival game
- Using Dedicated Server model
- GAS for all abilities

## Coding Standards
- Always use UPROPERTY for Blueprint access
- Prefix interfaces with I (IInteractable)
- Use GameplayTags for ability identification
```

### Allowed Tools

By default, the plugin runs Claude with these tools: `Read`, `Write`, `Edit`, `Grep`, `Glob`, `Bash`. You can modify this in `ClaudeSubsystem.cpp`:

```cpp
Config.AllowedTools = { TEXT("Read"), TEXT("Grep"), TEXT("Glob") }; // Read-only
```

## How It Works

1. User enters a prompt in the editor widget
2. Plugin builds context from UE5.7 knowledge + project information
3. Executes: `claude -p --skip-permissions --append-system-prompt "..." "your prompt"`
4. Claude Code runs with your project as the working directory
5. Response is captured and displayed in the chat panel
6. Conversation is persisted for future sessions

### Command Line Equivalent

```bash
cd "C:\YourProject"
claude -p --skip-permissions \
  --allowedTools "Read,Write,Edit,Grep,Glob,Bash" \
  --append-system-prompt "You are an expert Unreal Engine 5.7 developer..." \
  "How do I create a custom GameMode?"
```

## Troubleshooting

### "Claude CLI not found"

1. Verify Claude is installed: `claude --version`
2. Check it's in your PATH: `where claude`
3. Restart Unreal Editor after installation

### "Authentication required"

Run `claude auth login` in a terminal to authenticate.

### Responses are slow

Claude Code executes in your project directory and may read files for context. Large projects may have slower initial responses.

### Plugin doesn't compile

Ensure you're on Unreal Engine 5.7 for Windows. This plugin uses Windows-specific APIs.

### MCP Server not starting

Check if port 3000 is available. The MCP server logs to `LogUnrealClaude`.

### MCP tools not available / Blueprint tools not working

If Claude says the MCP tools are in its instructions but not in its function list:

1. **Install MCP bridge dependencies**: The most common cause is missing npm packages:
   ```bash
   cd YourProject/Plugins/UnrealClaude/Resources/mcp-bridge
   npm install
   ```

2. **Verify the HTTP server is running**: With the editor open, test:
   ```bash
   curl http://localhost:3000/mcp/status
   ```
   You should see a JSON response with project info.

3. **Check the Output Log**: Look for `LogUnrealClaude` messages:
   - `MCP Server started on http://localhost:3000` - Server is running
   - `Registered X MCP tools` - Tools are loaded

4. **Restart the editor**: After installing npm dependencies, restart Unreal Editor.


## Contributing

Feel free to fork for your own needs! Possible areas for improvement:

- [ ] Mac/Linux support
- [ ] Streaming output display
- [ ] Context menu integration (right-click on code)
- [ ] Blueprint node for runtime Claude queries
- [ ] Additional MCP tools

## License

MIT License - See [LICENSE](UnrealClaude/LICENSE) file.

## Credits

- Built for Unreal Engine 5.7
- Integrates with [Claude Code](https://claude.ai/code) by Anthropic
