#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# Default to the Development package: the Phase 3.2 observability logs are guarded by
# #if !UE_BUILD_SHIPPING, and the installed engine ships Core with NO_LOGGING for
# Shipping, so a Shipping package writes no playtest log at all.
APP_PATH="${APP_PATH:-$PROJECT_ROOT/Builds/macOS-Development/AshesOfHeaven.app}"
# Development stages the executable as AshesOfHeaven; other configs append the config.
EXECUTABLE="$(find "$APP_PATH/Contents/MacOS" -maxdepth 1 -type f -perm -111 2>/dev/null | head -1)"
LOG_DIR="${PLAYTEST_LOG_DIR:-$PROJECT_ROOT/Saved/PlaytestLogs}"
LOG_FILE="$LOG_DIR/Playtest-$(date +%Y%m%d-%H%M%S).log"

if [[ ! -x "$EXECUTABLE" ]]; then
  echo "ERROR: packaged executable not found or not executable: $EXECUTABLE" >&2
  echo "Run 'CLIENT_CONFIG=Development ./Scripts/Build-Mac.sh' first, or set APP_PATH." >&2
  exit 2
fi

case "$EXECUTABLE" in
  *-Shipping)
    echo "WARNING: this is a Shipping package; it will not write a playtest log." >&2
    ;;
esac

mkdir -p "$LOG_DIR"
echo "Launching normal-renderer Mac playtest: $APP_PATH"
echo "Development log path: $LOG_FILE"
echo "Window: 1280x720 at (100,100); extra arguments are passed through."

# A packaged build ignores -log/-abslog and emits everything on stdout instead, so tee
# the stream rather than trusting the engine to open the file itself.
"$EXECUTABLE" \
  -windowed -ResX=1280 -ResY=720 -WinX=100 -WinY=100 \
  -stdout -FullStdOutLogOutput "$@" 2>&1 | tee "$LOG_FILE"
