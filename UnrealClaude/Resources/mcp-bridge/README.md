# UnrealClaude MCP Bridge

This bridges Claude Code to the UnrealClaude plugin's HTTP server, allowing Claude to directly manipulate the Unreal Editor.

## Setup

### 1. Install dependencies

```bash
cd Resources/mcp-bridge
npm install
```

### 2. Add to Claude Code settings

Add to your `~/.claude/settings.json` or project `.claude/settings.json`:

```json
{
  "mcpServers": {
    "unrealclaude": {
      "command": "node",
      "args": ["/path/to/UnrealClaude/Resources/mcp-bridge/index.js"],
      "env": {
        "UNREAL_MCP_URL": "http://localhost:3000"
      }
    }
  }
}
```

### 3. Start Unreal Editor

The plugin must be running for the MCP bridge to work. When Unreal Editor starts with the UnrealClaude plugin, it automatically starts an HTTP server on port 3000.

---

## Available Tools

### Connection & Context Tools

| Tool | Description |
|------|-------------|
| `unreal_status` | Check connection to Unreal Editor |
| `unreal_get_ue_context` | Get UE 5.7 API documentation by category or query |

### Level & Actor Tools

| Tool | Description |
|------|-------------|
| `unreal_spawn_actor` | Spawn actors in the level |
| `unreal_get_level_actors` | List actors in the current level |
| `unreal_set_property` | Set properties on actors |
| `unreal_move_actor` | Move/rotate/scale actors |
| `unreal_delete_actors` | Delete actors from the level |
| `unreal_run_console_command` | Run Unreal console commands |
| `unreal_get_output_log` | Get recent output log entries |

### Script Execution Tools

| Tool | Description |
|------|-------------|
| `unreal_execute_script` | Execute C++, Python, or console scripts |
| `unreal_cleanup_scripts` | Remove generated scripts |
| `unreal_get_script_history` | Get script execution history |

### Viewport Tools

| Tool | Description |
|------|-------------|
| `unreal_capture_viewport` | Capture screenshot of active viewport |

### Blueprint Tools

| Tool | Description |
|------|-------------|
| `unreal_blueprint_query` | Query Blueprint information (list, inspect, get_graph) |
| `unreal_blueprint_modify` | Modify Blueprints (create, add variables/functions, add nodes, connect pins) |

### Animation Blueprint Tools

| Tool | Description |
|------|-------------|
| `unreal_anim_blueprint_modify` | Comprehensive Animation Blueprint manipulation |

---

## Animation Blueprint Operations

The `unreal_anim_blueprint_modify` tool provides full control over Animation Blueprints:

### State Machine Operations

| Operation | Description |
|-----------|-------------|
| `get_info` | Get AnimBlueprint structure overview |
| `get_state_machine` | Get detailed state machine info |
| `create_state_machine` | Create new state machine |
| `add_state` | Add state to state machine |
| `remove_state` | Remove state from state machine |
| `set_entry_state` | Set entry state for state machine |
| `connect_state_machine_to_output` | Connect state machine to AnimGraph output pose |

### Transition Operations

| Operation | Description |
|-----------|-------------|
| `add_transition` | Create transition between states |
| `remove_transition` | Remove transition |
| `set_transition_duration` | Set blend duration |
| `set_transition_priority` | Set evaluation priority |

### Transition Condition Graph

| Operation | Description |
|-----------|-------------|
| `add_condition_node` | Add logic node (TimeRemaining, Greater, Less, And, Or, Not, GetVariable) |
| `delete_condition_node` | Remove node from transition graph |
| `connect_condition_nodes` | Connect nodes in transition graph |
| `connect_to_result` | Connect condition to transition result |

### Node/Pin Introspection (NEW)

| Operation | Description |
|-----------|-------------|
| `get_transition_nodes` | List all nodes in transition graph(s) with their pins |
| `inspect_node_pins` | Get detailed pin info (types, connections, default values) |
| `set_pin_default_value` | Set pin default value with type validation |
| `add_comparison_chain` | Add GetVariable -> Comparison -> Result chain (auto-ANDs with existing) |
| `validate_blueprint` | Return compile status and diagnostics |

### Animation Assignment

| Operation | Description |
|-----------|-------------|
| `set_state_animation` | Assign AnimSequence, BlendSpace, BlendSpace1D, or Montage |
| `find_animations` | Search compatible animation assets |

### Batch Operations

| Operation | Description |
|-----------|-------------|
| `batch` | Execute multiple operations atomically |

---

## Usage Examples

### Basic Actor Manipulation

```
"Spawn a point light at position 0, 0, 500"
"List all StaticMeshActors in the level"
"Move the player start to coordinates 100, 200, 0"
"Delete all actors named 'Cube'"
"Set the intensity of PointLight_0 to 5000"
```

### Blueprint Operations

```
"Create a new Actor Blueprint called BP_Enemy"
"Add a float variable called Health to BP_Player"
"List all Blueprints in /Game/Blueprints/"
```

### Animation Blueprint Operations

