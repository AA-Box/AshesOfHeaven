#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_ROOT/AshesOfHeaven.uproject"
ENGINE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
UAT="$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
# Shipping is the packaging gate. Development is the playtest build: the Phase 3.2
# observability logs are guarded by #if !UE_BUILD_SHIPPING, and the installed engine
# ships Core with NO_LOGGING for Shipping, so only non-Shipping writes a playtest log.
CLIENT_CONFIG="${CLIENT_CONFIG:-Shipping}"
if [[ "$CLIENT_CONFIG" != "Development" && "$CLIENT_CONFIG" != "Shipping" ]]; then
  echo "ERROR: CLIENT_CONFIG must be Development or Shipping." >&2
  exit 2
fi
if [[ "$CLIENT_CONFIG" == "Shipping" ]]; then
  OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/Builds/macOS}"
else
  OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/Builds/macOS-$CLIENT_CONFIG}"
fi
PSO_VALIDATOR=(python3 "$SCRIPT_DIR/Validate-PSO.py")

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: macOS packaging must run on macOS with Apple's toolchain." >&2
  exit 2
fi
if [[ ! -x "$UAT" ]]; then
  echo "ERROR: RunUAT.sh not found at $UAT. Set UE_ROOT to the installed Unreal Engine root." >&2
  exit 2
fi

"${PSO_VALIDATOR[@]}" config --platform mac

mkdir -p "$OUTPUT_ROOT"
echo "Building AshesOfHeaven Mac $CLIENT_CONFIG package..."
UAT_ARGS=(
  BuildCookRun
  "-project=$PROJECT_FILE"
  -noP4
  -platform=Mac
  "-clientconfig=$CLIENT_CONFIG"
  -build -cook -stage -pak -archive -prereqs
  "-archivedirectory=$OUTPUT_ROOT"
)
if [[ -n "${ADDITIONAL_COOKER_OPTIONS:-}" ]]; then
  UAT_ARGS+=("-AdditionalCookerOptions=$ADDITIONAL_COOKER_OPTIONS")
elif command -v lsof >/dev/null 2>&1 && lsof -nP -iTCP:8000 -sTCP:LISTEN -t >/dev/null 2>&1; then
  # The editor's Unreal MCP server commonly owns 8000. Cook treats its failed
  # auto-start as a fatal error, so give the cook commandlet an isolated port.
  for MCP_COOK_PORT in $(seq 18080 18099); do
    if ! lsof -nP -iTCP:${MCP_COOK_PORT} -sTCP:LISTEN -t >/dev/null 2>&1; then
      UAT_ARGS+=("-AdditionalCookerOptions=-ModelContextProtocolPort=${MCP_COOK_PORT}")
      echo "MCP port 8000 is occupied; cooking with ModelContextProtocolPort=${MCP_COOK_PORT}."
      break
    fi
  done
fi
"$UAT" "${UAT_ARGS[@]}"

echo "Mac output: $OUTPUT_ROOT"
# Take the app from the staging directory, not from -archive: UAT's Mac archive step
# copies Binaries/Mac/<Target>.app, whose Contents/UE can hold cooked content from an
# earlier run. Staging is the only bundle guaranteed to match this cook.
# UE omits the config suffix for Development targets and appends it for every other config.
if [[ "$CLIENT_CONFIG" == "Development" ]]; then
  STAGED_APP="$PROJECT_ROOT/Saved/StagedBuilds/Mac/AshesOfHeaven.app"
else
  STAGED_APP="$PROJECT_ROOT/Saved/StagedBuilds/Mac/AshesOfHeaven-Mac-$CLIENT_CONFIG.app"
fi
FINAL_APP="$OUTPUT_ROOT/AshesOfHeaven.app"
if [[ ! -d "$STAGED_APP" ]]; then
  echo "ERROR: staged app not found at $STAGED_APP" >&2
  exit 2
fi
# Replace rather than ditto-merge, so removed files never survive into the next package.
rm -rf "$FINAL_APP" "$OUTPUT_ROOT/AshesOfHeaven-Mac-$CLIENT_CONFIG.app"
ditto "$STAGED_APP" "$FINAL_APP"
"${PSO_VALIDATOR[@]}" package --platform mac \
  --staged-root "$PROJECT_ROOT/Saved/StagedBuilds/Mac" \
  --archive-root "$OUTPUT_ROOT"
find "$OUTPUT_ROOT" -maxdepth 3 -name 'AshesOfHeaven.app' -print
