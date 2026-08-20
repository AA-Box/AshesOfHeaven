#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_ROOT/AshesOfHeaven.uproject"
ENGINE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
UAT="$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/Builds/macOS}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: macOS packaging must run on macOS with Apple's toolchain." >&2
  exit 2
fi
if [[ ! -x "$UAT" ]]; then
  echo "ERROR: RunUAT.sh not found at $UAT. Set UE_ROOT to the installed Unreal Engine root." >&2
  exit 2
fi

mkdir -p "$OUTPUT_ROOT"
echo "Building AshesOfHeaven Mac Shipping package..."
"$UAT" BuildCookRun \
  -project="$PROJECT_FILE" -noP4 -platform=Mac -clientconfig=Shipping \
  -build -cook -stage -pak -archive -prereqs -archivedirectory="$OUTPUT_ROOT"

echo "Mac output: $OUTPUT_ROOT"
ARCHIVED_APP="$OUTPUT_ROOT/AshesOfHeaven-Mac-Shipping.app"
FINAL_APP="$OUTPUT_ROOT/AshesOfHeaven.app"
if [[ -d "$ARCHIVED_APP" && ! -e "$FINAL_APP" ]]; then
  mv "$ARCHIVED_APP" "$FINAL_APP"
elif [[ -d "$ARCHIVED_APP" && -d "$FINAL_APP" ]]; then
  ditto "$ARCHIVED_APP" "$FINAL_APP"
fi
find "$OUTPUT_ROOT" -maxdepth 3 -name 'AshesOfHeaven.app' -print
