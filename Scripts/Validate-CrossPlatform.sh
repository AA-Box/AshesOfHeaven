#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"
# shellcheck source=Scripts/ah-test-evidence.sh
source "$SCRIPT_DIR/ah-test-evidence.sh"

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
  "Scripts/Validate-PSO.py"
  "Scripts/Run-AutomationTests.sh"
  "Scripts/ah-test-evidence.sh"
  "Docs/automation-evidence.json"
  "Docs/shipping-evidence.json"
  "Docs/PSO_AND_SHADER_PIPELINE.md"
  # Material families asserted by AshesOfHeaven.LevelOne.UnrealMaterialContract. That test
  # needs an editor/commandlet; this check catches a deleted instance on any runner.
  "Content/Ashes/Materials/Instances/MI_Concrete_Wet.uasset"
  "Content/Ashes/Materials/Instances/MI_HumanMetal_Dark.uasset"
  "Content/Ashes/Materials/Instances/MI_CathedralMatter_Dark.uasset"
  "Content/Ashes/Materials/Instances/MI_VeilObsidian_Black.uasset"
  "Content/Ashes/Materials/Instances/MI_EmissiveGlyph_Cyan.uasset"
  "Content/Ashes/Materials/Instances/MI_Erebus_BannerCloth.uasset"
  "Content/Ashes/Materials/Instances/MI_Erebus_BannerEmblem.uasset"
  "Content/Ashes/Materials/Instances/MI_Erebus_CathedralSilhouette.uasset"
)
for file in "${required[@]}"; do
  [[ -f "$file" ]] || { echo "ERROR: missing required cross-platform file: $file" >&2; exit 1; }
done

# A scan that did not run must never be reported as a clean scan. This was `if rg ...; then`
# and ubuntu-latest has no rg: the 127 exit reads as "the pattern matched nothing", so the gate
# reported clean having never looked. grep is in every image, and its status is checked
# explicitly - 1 is "no matches" and passes, anything above that means the scan itself broke.
raw="$(mktemp)"
leaks="$(mktemp)"
set +e
grep -rEn --include='*.cpp' --include='*.h' \
  'PLATFORM_(WINDOWS|MAC|ANDROID|IOS)|WIN32|Windows\.h|\\\\' \
  Source >"$raw"
scan=$?
set -e
if (( scan > 1 )); then
  echo "ERROR: the platform leak scan did not run (grep exited $scan)" >&2
  exit 1
fi
grep -v '/Platform/' "$raw" >"$leaks" || true
if [[ -s "$leaks" ]]; then
  echo "ERROR: platform-specific gameplay dependency found outside Source/.../Platform:" >&2
  cat "$leaks" >&2
  exit 1
fi

if find . -type f \( -name '*.p12' -o -name '*.mobileprovision' -o -name '*.keystore' -o -name '*.jks' \) \
  -not -path './Intermediate/*' -not -path './Saved/*' | grep -q .; then
  echo "ERROR: signing material must never be committed to the project." >&2
  exit 1
fi

bash -n Scripts/Build-Mac.sh Scripts/Build-Android.sh Scripts/Build-iOS.sh Scripts/Run-AutomationTests.sh
python3 Scripts/Validate-PSO.py config --all-platforms

# Every python test in the tree, so adding one gates the PR without touching this script.
# Deliberately NOT `unittest discover`: test_safe_extract.py is plain asserts with no TestCase,
# so discover imports it, finds nothing to run, and reports success having skipped its checks.
# Each file runs standalone and exits non-zero when it fails.
for test in Scripts/tests/test_*.py; do
  echo "== $test"
  python3 "$test"
done
# The gameplay automation suite needs an Unreal editor/commandlet, which no hosted runner has,
# and this repository is public so a self-hosted runner is not an option (fork code would
# execute on the runner's machine). The suite therefore runs on a developer machine and records
# its result against a hash of the inputs; this is where that claim is checked. Without it,
# "automated tests pass before merge" is exactly the unverified claim it was before.
echo "== recorded evidence"
expected_hash="$(ah_inputs_hash)"
python3 - "$expected_hash" "$AH_EVIDENCE_FILE" "$AH_SHIPPING_EVIDENCE_FILE" <<'PYEVIDENCE'
import json, pathlib, sys

expected_hash = sys.argv[1]
automation_path, shipping_path = pathlib.Path(sys.argv[2]), pathlib.Path(sys.argv[3])


def load(path, required_keys):
    try:
        evidence = json.loads(path.read_text())
    except (OSError, ValueError) as error:
        sys.exit(f"ERROR: cannot read {path}: {error}")
    for key in required_keys:
        if key not in evidence:
            sys.exit(f"ERROR: {path} is missing '{key}'.")
    if evidence["inputs_hash"] != expected_hash:
        sys.exit(
            f"ERROR: {path} describes a different tree.\n"
            f"  recorded: {evidence['inputs_hash']}\n"
            f"  this PR:  {expected_hash}\n"
            "  Source, Config, Content, Scripts or the .uproject changed since it was written."
        )
    return evidence


# macos-shipping is a SEPARATE claim: Run-AutomationTests.sh only builds
# AshesOfHeavenEditor Mac Development, so the automation evidence says nothing about whether
# the Shipping configuration still compiles, cooks and archives. Re-run
# `CLIENT_CONFIG=Shipping ./Scripts/Build-Mac.sh` and commit the refreshed file.
shipping = load(shipping_path, ("inputs_hash", "config", "platform"))
if shipping["config"] != "Shipping" or shipping["platform"] != "Mac":
    sys.exit(f"ERROR: {shipping_path} is not a Mac Shipping record "
             f"(config={shipping.get('config')}, platform={shipping.get('platform')}).")
print(f"Mac Shipping evidence matches this tree "
      f"({shipping.get('engine', 'unknown engine')} on {shipping.get('host', 'unknown host')})")

evidence_path, expected_hash = automation_path, expected_hash
evidence = load(evidence_path, ("inputs_hash", "tests", "failed", "level_one"))

if evidence["failed"] != len(evidence.get("known_failures", [])):
    sys.exit(f"ERROR: {evidence_path} records {evidence['failed']} unsuccessful test(s).")

print(f"automation evidence matches this tree: {evidence['tests']} tests, "
      f"{evidence['level_one']} Level One, {evidence['failed']} not successful "
      f"({evidence.get('engine', 'unknown engine')} on {evidence.get('host', 'unknown host')})")
PYEVIDENCE

echo "Cross-platform source/config validation passed. Platform binaries remain evidence-gated in Docs/PLATFORM_MATRIX.md."
