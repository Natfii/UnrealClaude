#!/usr/bin/env bash
# Phase 9: Simple local check that the UnrealClaude plugin still builds.
#
# Usage:
#   ./Scripts/BuildPlugin.sh /path/to/UnrealEngine
# or:
#   UE_ROOT=/path/to/UnrealEngine ./Scripts/BuildPlugin.sh
#
# Requires Unreal Engine 5.7 or compatible with RunUAT.sh.

set -euo pipefail

UE_ROOT="${1:-${UE_ROOT:-}}"

if [[ -z "$UE_ROOT" ]]; then
    echo "ERROR: Pass the Unreal Engine root as the first argument or set UE_ROOT."
    echo "Example: ./Scripts/BuildPlugin.sh /opt/UnrealEngine"
    exit 1
fi

UAT="$UE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"

if [[ ! -f "$UAT" ]]; then
    if [[ -f "$UE_ROOT/Engine/Build/BatchFiles/RunUAT.bat" ]]; then
        echo "ERROR: Found RunUAT.bat but this script requires RunUAT.sh."
        echo "Run the Windows batch equivalent or use a platform with RunUAT.sh."
        exit 1
    fi

    echo "ERROR: Could not find RunUAT.sh at:"
    echo "  $UE_ROOT/Engine/Build/BatchFiles/"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PLUGIN_DIR="$(cd "$SCRIPT_DIR/../UnrealClaude" && pwd)"
PLUGIN_FILE="$PLUGIN_DIR/UnrealClaude.uplugin"

if [[ ! -f "$PLUGIN_FILE" ]]; then
    echo "ERROR: Plugin descriptor not found at:"
    echo "  $PLUGIN_FILE"
    exit 1
fi

echo "Building UnrealClaude plugin..."
echo "  Engine : $UE_ROOT"
echo "  Plugin : $PLUGIN_FILE"

"$UAT" BuildPlugin \
    -Plugin="$PLUGIN_FILE" \
    -Package="/tmp/UnrealClaude-Build"

echo "SUCCESS: Plugin build completed."
