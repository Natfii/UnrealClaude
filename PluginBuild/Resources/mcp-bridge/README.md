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
      "args": ["C:/Users/Natal/Documents/GitHub/UnrealClaude/UnrealClaude/Resources/mcp-bridge/index.js"],
      "env": {
        "UNREAL_MCP_URL": "http://localhost:3000"
      }
    }
  }
}
```

### 3. Start Unreal Editor

The plugin must be running for the MCP bridge to work. When Unreal Editor starts with the UnrealClaude plugin, it automatically starts an HTTP server on port 3000.

## Available Tools

Once connected, Claude Code will have access to these Unreal Editor tools:

| Tool | Description |
|------|-------------|
| `unreal_status` | Check connection to Unreal Editor |
| `unreal_spawn_actor` | Spawn actors in the level |
| `unreal_get_level_actors` | List actors in the current level |
| `unreal_set_property` | Set properties on actors |
| `unreal_move_actor` | Move/rotate/scale actors |
| `unreal_delete_actors` | Delete actors from the level |
| `unreal_run_console_command` | Run Unreal console commands |

## Usage Examples

Once configured, you can ask Claude:

- "Spawn a point light at position 0, 0, 500"
- "List all StaticMeshActors in the level"
- "Move the player start to coordinates 100, 200, 0"
- "Delete all actors named 'Cube'"
- "Set the intensity of PointLight_0 to 5000"

## Troubleshooting

**Tools show "NOT CONNECTED"**
- Make sure Unreal Editor is running
- Verify the UnrealClaude plugin is enabled
- Check the Output Log in Unreal for "MCP Server started on http://localhost:3000"

**Port conflict**
- If port 3000 is in use, the plugin will fail to start the MCP server
- Close other applications using port 3000, or modify the port in the plugin source
