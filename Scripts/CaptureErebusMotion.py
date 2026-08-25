#!/usr/bin/env python3
"""Record VIDEO of the packaged build at a fixed Erebus pose, then extract frames.

CAVEAT: this harness is NOT trustworthy for absolute numbers yet - see
ScoreErebusMotion.py and Docs/FABLE_ART_LESSONS.md. The consecutive-frame contact
sheets it enables ARE trustworthy; the statistics need a static vantage first.

Every judgement in the fire/smoke pass so far came from single stills. Fire is a motion
phenomenon: sliding noise, sprite pop, per-frame flicker and TSR shimmer on low-alpha
translucency are invisible in a still and obvious in two consecutive frames.

Reuses ErebusAcceptance's launch contract exactly (same -ArtTarget/-ArtCam/-windowed
arguments, same load and settle timings, same "front it or the window renders black"
rule) so the footage is directly comparable to the acceptance stills.
"""
import subprocess
import sys
import time
from pathlib import Path

import os
REPO = Path(os.environ.get("AOH_REPO", Path(__file__).resolve().parents[1]))
APP = REPO / "Builds/macOS-Development/AshesOfHeaven.app"
EXE = APP / "Contents/MacOS/AshesOfHeaven"
SAVE = Path.home() / (
    "Library/Containers/com.YourCompany.AshesOfHeaven/Data/Library/Application Support"
    "/Epic/AshesOfHeaven/Saved/SaveGames/AshesOfHeaven_Slot_0.sav"
)
OUT = Path(sys.argv[1] if len(sys.argv) > 1 else REPO / "Saved/Motion")
POSE = (-1800, -120, 300, -3, 0)          # shot1_route_entry, two fires in frame
TARGET = "Erebus"
LOAD_SECONDS = 18
SETTLE_SECONDS = 16
RECORD_SECONDS = int(sys.argv[2]) if len(sys.argv) > 2 else 8
FPS_OUT = 10                               # frames to pull out per second of footage

OUT.mkdir(parents=True, exist_ok=True)
mov = OUT / "erebus_motion.mov"
log_path = OUT / "erebus_motion.log"

subprocess.run(["pkill", "-f", "macOS-Development/AshesOfHeaven.app"], check=False)
time.sleep(2)
SAVE.unlink(missing_ok=True)

argv = [str(EXE), "-freshchapter", f"-ArtTarget={TARGET}",
        "-ArtCam=" + ",".join(str(v) for v in POSE),
        "-windowed", "-ResX=1280", "-ResY=720", "-WinX=100", "-WinY=100",
        "-stdout", "-FullStdOutLogOutput"]
with open(log_path, "wb") as log:
    subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
print(f"launched, waiting {LOAD_SECONDS}s for the stage")
time.sleep(LOAD_SECONDS)
subprocess.run(["open", "-a", str(APP)], check=False)   # fronts it; hidden = black frames
print(f"fronted, waiting {SETTLE_SECONDS}s for auto-exposure")
time.sleep(SETTLE_SECONDS)

mov.unlink(missing_ok=True)
print(f"recording {RECORD_SECONDS}s")
# The packaged window renders a BLACK 3D scene whenever it is not frontmost, while
# Slate keeps drawing the HUD. Over a 10s capture it lost focus repeatedly and 24% of
# frames came out with the whole scene faded to black -- which reads as the effect
# dropping out and is not detectable from the HUD, because the HUD is the one thing
# still being drawn. Hold it frontmost for the whole recording.
keeper = subprocess.Popen(
    ["bash", "-c",
     f'for i in $(seq 1 {RECORD_SECONDS * 2}); do open -a "{APP}" >/dev/null 2>&1; sleep 0.5; done'],
    start_new_session=True)
try:
    subprocess.run(["screencapture", "-v", f"-V{RECORD_SECONDS}", "-x", str(mov)], check=True)
finally:
    keeper.terminate()
subprocess.run(["pkill", "-f", "macOS-Development/AshesOfHeaven.app"], check=False)

if not mov.is_file() or mov.stat().st_size < 100_000:
    raise SystemExit(f"no usable recording at {mov}")

for old in OUT.glob("frame_*.png"):
    old.unlink()
subprocess.run(["ffmpeg", "-loglevel", "error", "-i", str(mov),
                "-vf", f"fps={FPS_OUT}", str(OUT / "frame_%03d.png")], check=True)
frames = sorted(OUT.glob("frame_*.png"))
probe = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                        "-show_entries", "stream=width,height,r_frame_rate,nb_frames",
                        "-of", "default=noprint_wrappers=1", str(mov)],
                       capture_output=True, text=True)
print(probe.stdout.strip())
print(f"{mov} {mov.stat().st_size // 1024}KB -> {len(frames)} frames at {FPS_OUT}fps")
