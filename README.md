# UnrealClaude

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?style=flat&logo=unrealengine&logoColor=white)
![C++](https://img.shields.io/badge/C%2B%2B-20-00599C?style=flat&logo=c%2B%2B&logoColor=white)
![Platform](https://img.shields.io/badge/Platform-Win64-0078D6?style=flat&logo=windows&logoColor=white)
![Claude Code](https://img.shields.io/badge/Claude%20Code-Integration-D97757?style=flat&logo=anthropic&logoColor=white)
![MCP](https://img.shields.io/badge/MCP-20%2B%20Tools-8A2BE2?style=flat)
![License](https://img.shields.io/badge/License-MIT-green?style=flat)

**Claude Code CLI integration for Unreal Engine 5.7** - Get AI coding assistance with built-in UE5.7 documentation context directly in the editor.

> **Windows Only** - This plugin uses Windows-specific process APIs to execute the Claude Code CLI.

## Overview

UnrealClaude integrates the [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) directly into the Unreal Engine 5.7 Editor. Instead of using the API directly, this plugin shells out to the `claude` command-line tool, leveraging your existing Claude Code authentication and capabilities.

<img width="1131" height="1055" alt="{051D8F19-2677-4682-9DDF-A461041C1039}" src="https://github.com/user-attachments/assets/3803aed6-cb2d-4d2a-bac3-dbb1ec3fbf1d" />

**Key Features:**
- **Native Editor Integration** - Chat panel docked in your editor with live streaming responses, tool call grouping, and code block rendering
- **MCP Server** - 20+ Model Context Protocol tools for actor manipulation, Blueprint editing, level management, materials, input, and more
- **Dynamic UE 5.7 Context System** - The MCP bridge includes a dynamic context loader that provides accurate UE 5.7 API documentation on demand
- **Blueprint Editing** - Create and modify Blueprints, Animation Blueprints, state machines (Few bugs still, don't rely on fully)
- **Level Management** - Open, create, and manage levels and map templates programmatically
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

<img width="1210" height="98" alt="{0C6B3595-D641-4F4A-AAEC-1797574A1DD1}" src="https://github.com/user-attachments/assets/71b3eb22-0d5e-4181-9703-f68098f04c8b" />
(Check the Editor catagory in the plugin browser. You might need to scroll down for it if search doesn't pick it up)

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

The plugin includes a Model Context Protocol (MCP) server with 20+ tools that expose editor functionality to Claude and external tools. The MCP server runs on port 3000 by default and starts automatically when the editor loads.

**Tool Categories:**
- **Actor Tools** - Spawn, move, delete, inspect, and set properties on actors
- **Level Management** - Open levels, create new levels from templates, list available templates
- **Blueprint Tools** - Create and modify Blueprints (variables, functions, nodes, pins)
- **Animation Blueprint Tools** - Full state machine editing (states, transitions, conditions, batch operations)
- **Asset Tools** - Search assets, query dependencies and referencers with pagination
- **Character Tools** - Character configuration, movement settings, and data queries
- **Material Tools** - Material and material instance operations
- **Enhanced Input Tools** - Input action and mapping context management
- **Utility Tools** - Console commands, output log, viewport capture, script execution
- **Async Task Queue** - Background execution for long-running operations

<img width="707" height="542" alt="{AB6AC101-4A4C-4607-BFB6-187D49F5E65B}" src="https://github.com/user-attachments/assets/e0c2e398-8fcd-4ac6-ade7-d50870215ec1" />

For full MCP tool documentation with parameters, examples, and API details, see [UnrealClaude's MCP Bridge](https://github.com/Natfii/ue5-mcp-bridge) repository.

#### Dynamic UE 5.7 Context System

The MCP bridge includes a dynamic context loader that provides accurate UE 5.7 API documentation on demand. Use `unreal_get_ue_context` to query by category (animation, blueprint, slate, actor, assets, replication) or search by keywords. Context status is shown in `unreal_status` output.

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

### Debugging the MCP Bridge

The MCP bridge is also available as a [standalone repository](https://github.com/Natfii/ue5-mcp-bridge) with its own Vitest test suite. If you're experiencing bridge-level issues (tool listing, parameter translation, context injection), you can run the bridge tests independently:

```bash
cd path/to/ue5-mcp-bridge
npm install
npm test
```

This tests the bridge without requiring a running Unreal Editor.


## Contributing

Feel free to fork for your own needs! Possible areas for improvement:

- [ ] Mac/Linux support
- [ ] Context menu integration (right-click on code)
- [ ] Blueprint node for runtime Claude queries
- [ ] Additional MCP tools

## License

MIT License - See [LICENSE](UnrealClaude/LICENSE) file.

## Credits

- Built for Unreal Engine 5.7
- Integrates with [Claude Code](https://claude.ai/code) by Anthropic
