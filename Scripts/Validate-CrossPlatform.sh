#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

required=(
  "AshesOfHeaven.uproject"
  "Config/DefaultDeviceProfiles.ini"
  "Config/DefaultScalability.ini"
  "Docs/PLATFORM_MATRIX.md"
  "Docs/PERFORMANCE_BUDGETS.md"
  "Docs/BUILD_AND_INSTALL.md"
  "Scripts/Build-Windows.ps1"
  "Scripts/Build-Mac.sh"
  "Scripts/Build-Android.sh"
  "Scripts/Build-iOS.sh"
)
for file in "${required[@]}"; do
  [[ -f "$file" ]] || { echo "ERROR: missing required cross-platform file: $file" >&2; exit 1; }
done

if rg -n --glob '*.cpp' --glob '*.h' \
  'PLATFORM_(WINDOWS|MAC|ANDROID|IOS)|WIN32|Windows\.h|\\\\' \
  Source --glob '!Platform/**' >/tmp/ashes_platform_leaks.txt; then
  echo "ERROR: platform-specific gameplay dependency found outside Source/.../Platform:" >&2
  cat /tmp/ashes_platform_leaks.txt >&2
  exit 1
fi

if find . -type f \( -name '*.p12' -o -name '*.mobileprovision' -o -name '*.keystore' -o -name '*.jks' \) \
  -not -path './Intermediate/*' -not -path './Saved/*' | grep -q .; then
  echo "ERROR: signing material must never be committed to the project." >&2
  exit 1
fi

bash -n Scripts/Build-Mac.sh Scripts/Build-Android.sh Scripts/Build-iOS.sh
echo "Cross-platform source/config validation passed. Platform binaries remain evidence-gated in Docs/PLATFORM_MATRIX.md."
