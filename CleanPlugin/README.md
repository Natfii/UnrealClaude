# UnrealClaude

**Claude Code CLI integration for Unreal Engine 5.7** - Get AI coding assistance with built-in UE5.7 documentation context directly in the editor.

> **Windows Only** - This plugin uses Windows-specific process APIs to execute the Claude Code CLI.

## Overview

UnrealClaude integrates the [Claude Code CLI](https://docs.anthropic.com/en/docs/claude-code) directly into the Unreal Engine 5.7 Editor. Instead of using the API directly, this plugin shells out to the `claude` command-line tool, leveraging your existing Claude Code authentication and capabilities.

**Key Features:**
- **Native Editor Integration** - Chat panel docked in your editor
- **UE5.7 Context** - System prompts optimized for Unreal Engine 5.7 development
- **MCP Server** - Model Context Protocol server for external tool integration
- **Script Execution** - Claude can write, compile (via Live Coding), and execute scripts with your permission
- **Session Persistence** - Conversation history saved across editor sessions
- **Project-Aware** - Automatically gathers project context (modules, plugins, assets)
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
3. Launch the editor - the plugin will load automatically

### Option B: Engine Plugin (All Projects)

Copy to your engine's plugins folder:
```
C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace\UnrealClaude\
```

### Building from Source

If you need to rebuild (different UE version, modifications, etc.):
```bash
# From UE installation directory
Engine\Build\BatchFiles\RunUAT.bat BuildPlugin -Plugin="PATH\TO\UnrealClaude.uplugin" -Package="OUTPUT\PATH" -TargetPlatforms=Win64
```

## Usage

### Opening the Claude Panel

**Method 1:** Menu → Tools → Claude Assistant

**Method 2:** Click the "Claude" button in the toolbar

### Quick Ask

Use **Tools → Quick Ask Claude** for fast questions without opening the full panel. Responses appear as editor notifications.

### The Chat Interface

- **Input Box**: Type your question and press Enter (or Shift+Enter for newlines)
- **UE5.7 Context**: Toggle to include/exclude Unreal Engine 5.7 context in prompts
- **Project Context**: Toggle to include/exclude project-specific context
- **Clear**: Clear the conversation history
- **Copy Last**: Copy Claude's last response to clipboard

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

| Tool | Description |
|------|-------------|
| `get_level_actors` | List all actors in the current level |
| `spawn_actor` | Spawn a new actor by class |
| `delete_actors` | Delete actors by name |
| `move_actor` | Move/rotate/scale actors |
| `set_property` | Set actor properties |
| `run_console_command` | Execute Unreal console commands |
| `get_output_log` | Read recent output log entries with optional filtering |

The MCP server runs on port 8080 by default. See `.mcp.json` for configuration.

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

Check if port 8080 is available. The MCP server logs to `LogUnrealClaude`.

## Architecture

```
UnrealClaude/
├── Binaries/Win64/                     # Prebuilt binaries (UE 5.7)
│   ├── UnrealEditor-UnrealClaude.dll
│   ├── UnrealEditor-UnrealClaude.pdb
│   └── UnrealEditor.modules
├── Source/UnrealClaude/
│   ├── Public/
│   │   ├── UnrealClaudeModule.h        # Module interface
│   │   ├── ClaudeCodeRunner.h          # CLI execution (IClaudeRunner)
│   │   ├── ClaudeSubsystem.h           # Main orchestration
│   │   ├── ClaudeSessionManager.h      # Conversation persistence
│   │   ├── ClaudeEditorWidget.h        # Chat UI (Slate)
│   │   ├── ProjectContext.h            # Project info gathering
│   │   └── UnrealClaudeCommands.h      # Editor commands
│   └── Private/
│       ├── MCP/                        # MCP Server implementation
│       │   ├── UnrealClaudeMCPServer.h/cpp
│       │   └── Tools/                  # MCP tool implementations
│       └── ...
├── Resources/
│   └── mcp-bridge/                     # Node.js MCP bridge
├── Config/
│   └── FilterPlugin.ini
└── UnrealClaude.uplugin
```

## Known Limitations

- **Windows Only**: Uses Windows process APIs
- **CLI Dependency**: Requires Claude Code CLI installed and authenticated
- **No Streaming**: Responses appear all at once (CLI limitation)
- **Single Request**: One request at a time (cancellation available)

## Contributing

Pull requests welcome! Areas for improvement:

- [ ] Mac/Linux support
- [ ] Streaming output display
- [ ] Context menu integration (right-click on code)
- [ ] Blueprint node for runtime Claude queries
- [ ] Additional MCP tools

## License

MIT License - See [LICENSE](LICENSE) file.

## Credits

- Built for Unreal Engine 5.7
- Integrates with [Claude Code](https://claude.ai/code) by Anthropic
