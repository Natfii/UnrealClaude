# macOS Installation Guide for UnrealClaude

Tested on **Apple Silicon** (M4 Max), **UE 5.7.0**, **macOS 15**.

## What You Need

- **Unreal Engine 5.7** installed and working
- A **Claude Pro or Max subscription** (the plugin uses your existing Claude account)

## Step 1: Install Node.js

The plugin needs Node.js to run its MCP bridge. If you already have it, skip to Step 2.

**Option A** — Download the installer from [nodejs.org](https://nodejs.org/) (pick the LTS version).

**Option B** — If you have Homebrew:

```bash
brew install node
```

Verify it worked:

```bash
node --version   # should print v18 or higher
npm --version    # should print a version number
```

## Step 2: Install the Claude Code CLI

Open Terminal and run:

```bash
npm install -g @anthropic-ai/claude-code
```

Then authenticate:

```bash
claude
```

This opens a browser window to sign in with your Claude account. Once authenticated, you can close the terminal session.

## Step 3: Install the Plugin

### Prebuilt (recommended)

1. Download `UnrealClaude-Mac-v1.2.2.zip` from the [Releases page](https://github.com/Natfii/UnrealClaude/releases)
2. Unzip it — you should see a folder called `UnrealClaude`
3. Copy the `UnrealClaude` folder into your project's `Plugins` directory:

```
YourProject/
  Plugins/
    UnrealClaude/      <-- put it here
      Binaries/
      Resources/
      Source/
      UnrealClaude.uplugin
```

4. Install the MCP bridge dependencies:

```bash
cd YourProject/Plugins/UnrealClaude/Resources/mcp-bridge
npm install
```

### From source (if you prefer to build yourself)

```bash
# Clone the repo (includes the MCP bridge submodule)
git clone --recurse-submodules https://github.com/Natfii/UnrealClaude.git
cd UnrealClaude

# Install MCP bridge dependencies
cd UnrealClaude/Resources/mcp-bridge
npm install
cd ../../..

# Build (replace with your UE 5.7 path)
/Users/Shared/Epic\ Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin \
  -Plugin="$(pwd)/UnrealClaude/UnrealClaude.uplugin" \
  -Package="$(pwd)/BuiltPlugin" \
  -TargetPlatforms=Mac

# Copy to your project
cp -R BuiltPlugin YourProject/Plugins/UnrealClaude
```

## Step 4: Launch

1. Open your project in Unreal Engine 5.7
2. Go to **Tools > Claude Assistant** (or look for it in the menu bar)
3. The status bar at the bottom should show a green **Ready** indicator
4. Type a message and press Enter or click Send

## Clipboard Image Paste

You can paste images directly into the chat:
- Copy an image to your clipboard (screenshot, Preview, Photoshop, etc.)
- Click the **Paste** button in the chat panel

No additional tools or dependencies are needed on macOS.

## Troubleshooting

**Plugin doesn't appear in the editor:**
Make sure the `UnrealClaude` folder is directly inside `Plugins/` (not nested like `Plugins/UnrealClaude/UnrealClaude/`).

**Claude hangs at "Thinking...":**
Run `claude` in Terminal to check your authentication is working. If it asks you to log in, do so, then restart the editor.

**"Claude CLI not found" in the Output Log:**
The plugin looks for the `claude` binary in standard locations. Run `which claude` in Terminal to verify it's installed. If it's in an unusual location, make sure it's on your `PATH`.
