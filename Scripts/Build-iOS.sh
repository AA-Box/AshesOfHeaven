#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PROJECT_FILE="$PROJECT_ROOT/AshesOfHeaven.uproject"
ENGINE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
UAT="$ENGINE_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
OUTPUT_ROOT="${OUTPUT_ROOT:-$PROJECT_ROOT/Builds/iOS}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: iOS/iPadOS packaging requires macOS and Xcode." >&2
  exit 2
fi
if ! command -v xcodebuild >/dev/null 2>&1; then
  echo "ERROR: Xcode command-line tools are unavailable." >&2
  exit 2
fi
if [[ ! -x "$UAT" ]]; then
  echo "ERROR: RunUAT.sh not found at $UAT. Set UE_ROOT to the installed Unreal Engine root." >&2
  exit 2
fi

if [[ "${AH_IOS_DISTRIBUTION:-0}" == "1" ]]; then
  : "${AH_IOS_TEAM_ID:?ERROR: AH_IOS_TEAM_ID is required for distribution packaging}"
  : "${AH_IOS_PROVISIONING_PROFILE:?ERROR: AH_IOS_PROVISIONING_PROFILE is required for distribution packaging}"
  echo "Distribution signing is enabled through the external Apple credential environment."
else
  echo "Building unsigned/development-compatible iOS package configuration."
fi

mkdir -p "$OUTPUT_ROOT"
PACKAGE_ARGS=()
if [[ "${AH_IOS_DISTRIBUTION:-0}" == "1" ]]; then
  PACKAGE_ARGS+=("-distribution")
fi
"$UAT" BuildCookRun \
  -project="$PROJECT_FILE" -noP4 -platform=IOS -clientconfig=Shipping \
  -build -cook -stage -pak -package -archive \
  "${PACKAGE_ARGS[@]}" \
  -prereqs -archivedirectory="$OUTPUT_ROOT"

echo "iOS/iPadOS output: $OUTPUT_ROOT"
find "$OUTPUT_ROOT" -maxdepth 5 \( -name '*.ipa' -o -name '*.app' \) -print
