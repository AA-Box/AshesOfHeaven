#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
TARGET="${1:-Erebus}"
if [[ $# -gt 0 ]]; then
  shift
fi
APP_PATH="${APP_PATH:-$PROJECT_ROOT/Builds/macOS-Development/AshesOfHeaven.app}"
EXECUTABLE="$(find "$APP_PATH/Contents/MacOS" -maxdepth 1 -type f -perm -111 2>/dev/null | head -1)"

if [[ ! -x "$EXECUTABLE" ]]; then
  echo "ERROR: packaged Development executable not found: $EXECUTABLE" >&2
  echo "Run CLIENT_CONFIG=Development ./Scripts/Build-Mac.sh first." >&2
  exit 2
fi

echo "Launching in-engine art target: $TARGET"
echo "Use the actual Unreal viewport for review; this script does not fabricate screenshots."
"$EXECUTABLE" \
  -windowed -ResX=1280 -ResY=720 -WinX=100 -WinY=100 \
  -ArtTarget="$TARGET" -stdout -FullStdOutLogOutput "$@"
