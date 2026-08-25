#!/usr/bin/env python3
"""Score a packaged-build recording for periodicity the game is actually responsible for.

WHY THIS IS DIFFERENTIAL. An earlier version autocorrelated the fire region's brightness
in absolute terms and reported a confident r=0.69-0.76 peak at 0.95s across four builds.
It was measuring `screencapture -v`'s own encoding cadence: a perfectly static HUD region
scores r=0.56 and an empty skyline r=0.71 at the SAME lag in the same recording. Any
absolute number from this pipeline is that artifact plus signal, so the only trustworthy
figure is the EXCESS over a control region the effect does not touch.

  fire_r      autocorrelation of the fire ROI       (signal + capture artifact)
  control_r   autocorrelation of a static ROI       (capture artifact alone)
  excess      fire_r - control_r at the same lag    (what the game contributes)

WHY FRAMES ARE GATED, NOT DELETED. The packaged window renders a BLACK 3D scene whenever
it is not frontmost while Slate keeps drawing the HUD, so a lost-focus frame looks exactly
like the effect vanishing and a HUD-based check cannot see it by construction. Those
frames are excluded - but they are excluded IN PLACE, keeping their original index. An
earlier version compacted the survivors into a dense list and kept using the whole
recording's fps, which inflated every reported period by 1/(1 - discard rate) and stitched
separate dropout runs together across the holes. On a synthetic 1.20s signal that made the
same footage report 1.20s, 0.85s or 0.75s purely according to how often the gate fired.
Lags are therefore counted on the ORIGINAL timeline and only frame pairs where both ends
survived contribute.

Usage: ScoreErebusMotion.py <recording.mov> [--fire x0 y0 x1 y1]
                                            [--control x0 y0 x1 y1] [--gate x0 y0 x1 y1]
"""
import glob
import math
import os
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image

args = sys.argv[1:]


def selftest():
    """Prove the lag axis is invariant to how often the focus gate fires.

    The bug this guards against shipped once: compacting the surviving frames into a dense
    list while still using the whole recording's fps made the SAME footage report 1.20s,
    0.90s or 0.70s purely according to the discard rate.
    """
    import random
    fps_t, period, n = 20.0, 1.20, 600
    random.seed(1)
    base = [math.exp(-((i % int(period * fps_t)) - 3) ** 2 / 6.0) for i in range(n)]
    sig = [b + random.gauss(0, 0.25) + 0.15 * math.sin(2 * math.pi * i / (11 * fps_t))
           for i, b in enumerate(base)]
    for rate in (0.0, 0.25, 0.40):
        random.seed(7)
        ok = [random.random() > rate for _ in range(n)]
        kept = [v for v, k in zip(sig, ok) if k]
        mean = sum(kept) / len(kept)
        sd = math.sqrt(sum((v - mean) ** 2 for v in kept) / len(kept))
        best, best_r = None, -2.0
        for lag in range(3, int(fps_t * 4.0)):
            pairs = [(sig[i] - mean) * (sig[i + lag] - mean)
                     for i in range(n - lag) if ok[i] and ok[i + lag]]
            if len(pairs) >= 20:
                r = sum(pairs) / (len(pairs) * sd * sd)
                if r > best_r:
                    best, best_r = lag, r
        got = best / fps_t
        assert abs(got - period) < 0.09, f"discard {rate:.0%}: reported {got:.2f}s, expected {period:.2f}s"
        print(f"  discard {rate:5.0%} -> {got:.2f}s  OK")
    print("selftest passed: lag axis is gate-rate invariant")


if args and args[0] == "--selftest":
    selftest()
    raise SystemExit(0)
if not args:
    raise SystemExit(__doc__)
mov = Path(args[0])
if not mov.is_file():
    raise SystemExit(f"no recording at {mov}")


def grab(flag, default):
    if flag in args:
        i = args.index(flag)
        return tuple(int(v) for v in args[i + 1:i + 5])
    return default


# Defaults are for a full-screen capture on a 3456x2234 backing store. They are validated
# against the real frame below rather than trusted: PIL's crop ZERO-PADS an out-of-range
# box instead of raising, so a smaller capture would silently mix a constant black block
# into the series and quietly change every number.
FIRE = grab("--fire", (300, 1260, 780, 1760))
CONTROL = grab("--control", (1500, 200, 2300, 700))
GATE = grab("--gate", (2300, 800, 3200, 1600))

info = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                       "-show_entries", "format=duration:stream=nb_frames",
                       "-of", "default=noprint_wrappers=1:nokey=1", str(mov)],
                      capture_output=True, text=True).stdout.split()
nb, dur = int(info[0]), float(info[1])
fps = nb / dur


