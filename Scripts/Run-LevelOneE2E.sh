#!/usr/bin/env bash
set -euo pipefail

# Packaged end-to-end proof of the Level One campaign lifecycle:
#
#   package -> fresh launch -> play all twelve objectives -> chapter completes -> save written
#   -> terminate the process -> launch again -> the level is STILL complete
#
# The automation suite (AshesOfHeaven.LevelOne.CampaignE2E.*) covers the same contract
# in-process. This script is the half a test inside the editor cannot prove: that the state
# survives a real process boundary, written by the packaged binary to the packaged save path.
#
# Synthetic keyboard and mouse input does not reach the packaged game on macOS here, so the
# playthrough is driven by -LevelOneAutoplay, which completes one objective at a time through
# the ordinary UAHObjectiveSubsystem path. It does not write completion directly: the chapter
# still has to route through the director and OnMissionComplete to reach the save.
#
# Env knobs:
#   AH_SKIP_PACKAGE=1   reuse the existing Builds/macOS-Development app
#   AH_E2E_TIMEOUT=600  seconds to wait for the playthrough to finish (default 420)

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
APP_ROOT="${AH_E2E_APP_ROOT:-$PROJECT_ROOT/Builds/macOS-Development}"
APP="$APP_ROOT/AshesOfHeaven.app"
EXE="$APP/Contents/MacOS/AshesOfHeaven"
PLAY_TIMEOUT="${AH_E2E_TIMEOUT:-420}"
RELAUNCH_TIMEOUT="${AH_E2E_RELAUNCH_TIMEOUT:-180}"

if [[ "$(uname -s)" != "Darwin" ]]; then
  echo "ERROR: the packaged Level One E2E runs on macOS." >&2
  exit 2
fi

CONTAINER="$HOME/Library/Containers/com.YourCompany.AshesOfHeaven/Data/Library"
GAME_LOG="$CONTAINER/Logs/AshesOfHeaven/AshesOfHeaven.log"
SAVE_FILE="$CONTAINER/Application Support/Epic/AshesOfHeaven/Saved/SaveGames/AshesOfHeaven_Slot_0.sav"

if [[ "${AH_SKIP_PACKAGE:-0}" != "1" ]]; then
  echo "==> Packaging macOS Development build"
  CLIENT_CONFIG=Development "$SCRIPT_DIR/Build-Mac.sh"
fi
[[ -x "$EXE" ]] || { echo "ERROR: packaged executable not found at $EXE" >&2; exit 2; }

# Kills the game and waits for the process to actually leave, so the next launch cannot
# inherit its window, its log handle or its save file lock.
terminate_game() {
  pkill -f "AshesOfHeaven.app/Contents/MacOS/AshesOfHeaven" 2>/dev/null || true
  for _ in $(seq 1 40); do
    pgrep -f "AshesOfHeaven.app/Contents/MacOS/AshesOfHeaven" >/dev/null || return 0
    sleep 0.5
  done
  pkill -9 -f "AshesOfHeaven.app/Contents/MacOS/AshesOfHeaven" 2>/dev/null || true
  sleep 1
}

# Launches the packaged game detached (it dies with the shell call otherwise) and waits for a
# log line, failing on timeout rather than hanging the run.
launch_and_await() {
  local description="$1"; shift
  local pattern="$1"; shift
  local timeout_seconds="$1"; shift

  terminate_game
  rm -f "$GAME_LOG"
  echo "==> $description"
  echo "    args: $*"
  ( nohup "$EXE" "$@" >/dev/null 2>&1 & )

  local waited=0
  while (( waited < timeout_seconds )); do
    if [[ -f "$GAME_LOG" ]] && grep -qF "$pattern" "$GAME_LOG"; then
      echo "    matched: $pattern (after ${waited}s)"
      return 0
    fi
    if (( waited > 30 )) && ! pgrep -f "AshesOfHeaven.app/Contents/MacOS/AshesOfHeaven" >/dev/null; then
      echo "ERROR: the packaged game exited before '$pattern' appeared." >&2
      [[ -f "$GAME_LOG" ]] && tail -40 "$GAME_LOG" >&2
      return 1
    fi
    sleep 2
    waited=$((waited + 2))
  done
  echo "ERROR: timed out after ${timeout_seconds}s waiting for '$pattern'." >&2
  [[ -f "$GAME_LOG" ]] && tail -60 "$GAME_LOG" >&2
  return 1
}

echo "==> Clearing the packaged campaign save"
terminate_game
rm -f "$SAVE_FILE"

# ---- Run 1: a complete playthrough, from a guaranteed-fresh chapter. -------------------
launch_and_await "Run 1: playing Level One to completion" \
  "[LevelOneE2E] autoplay_finished missionComplete=true" "$PLAY_TIMEOUT" \
  -freshchapter -LevelOneAutoplay -windowed -ResX=640 -ResY=360

RUN1_LOG="$PROJECT_ROOT/Saved/Logs/LevelOneE2E-Run1.log"
mkdir -p "$(dirname "$RUN1_LOG")"
cp "$GAME_LOG" "$RUN1_LOG"

grep -qF "[Campaign][Save] chapter_complete id=Ch01 result=success" "$RUN1_LOG" || {
  echo "ERROR: the run finished without writing campaign completion to disk." >&2
  grep -F "[Campaign][Save]" "$RUN1_LOG" >&2 || echo "    (no [Campaign][Save] lines at all)" >&2
  exit 1
}
echo "    campaign completion written"

# The process must be gone before the save file is judged: this is the "quit the game" step.
terminate_game
[[ -f "$SAVE_FILE" ]] || { echo "ERROR: no save file at $SAVE_FILE after completing the chapter." >&2; exit 1; }
echo "==> Save on disk: $(stat -f '%z bytes' "$SAVE_FILE")"

# ---- Run 2: relaunch from that save. No -freshchapter, no autoplay. --------------------
launch_and_await "Run 2: relaunching from the save on disk" \
  "[Phase4.4][Runtime]" "$RELAUNCH_TIMEOUT" \
  -windowed -ResX=640 -ResY=360

RUN2_LOG="$PROJECT_ROOT/Saved/Logs/LevelOneE2E-Run2.log"
cp "$GAME_LOG" "$RUN2_LOG"
terminate_game

RUNTIME_LINE="$(grep -F "[Phase4.4][Runtime]" "$RUN2_LOG" | tail -1)"
echo "==> Relaunch state: $RUNTIME_LINE"

FAILED=0
case "$RUNTIME_LINE" in
  *"ChapterComplete=true"*) echo "    PASS: the relaunched process still reports Level One complete" ;;
  *) echo "ERROR: the relaunched process does not report Level One complete." >&2; FAILED=1 ;;
esac
case "$RUNTIME_LINE" in
  *"Stage=EAHChapterStage::ChapterComplete"*) echo "    PASS: the restored stage is ChapterComplete" ;;
  *) echo "ERROR: the restored stage is not ChapterComplete (a mid-level checkpoint won)." >&2; FAILED=1 ;;
esac

if (( FAILED )); then
  echo "Logs: $RUN1_LOG, $RUN2_LOG" >&2
  exit 1
fi
echo "==> Level One packaged E2E passed. Logs: $RUN1_LOG, $RUN2_LOG"
