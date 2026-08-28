#!/usr/bin/env bash
set -euo pipefail

# Runs the Unreal automation suite headless and fails on any test failure.
#
# The Level One narrative/progression/material tests are EditorContext|CommandletContext, so
# they only exist inside an editor or commandlet process. Without this script nothing in CI
# ever executes them and "automated tests pass before merge" is an unverified claim.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
# shellcheck source=Scripts/ah-test-evidence.sh
source "$SCRIPT_DIR/ah-test-evidence.sh"

# FMacPlatformProcess::CreateProc splits command lines on spaces, so a project path with a
# space breaks headless editor invocations. Drive them through a space-free symlink.
SPACE_FREE_ROOT="$PROJECT_ROOT"
if [[ "$PROJECT_ROOT" == *" "* ]]; then
  SPACE_FREE_ROOT="$HOME/.cache/aoh-tests"
  mkdir -p "$(dirname "$SPACE_FREE_ROOT")"
  ln -sfn "$PROJECT_ROOT" "$SPACE_FREE_ROOT"
fi

PROJECT_FILE="$PROJECT_ROOT/AshesOfHeaven.uproject"
RUN_PROJECT_FILE="$SPACE_FREE_ROOT/AshesOfHeaven.uproject"
ENGINE_ROOT="${UE_ROOT:-/Users/Shared/Epic Games/UE_5.8}"
TEST_FILTER="${AH_TEST_FILTER:-AshesOfHeaven}"
REPORT_DIR="${AH_TEST_REPORT_DIR:-$PROJECT_ROOT/Saved/AutomationReport}"
LOG_FILE="${AH_TEST_LOG:-$PROJECT_ROOT/Saved/Logs/AutomationRun.log}"
# The MCP editor plugin and a live editor both bind 8000; keep headless runs off it.
MCP_PORT="${AH_MCP_PORT:-18085}"
# Fewer than this many AshesOfHeaven.LevelOne.* results means the filter silently matched
# nothing, which must fail instead of reporting a green run.
MIN_LEVEL_ONE_TESTS="${AH_MIN_LEVEL_ONE_TESTS:-8}"
# Comma-separated test paths allowed to fail. A test listed here that PASSES also fails
# the run, so the list cannot quietly outlive the bug it documents. Empty by default.
export AH_KNOWN_FAILURES="${AH_KNOWN_FAILURES:-}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: this runner script targets macOS hosts. Use the engine's Build.bat/UnrealEditor-Cmd.exe equivalents on Windows." >&2
  exit 2
fi
BUILD_SH="$ENGINE_ROOT/Engine/Build/BatchFiles/Mac/Build.sh"
EDITOR_CMD="$ENGINE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd"
for required in "$BUILD_SH" "$EDITOR_CMD"; do
  [[ -x "$required" ]] || { echo "ERROR: $required not found. Set UE_ROOT to the installed Unreal Engine root." >&2; exit 2; }
done

rm -rf "$REPORT_DIR"
mkdir -p "$REPORT_DIR" "$(dirname "$LOG_FILE")"

echo "Building AshesOfHeavenEditor (Mac Development)..."
"$BUILD_SH" AshesOfHeavenEditor Mac Development -project="$PROJECT_FILE"

echo "Running automation tests matching '$TEST_FILTER'..."
set +e
"$EDITOR_CMD" "$RUN_PROJECT_FILE" \
  -ExecCmds="Automation RunTests $TEST_FILTER;Quit" \
  -TestExit="Automation Test Queue Empty" \
  -ReportExportPath="$REPORT_DIR" \
  -abslog="$LOG_FILE" \
  -unattended -nop4 -nosplash -nullrhi -nosound -stdout -FullStdOutLogOutput \
  -ModelContextProtocolPort="$MCP_PORT"
EDITOR_EXIT=$?
set -e
echo "UnrealEditor-Cmd exited with $EDITOR_EXIT (log: $LOG_FILE)"