def series(frames, roi):
    return [sum(Image.open(f).crop(roi).convert("L").resize((96, 100)).get_flattened_data()) / 9600
            for f in frames]


with tempfile.TemporaryDirectory() as tmp:
    subprocess.run(["ffmpeg", "-loglevel", "error", "-i", str(mov), "-vsync", "0",
                    os.path.join(tmp, "f_%04d.png")], check=True)
    frames = sorted(glob.glob(os.path.join(tmp, "f_*.png")))
    if not frames:
        raise SystemExit(f"ffmpeg extracted no frames from {mov}")
    width, height = Image.open(frames[0]).size
    for name, box in (("--fire", FIRE), ("--control", CONTROL), ("--gate", GATE)):
        if box[0] < 0 or box[1] < 0 or box[2] > width or box[3] > height or box[0] >= box[2] or box[1] >= box[3]:
            raise SystemExit(
                f"{name} box {box} does not fit this {width}x{height} recording. PIL would "
                f"zero-pad it and the numbers would be silently wrong. Pass {name} explicitly.")
    fire, control, gate = series(frames, FIRE), series(frames, CONTROL), series(frames, GATE)

gmed = statistics.median(gate)
valid = [g > gmed * 0.55 for g in gate]
dropped = valid.count(False)
print(f"{mov.name}: {len(fire)} frames, {dur:.2f}s, {fps:.2f}fps")
print(f"  focus gate: excluded {dropped}/{len(gate)} frames ({100 * dropped / len(gate):.1f}%) "
      f"as lost-focus blackouts (indices preserved)")
if len(fire) - dropped < 30:
    raise SystemExit("too few surviving frames to score; re-record with the window frontmost")


def acf(values):
    """Autocorrelation on the ORIGINAL timeline: lag is in real frames, and a pair only
    contributes when both of its ends survived the focus gate."""
    kept = [v for v, ok in zip(values, valid) if ok]
    mean = sum(kept) / len(kept)
    sd = math.sqrt(sum((v - mean) ** 2 for v in kept) / len(kept))
    out = {}
    for lag in range(max(2, int(fps * 0.15)), min(len(values) - 10, int(fps * 4.0))):
        pairs = [(values[i] - mean) * (values[i + lag] - mean)
                 for i in range(len(values) - lag) if valid[i] and valid[i + lag]]
        if len(pairs) >= 20:
            out[lag] = sum(pairs) / (len(pairs) * sd * sd)
    return mean, sd, out


fm, fsd, flags = acf(fire)
cm, csd, clags = acf(control)
shared = sorted(set(flags) & set(clags))
if not shared:
    raise SystemExit("no lag had enough surviving frame pairs to score")
excess = sorted(((flags[l] - clags[l], l, flags[l], clags[l]) for l in shared), reverse=True)

print(f"  fire    roi={FIRE}  mean {fm:6.2f} sd {fsd:5.2f} swing {100 * fsd / fm:5.1f}%")
print(f"  control roi={CONTROL}  mean {cm:6.2f} sd {csd:5.2f} swing {100 * csd / cm:5.1f}%")
print("  strongest EXCESS periodicity (fire minus control, same lag):")
for e, lag, fr, cr in excess[:5]:
    print(f"    excess={e:+.3f}  lag {lag:3d} ({lag / fps:.2f}s)   fire r={fr:.3f}  control r={cr:.3f}")
top = excess[0][0]
print(f"  periodicity: {'LOOPING' if top > 0.35 else 'suspect' if top > 0.20 else 'aperiodic'} "
      f"(excess={top:+.3f})")

# DROPOUT: distinct failure from periodicity - the effect intermittently vanishes. Runs are
# measured on the original timeline, so a gated-out frame BREAKS a run rather than joining
# two separate ones into a longer fake one.
lit = [v for v, ok in zip(fire, valid) if ok]
med = statistics.median(lit)
dim = [i for i, (v, ok) in enumerate(zip(fire, valid)) if ok and v < med * 0.45]
runs, longest = [], 0.0
if dim:
    start = prev = dim[0]
    for i in dim[1:]:
        if i != prev + 1:
            runs.append(prev - start + 1)
            start = i
        prev = i
    runs.append(prev - start + 1)
    longest = max(runs) / fps
q = statistics.quantiles(lit, n=10)
print(f"  dropout: {len(dim)} of {len(lit)} scored frames below 45% of median "
      f"({100 * len(dim) / len(lit):.1f}%), longest run {longest:.2f}s")
print(f"           p10 {q[0]:.1f}  median {med:.1f}  p90 {q[8]:.1f}  "
      f"{'BIMODAL' if q[0] < med * 0.45 else 'unimodal'}")
