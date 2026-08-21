#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
APP_PATH="${APP_PATH:-$PROJECT_ROOT/Builds/macOS/AshesOfHeaven.app}"
EXECUTABLE="$APP_PATH/Contents/MacOS/AshesOfHeaven-Mac-Shipping"
LOG_DIR="${PLAYTEST_LOG_DIR:-$PROJECT_ROOT/Saved/PlaytestLogs}"
LOG_FILE="$LOG_DIR/Playtest-$(date +%Y%m%d-%H%M%S).log"

if [[ ! -x "$EXECUTABLE" ]]; then
  echo "ERROR: packaged executable not found or not executable: $EXECUTABLE" >&2
  echo "Run ./Scripts/Build-Mac.sh first, or set APP_PATH to a packaged app." >&2
  exit 2
fi

mkdir -p "$LOG_DIR"
echo "Launching normal-renderer Mac playtest: $APP_PATH"
echo "Development log path: $LOG_FILE"
echo "Window: 1280x720 at (100,100); extra arguments are passed through."

exec "$EXECUTABLE" \
  -windowed -ResX=1280 -ResY=720 -WinX=100 -WinY=100 \
  -stdout -FullStdOutLogOutput -log="$LOG_FILE" "$@"
