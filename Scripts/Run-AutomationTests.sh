#!/usr/bin/env bash
set -euo pipefail

# Runs the Unreal automation suite headless and fails on any test failure.
#
# The Level One narrative/progression/material tests are EditorContext|CommandletContext, so
# they only exist inside an editor or commandlet process. Without this script nothing in CI
# ever executes them and "automated tests pass before merge" is an unverified claim.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

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
MIN_LEVEL_ONE_TESTS="${AH_MIN_LEVEL_ONE_TESTS:-6}"
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
PYREPORT