AH_EVIDENCE_COUNTS="$(mktemp)"
export AH_EVIDENCE_COUNTS
trap 'rm -f "$AH_EVIDENCE_COUNTS"' EXIT

python3 - "$REPORT_DIR/index.json" "$MIN_LEVEL_ONE_TESTS" <<'PYREPORT'
import json, os, sys, pathlib

report_path, min_level_one = pathlib.Path(sys.argv[1]), int(sys.argv[2])
if not report_path.is_file():
    sys.exit(f"ERROR: no automation report at {report_path}; the test run did not complete.")

known_failures = {name.strip() for name in os.environ.get("AH_KNOWN_FAILURES", "").split(",") if name.strip()}

# UE writes the report with a UTF-8 BOM.
report = json.loads(report_path.read_text(encoding="utf-8-sig"))
tests = report.get("tests", [])
failed = [t for t in tests if t.get("state") != "Success"]
level_one = [t for t in tests if t.get("fullTestPath", "").startswith("AshesOfHeaven.LevelOne.")]
unexpected = [t for t in failed if t.get("fullTestPath") not in known_failures]
stale_known = [t.get("fullTestPath") for t in tests
               if t.get("state") == "Success" and t.get("fullTestPath") in known_failures]

print(f"automation: {len(tests)} tests, {len(failed)} not successful, {len(level_one)} Level One")
for test in failed:
    tolerated = " (known failure)" if test.get("fullTestPath") in known_failures else ""
    print(f"FAIL {test.get('fullTestPath')} state={test.get('state')}{tolerated}")
    for entry in test.get("entries", []):
        event = entry.get("event", {})
        if event.get("type") == "Error":
            print(f"    Error: {event.get('message')}")

problems = []
if unexpected:
    problems.append(f"{len(unexpected)} automation test(s) did not succeed: "
                    + ", ".join(t.get("fullTestPath") for t in unexpected))
if stale_known:
    problems.append("AH_KNOWN_FAILURES lists tests that now pass, remove them: " + ", ".join(stale_known))
if len(level_one) < min_level_one:
    problems.append(f"expected at least {min_level_one} AshesOfHeaven.LevelOne.* tests, ran {len(level_one)}")
if problems:
    sys.exit("ERROR: " + "; ".join(problems))
print("Automation suite passed." + (f" ({len(failed)} known failure(s) tolerated)" if failed else ""))

with open(os.environ["AH_EVIDENCE_COUNTS"], "w") as handle:
    json.dump({"tests": len(tests), "failed": len(failed), "level_one": len(level_one)}, handle)
PYREPORT

# The run passed. Record it against a hash of the inputs that produced it, so a hosted runner
# can tell on every pull request whether this result still describes the tree being merged.
# Only reached when the parser above exited zero, so a failing suite can never write evidence.
cd "$PROJECT_ROOT"
python3 - "$(ah_inputs_hash)" "$AH_EVIDENCE_COUNTS" "$AH_EVIDENCE_FILE" "$ENGINE_ROOT" <<'PYEVIDENCE'
import json, os, pathlib, subprocess, sys

inputs_hash, counts_path, evidence_path, engine_root = sys.argv[1:5]
counts = json.loads(pathlib.Path(counts_path).read_text())
evidence = {
    "inputs_hash": inputs_hash,
    "tests": counts["tests"],
    "failed": counts["failed"],
    "level_one": counts["level_one"],
    "engine": os.path.basename(engine_root.rstrip("/")),
    "host": subprocess.run(["uname", "-sm"], capture_output=True, text=True).stdout.strip(),
    "known_failures": sorted(n.strip() for n in os.environ.get("AH_KNOWN_FAILURES", "").split(",") if n.strip()),
}
pathlib.Path(evidence_path).write_text(json.dumps(evidence, indent=2, sort_keys=True) + "\n")
print(f"Recorded automation evidence in {evidence_path} (inputs_hash={inputs_hash[:12]}...).")
print("Commit it with the change it describes, or source-validation will reject the pull request.")
PYEVIDENCE
