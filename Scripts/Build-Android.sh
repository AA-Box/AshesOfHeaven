#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_ROOT/AshesOfHeaven.uproject"
ENGINE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
UAT="$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
CLIENT_CONFIG="${CLIENT_CONFIG:-Shipping}"
if [[ "$CLIENT_CONFIG" != "Development" && "$CLIENT_CONFIG" != "Shipping" ]]; then
  echo "ERROR: CLIENT_CONFIG must be Development or Shipping." >&2
  exit 2
fi
if [[ "$CLIENT_CONFIG" == "Shipping" ]]; then
  OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/Builds/Android}"
else
  OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/Builds/Android-$CLIENT_CONFIG}"
fi
PSO_VALIDATOR=(python3 "$SCRIPT_DIR/Validate-PSO.py")

if [[ "$(uname -s)" != "Darwin" && "$(uname -s)" != "Linux" ]]; then
  echo "ERROR: Android packaging requires a supported Unreal build host." >&2
  exit 2
fi
if [[ ! -x "$UAT" ]]; then
  echo "ERROR: RunUAT.sh not found at $UAT. Set UE_ROOT to the installed Unreal Engine root." >&2
  exit 2
fi
if [[ -z "${ANDROID_HOME:-}" && -z "${ANDROID_SDK_ROOT:-}" ]]; then
  echo "ERROR: ANDROID_HOME or ANDROID_SDK_ROOT must point to the Android SDK." >&2
  exit 2
fi

"${PSO_VALIDATOR[@]}" config --platform android

mkdir -p "$OUTPUT_ROOT"
echo "Building AshesOfHeaven Android ARM64 $CLIENT_CONFIG APK (ASTC)..."
"$UAT" BuildCookRun \
  -project="$PROJECT_FILE" -noP4 -platform=Android -cookflavor=ASTC \
  -clientconfig="$CLIENT_CONFIG" -build -cook -stage -pak -package -archive \
  -prereqs -archivedirectory="$OUTPUT_ROOT"

if [[ "${AH_ANDROID_AAB:-0}" == "1" ]]; then
  if [[ "$CLIENT_CONFIG" != "Shipping" ]]; then
    echo "ERROR: AH_ANDROID_AAB=1 requires CLIENT_CONFIG=Shipping." >&2
    exit 2
  fi
  if [[ -z "${ANDROID_KEYSTORE:-}" ]]; then
    echo "ERROR: AH_ANDROID_AAB=1 requires ANDROID_KEYSTORE from the secure build environment." >&2
    exit 2
  fi
  echo "Building Android distribution artifact for store/AAB configuration..."
  "$UAT" BuildCookRun \
    -project="$PROJECT_FILE" -noP4 -platform=Android -cookflavor=ASTC \
    -clientconfig="$CLIENT_CONFIG" -distribution -build -cook -stage -pak -package -archive \
    -prereqs -archivedirectory="$OUTPUT_ROOT/AAB"
fi

"${PSO_VALIDATOR[@]}" package --platform android \
  --staged-root "$PROJECT_ROOT/Saved/StagedBuilds/Android_ASTC" \
  --archive-root "$OUTPUT_ROOT"

echo "Android output: $OUTPUT_ROOT"
find "$OUTPUT_ROOT" -maxdepth 5 \( -name '*.apk' -o -name '*.aab' \) -print