```
"Get info about the Animation Blueprint at /Game/Characters/ABP_Character"
"Create a state machine called Locomotion in ABP_Character"
"Add states Idle, Walk, and Run to the Locomotion state machine"
"Add a transition from Idle to Walk"
"Set the transition condition: Speed > 0"
```

### Animation Blueprint JSON Examples

**Get all transition nodes:**
```json
{
  "blueprint_path": "/Game/ABP_Character",
  "operation": "get_transition_nodes",
  "state_machine": "Locomotion"
}
```

**Inspect node pins:**
```json
{
  "blueprint_path": "/Game/ABP_Character",
  "operation": "inspect_node_pins",
  "state_machine": "Locomotion",
  "from_state": "Idle",
  "to_state": "Walk",
  "node_id": "Node_Greater_123"
}
```

**Set pin default value:**
```json
{
  "blueprint_path": "/Game/ABP_Character",
  "operation": "set_pin_default_value",
  "state_machine": "Locomotion",
  "from_state": "Idle",
  "to_state": "Walk",
  "node_id": "Node_Greater_123",
  "pin_name": "B",
  "pin_value": "0.1"
}
```

**Add comparison chain (auto-ANDs with existing):**
```json
{
  "blueprint_path": "/Game/ABP_Character",
  "operation": "add_comparison_chain",
  "state_machine": "Locomotion",
  "from_state": "Idle",
  "to_state": "Walk",
  "variable_name": "Speed",
  "comparison_type": "Greater",
  "compare_value": "0.0"
}
```

**Validate blueprint:**
```json
{
  "blueprint_path": "/Game/ABP_Character",
  "operation": "validate_blueprint"
}
```

**Batch operations:**
```json
{
  "blueprint_path": "/Game/ABP_Character",
  "operation": "batch",
  "operations": [
    {"operation": "add_state", "state_machine": "Locomotion", "state_name": "Idle", "is_entry_state": true},
    {"operation": "add_state", "state_machine": "Locomotion", "state_name": "Walk"},
    {"operation": "add_transition", "state_machine": "Locomotion", "from_state": "Idle", "to_state": "Walk"},
    {"operation": "add_comparison_chain", "state_machine": "Locomotion", "from_state": "Idle", "to_state": "Walk", "variable_name": "Speed", "comparison_type": "Greater", "compare_value": "0.0"}
  ]
}
```

---

## Supported Condition Node Types

| Type | Description | Inputs | Output |
|------|-------------|--------|--------|
| `TimeRemaining` | Time remaining in current animation | - | float |
| `Greater` | A > B | A (float), B (float) | bool |
| `Less` | A < B | A (float), B (float) | bool |
| `GreaterEqual` | A >= B | A (float), B (float) | bool |
| `LessEqual` | A <= B | A (float), B (float) | bool |
| `Equal` | A == B | A (float), B (float) | bool |
| `NotEqual` | A != B | A (float), B (float) | bool |
| `And` | A AND B | A (bool), B (bool) | bool |
| `Or` | A OR B | A (bool), B (bool) | bool |
| `Not` | NOT A | A (bool) | bool |
| `GetVariable` | Get blueprint variable | node_params: {variable_name} | varies |

---

## Troubleshooting

**Tools show "NOT CONNECTED"**
- Make sure Unreal Editor is running
- Verify the UnrealClaude plugin is enabled
- Check the Output Log in Unreal for "MCP Server started on http://localhost:3000"

**Port conflict**
- If port 3000 is in use, the plugin will fail to start the MCP server
- Close other applications using port 3000, or modify the port in the plugin source

**Missing node_modules on fresh install**
- Run `npm install` in the mcp-bridge folder
- Or copy node_modules from a working installation

**stdin/stdout errors**
- Ensure Node.js is installed and in PATH
- Verify all dependencies are installed (`npm install`)
- Check that the path in settings.json points to the correct index.js location

---

## Dynamic UE 5.7 Context System

The bridge includes a dynamic context loader that provides accurate UE 5.7 API documentation on demand.

### Available Context Categories

| Category | Description |
|----------|-------------|
| `animation` | Animation Blueprint, state machines, transitions, UAnimInstance |
| `blueprint` | Blueprint graphs, UK2Node, FBlueprintEditorUtils |
| `slate` | Slate UI widgets, SNew/SAssignNew, layout patterns |
| `actor` | Actor spawning, components, transforms, iteration |
| `assets` | Asset loading, TSoftObjectPtr, FStreamableManager, async loading |
| `replication` | Network replication, RPCs, DOREPLIFETIME, OnRep |

### Using the Context Tool

**Get specific category:**
```json
{
  "category": "animation"
}
```

**Search by query:**
```json
{
  "query": "state machine transitions"
}
```

**List available categories:**
```json
{}
```

### Automatic Context Injection

Enable automatic context injection by setting the environment variable:

```json
{
  "mcpServers": {
    "unrealclaude": {
      "command": "node",
      "args": ["/path/to/mcp-bridge/index.js"],
      "env": {
        "UNREAL_MCP_URL": "http://localhost:3000",
        "INJECT_CONTEXT": "true"
      }
    }
  }
}
```

When enabled, relevant UE 5.7 context will be automatically appended to tool responses based on the tool being used.
