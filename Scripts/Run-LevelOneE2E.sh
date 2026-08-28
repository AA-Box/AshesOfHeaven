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
# The game is launched headless (-nullrhi). It is not a rendering test - every assertion reads
# the log - and with a real RHI the packaged process blocks in FRenderCommandFence::Wait as soon
# as its window is not the frontmost surface, which is the normal state for an automated run.
# Headless also means this works over SSH and on a machine with no GUI session.
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
# Run 1 dies once at this objective and runs the failsafe clock out once, so a single packaged
# run covers both real lifecycles: death -> level reopen -> GameMode restore, and countdown
# expiry -> mission-failed banner -> fade -> checkpoint reload. Objective 2 is past the opening
# capture, so the checkpoint being restored carries real progress.
DEATH_OBJECTIVE="${AH_E2E_DEATH_OBJECTIVE:-2}"
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
  -freshchapter -LevelOneAutoplay "-LevelOneAutoplayDeathAt=$DEATH_OBJECTIVE" \
  -LevelOneAutoplayFailsafeExpiry -nullrhi -nosound -unattended

RUN1_LOG="$PROJECT_ROOT/Saved/Logs/LevelOneE2E-Run1.log"
mkdir -p "$(dirname "$RUN1_LOG")"
cp "$GAME_LOG" "$RUN1_LOG"

grep -qF "[Campaign][Save] chapter_complete id=Ch01 result=success" "$RUN1_LOG" || {
  echo "ERROR: the run finished without writing campaign completion to disk." >&2
  grep -F "[Campaign][Save]" "$RUN1_LOG" >&2 || echo "    (no [Campaign][Save] lines at all)" >&2
  exit 1
}
echo "    campaign completion written"

# Both lifecycles have to have actually happened inside that run. Asserting only the final
# completion would pass even if the death and the failsafe expiry had silently no-opped.
assert_in_run1() {
  local pattern="$1"
  local description="$2"
  if grep -qF "$pattern" "$RUN1_LOG"; then
    echo "    $description"
  else
    echo "ERROR: $description did not happen ('$pattern' absent from Run 1)." >&2
    RUN1_FAILED=1
  fi
}
RUN1_FAILED=0
assert_in_run1 "[LevelOneE2E] autoplay_death" "the player died during the run"
assert_in_run1 "[Phase3.2][Player] death restart_scheduled=true" "death scheduled the real restart"
assert_in_run1 "[Phase3.2][Player] death_restart_execute" "the restart executed and reopened the level"
assert_in_run1 "[LevelOneE2E] autoplay_failsafe_expiry" "the failsafe clock was run out"
assert_in_run1 "[Chapter][Countdown] failsafe_expired" "the failsafe clock reported expiry"
assert_in_run1 "[Chapter][Failsafe] mission_failed reason=transmission_complete" "expiry failed the mission"

# The level must be reopened at least twice after the initial boot (once per lifecycle), and
# each reopen re-runs AAHChapterOneGameMode's restore path.
RESTORES="$(grep -cF "[Phase4.4][Runtime]" "$RUN1_LOG" || true)"
if [ "$RESTORES" -lt 3 ]; then
  echo "ERROR: expected at least 3 GameMode restores in Run 1 (boot + death reload + failsafe reload), saw $RESTORES." >&2
  RUN1_FAILED=1
else
  echo "    GameMode restored $RESTORES times (boot + death reload + failsafe reload)"
fi

# The retry after a failsafe failure must get the full window back, not the remainder.
if grep -qF "[Phase3.2][Countdown] begin seconds=522.0" "$RUN1_LOG"; then
  echo "    the restored attempt got the full failsafe window back"
else
  echo "ERROR: the restored attempt did not restart the failsafe clock at its full window." >&2
  RUN1_FAILED=1
fi

if [ "$RUN1_FAILED" -ne 0 ]; then
  echo "Run 1 log: $RUN1_LOG" >&2
  exit 1
fi

# The process must be gone before the save file is judged: this is the "quit the game" step.
terminate_game
[[ -f "$SAVE_FILE" ]] || { echo "ERROR: no save file at $SAVE_FILE after completing the chapter." >&2; exit 1; }
echo "==> Save on disk: $(stat -f '%z bytes' "$SAVE_FILE")"

# ---- Run 2: relaunch from that save. No -freshchapter, no autoplay. --------------------
launch_and_await "Run 2: relaunching from the save on disk" \
  "[Phase4.4][Runtime]" "$RELAUNCH_TIMEOUT" \
  -nullrhi -nosound -unattended

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
