# Linux Installation Guide for UnrealClaude

Verified on **Bazzite** (Fedora-based Atomic) host with a **Rocky Linux 9** Distrobox container, **UE 5.7**, and **NVIDIA RTX 4070** (Driver 590.48).

## System Dependencies

Install the required libraries (Rocky Linux 9 / Fedora):

```bash
sudo dnf install -y \
  nss nspr mesa-libgbm \
  libXcomposite libXdamage libXrandr \
  alsa-lib pciutils-libs libXcursor \
  atk at-spi2-atk \
  pango cairo gdk-pixbuf2 gtk3
```

These resolve common `libnss3.so` and `libatk-1.0.so` errors when launching the Unreal Editor on Linux.

## Clipboard Support

UnrealClaude supports clipboard image paste on Linux via two backends:

- **Wayland** (preferred): Requires `wl-clipboard`
- **X11** (fallback): Requires `xclip`

```bash
# Wayland (most modern Fedora/Bazzite setups)
sudo dnf install -y wl-clipboard

# X11 / XWayland fallback
sudo dnf install -y xclip
```

## Wayland Setup

If you are running a Wayland session (default on Fedora 42+, Bazzite, etc.), set these environment variables before launching the editor:

```bash
export SDL_VIDEODRIVER=wayland
export UE_Linux_EnableWaylandNative=1
```

You can add these to your `~/.bashrc` or create a launch wrapper script.

## NVIDIA Vulkan in Containers (Distrobox)

If running UE 5.7 inside a Distrobox or similar container, you may need to point Vulkan to the correct NVIDIA ICD:

```bash
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/nvidia_icd.json
```

Verify Vulkan is working:

```bash
vulkaninfo --summary
```

## Building the Plugin from Source

Linux users must compile the plugin from source (no prebuilt binaries are provided).

```bash
# Replace with your UE 5.7 install path
UE_ROOT=/path/to/UnrealEngine

$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh BuildPlugin \
  -Plugin="$(pwd)/UnrealClaude/UnrealClaude.uplugin" \
  -Package="$(pwd)/BuiltPlugin" \
  -TargetPlatforms=Linux
```

## MCP Bridge Setup

The MCP bridge requires Node.js >= 18.0.0:

```bash
# Install Node.js (Fedora)
sudo dnf install -y nodejs npm

# Verify version
node --version  # must be >= 18.0.0

# Install bridge dependencies
cd UnrealClaude/Resources/mcp-bridge
npm install
```

## Claude CLI

Install the Claude Code CLI:

```bash
npm install -g @anthropic-ai/claude-code
```

The plugin searches for the `claude` binary in these locations (in order):

1. `~/.local/bin/claude`
2. `/usr/local/bin/claude`
3. `/usr/bin/claude`
4. `~/.npm-global/bin/claude`
5. nvm-managed Node.js directories
6. Anywhere on your `PATH`

## Launch

```bash
./UnrealEditor -vulkan
```

Or with explicit Wayland support:

```bash
SDL_VIDEODRIVER=wayland UE_Linux_EnableWaylandNative=1 ./UnrealEditor -vulkan
```
