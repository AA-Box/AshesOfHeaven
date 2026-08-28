#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=Scripts/ah-test-evidence.sh
source "$SCRIPT_DIR/ah-test-evidence.sh"

# A project path containing a space breaks the native-shader-library step of every cook.
# FMacPlatformProcess::CreateProc splits the command line on spaces and truncates to 256 argv
# BEFORE re-joining the quoted paths (see its own "make sure we do not lose arguments with spaces"
# comment), so each shader path counts once per word. With "ASHES OF HEAVEN" in the path a 97-file
# metal-pack batch counts as 305 argv, gets truncated, and metal-pack then SIGSEGVs inside
# FMetalCompilerToolchain::ExecMetalPack:
#     LogHAL: Warning: FMacPlatformProcess::CreateProc: too many (305) commandline arguments passed
# The cook is fine when driven through a space-free symlink, but driving the COMPILE through one
# too breaks UnrealBuildTool's accelerator, which writes objects under one path and links them
# from the other ("ld: LINKEDIT content 'symbol table strings' extends beyond end of segment").
# So compile at the real path and cook through the symlink: they share the same physical Saved/
# and Binaries/, and no UBT runs in the cook phase. bSharedMaterialNativeLibraries=False also
# avoids the crash, by shrinking the batches, but it changes what ships and stays True.
SPACE_FREE_ROOT="$PROJECT_ROOT"
if [[ "$PROJECT_ROOT" == *" "* ]]; then
  SPACE_FREE_ROOT="$HOME/.cache/aoh"
  mkdir -p "$(dirname "$SPACE_FREE_ROOT")"
  ln -sfn "$PROJECT_ROOT" "$SPACE_FREE_ROOT"
fi

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
# Compile both targets directly, at the real project path. BuildCookRun -build on its own only
# built the editor here, so the cook phase happily staged a stale client binary and the capture
# ran last build's code - which is worse than a build failure, because it looks like a result.
UBT_BUILD=("$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh")
UAT_ARGS=(
  BuildCookRun
  "-project=$SPACE_FREE_ROOT/AshesOfHeaven.uproject"
  -noP4
  -platform=Mac
  "-clientconfig=$CLIENT_CONFIG"
  -nobuild -cook -stage -pak -archive -prereqs
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
"${UBT_BUILD[@]}" AshesOfHeavenEditor Mac Development "-project=$PROJECT_FILE" -WaitMutex
"${UBT_BUILD[@]}" AshesOfHeaven Mac "$CLIENT_CONFIG" "-project=$PROJECT_FILE" -WaitMutex
"$UAT" "${UAT_ARGS[@]}"

# Stale-client guard. BuildCookRun -build on its own built only the editor, so the cook happily
# staged a client binary from a previous run and the acceptance capture measured the previous
# build's code - which is worse than a failed build, because it looks like a result. The compiled
# client must be newer than the newest source file it was built from.
# UE omits the config suffix for Development targets and appends it for every other config,
# so an unsuffixed path here would stat the stale Development client and fail every Shipping build.
CLIENT_BINARY="$PROJECT_ROOT/Binaries/Mac/AshesOfHeaven"
[[ "$CLIENT_CONFIG" == "Development" ]] || CLIENT_BINARY+="-Mac-$CLIENT_CONFIG"
if [[ ! -f "$CLIENT_BINARY" ]]; then
  echo "ERROR: client binary missing at $CLIENT_BINARY; the build phase did not produce a client." >&2
  exit 2
fi
NEWEST_SOURCE_TIME="$(find "$PROJECT_ROOT/Source" -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.cs' \) -exec stat -f '%m' {} + | sort -rn | head -1)"
CLIENT_BUILD_TIME="$(stat -f '%m' "$CLIENT_BINARY")"
if (( CLIENT_BUILD_TIME < NEWEST_SOURCE_TIME )); then
  echo "ERROR: client binary is older than the newest source file." >&2
  echo "       binary  $(date -r "$CLIENT_BUILD_TIME")" >&2
  echo "       source  $(date -r "$NEWEST_SOURCE_TIME")" >&2
  echo "       The cook would stage a stale client. Rebuild before packaging." >&2
  exit 2
fi

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
# UE names the executable inside the bundle after the target, and only Development drops the
# config suffix - Shipping ships Contents/MacOS/AshesOfHeaven-Mac-Shipping. Hardcoding the
# Development name made this guard compare two paths that do not exist in a Shipping bundle;
# `cmp -s` on missing files exits non-zero, so every Shipping package failed here after a
# successful compile, cook and stage. Resolve the name, and separate "missing" from "differs"
# so a real packaging fault is never reported as a mismatch.
if [[ "$CLIENT_CONFIG" == "Development" ]]; then
  CLIENT_EXE="AshesOfHeaven"
else
  CLIENT_EXE="AshesOfHeaven-Mac-$CLIENT_CONFIG"
fi
for candidate in "$STAGED_APP/Contents/MacOS/$CLIENT_EXE" "$FINAL_APP/Contents/MacOS/$CLIENT_EXE"; do
  [[ -f "$candidate" ]] || { echo "ERROR: packaged client executable not found at $candidate" >&2; exit 2; }
done
if ! cmp -s "$STAGED_APP/Contents/MacOS/$CLIENT_EXE" "$FINAL_APP/Contents/MacOS/$CLIENT_EXE"; then
  echo "ERROR: archived client differs from the staged client; the archive is not this cook." >&2
  exit 2
fi
"${PSO_VALIDATOR[@]}" package --platform mac \
  --staged-root "$PROJECT_ROOT/Saved/StagedBuilds/Mac" \
  --archive-root "$OUTPUT_ROOT"
find "$OUTPUT_ROOT" -maxdepth 3 -name 'AshesOfHeaven.app' -print

# Only a Shipping package may record shipping evidence, and only here - past every failure
# path above, so a broken package cannot leave a green record behind. Development packages
# (the Level One E2E harness) deliberately write nothing: they prove the game runs, not that
# the shipping configuration compiles, cooks and archives.
if [[ "$CLIENT_CONFIG" == "Shipping" ]]; then
  cd "$PROJECT_ROOT"
  python3 - "$(ah_inputs_hash)" "$AH_SHIPPING_EVIDENCE_FILE" "$ENGINE_ROOT" "$CLIENT_CONFIG" <<'PYSHIPPING'
import json, os, pathlib, subprocess, sys

inputs_hash, evidence_path, engine_root, client_config = sys.argv[1:5]
pathlib.Path(evidence_path).write_text(json.dumps({
    "inputs_hash": inputs_hash,
    "config": client_config,
    "platform": "Mac",
    "engine": os.path.basename(engine_root.rstrip("/")),
    "host": subprocess.run(["uname", "-sm"], capture_output=True, text=True).stdout.strip(),
}, indent=2, sort_keys=True) + "\n")
print(f"Recorded Mac Shipping evidence in {evidence_path} (inputs_hash={inputs_hash[:12]}...).")
print("Commit it with the change it describes, or source-validation will reject the pull request.")
PYSHIPPING
fi
