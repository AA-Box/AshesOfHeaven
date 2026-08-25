#!/usr/bin/env python3
"""Score a packaged-build recording for periodicity the game is actually responsible for.

WHY THIS IS DIFFERENTIAL. The first version of this script autocorrelated the fire
region's brightness in absolute terms and reported a confident r=0.69-0.76 peak at
0.95s across four different builds. It was measuring `screencapture -v`'s own encoding
cadence: a perfectly static HUD region scores r=0.56 and an empty skyline r=0.71 at the
SAME lag in the same recording. Any absolute number from this pipeline is that artifact
plus signal, so the only trustworthy figure is the EXCESS over a static control.

  fire_r      autocorrelation of the fire ROI          (signal + capture artifact)
  control_r   autocorrelation of a static ROI          (capture artifact alone)
  excess      fire_r - control_r at the same lag       (what the game contributes)

WHY FRAMES ARE DROPPED. The packaged window renders a BLACK 3D scene whenever it is
not frontmost, while Slate keeps drawing the HUD -- so a lost-focus frame looks like
the effect vanishing, and a HUD-based sanity check cannot see it, because the HUD is
the one thing still drawn. On a machine running several sessions something steals
focus every ~2s and ~25% of frames came back black. Holding the app frontmost did not
stop it, so instead every frame is gated on a SCENE region far from the effect: if
that region is dark, the whole frame is a focus artifact and is discarded rather than
being counted as the effect dropping out.

Usage: motion_metric.py <recording.mov> [--fire x0 y0 x1 y1] [--control x0 y0 x1 y1]
"""
import glob, math, os, statistics, subprocess, sys, tempfile
from pathlib import Path
from PIL import Image

args = sys.argv[1:]
mov = Path(args[0])
def grab(flag, default):
    if flag in args:
        i = args.index(flag)
        return tuple(int(v) for v in args[i + 1:i + 5])
    return default
FIRE = grab("--fire", (300, 1260, 780, 1760))
CONTROL = grab("--control", (1500, 200, 2300, 700))   # distant skyline: no particles
if not mov.is_file():
    raise SystemExit(f"no recording at {mov}")

info = subprocess.run(["ffprobe", "-v", "error", "-select_streams", "v:0",
                       "-show_entries", "format=duration:stream=nb_frames",
                       "-of", "default=noprint_wrappers=1:nokey=1", str(mov)],
                      capture_output=True, text=True).stdout.split()
nb, dur = int(info[0]), float(info[1])
fps = nb / dur

def series(frames, roi):
    out = []
    for f in frames:
        im = Image.open(f).crop(roi).convert("L").resize((96, 100))
        out.append(sum(im.get_flattened_data()) / (96 * 100))
    return out

with tempfile.TemporaryDirectory() as tmp:
    subprocess.run(["ffmpeg", "-loglevel", "error", "-i", str(mov), "-vsync", "0",
                    os.path.join(tmp, "f_%04d.png")], check=True)
    frames = sorted(glob.glob(os.path.join(tmp, "f_*.png")))
    # Focus gate: a mid-scene region with no particles in it. Black => window lost focus.
    GATE = (2300, 800, 3200, 1600)
    gate = series(frames, GATE)
    gmed = statistics.median(gate)
    keep = [i for i, g in enumerate(gate) if g > gmed * 0.55]
    dropped = len(frames) - len(keep)
    frames = [frames[i] for i in keep]
    fire, control = series(frames, FIRE), series(frames, CONTROL)
    print(f"  focus gate: discarded {dropped}/{len(gate)} frames "
          f"({100*dropped/len(gate):.1f}%) as lost-focus blackouts")

def acf(s):
    n = len(s); m = sum(s) / n
    c = [v - m for v in s]; den = sum(x * x for x in c)
    sd = math.sqrt(den / n)
    lags = {}
    for lag in range(max(2, int(fps * 0.15)), min(n - 10, int(fps * 4.0))):
        lags[lag] = sum(c[i] * c[i + lag] for i in range(n - lag)) / den
    return m, sd, lags

fm, fsd, flags = acf(fire)
cm, csd, clags = acf(control)
excess = sorted(((flags[l] - clags[l], l, flags[l], clags[l]) for l in flags), reverse=True)

print(f"{mov.name}: {len(fire)} frames, {dur:.2f}s, {fps:.2f}fps")
print(f"  fire    roi={FIRE}  mean {fm:6.2f} sd {fsd:5.2f} swing {100*fsd/fm:5.1f}%")
print(f"  control roi={CONTROL}  mean {cm:6.2f} sd {csd:5.2f} swing {100*csd/cm:5.1f}%")
print("  strongest EXCESS periodicity (fire minus control, same lag):")
for e, lag, fr, cr in excess[:5]:
    print(f"    excess={e:+.3f}  lag {lag:3d} ({lag/fps:.2f}s)   fire r={fr:.3f}  control r={cr:.3f}")
top = excess[0][0]
print(f"  periodicity: {'LOOPING' if top > 0.35 else 'suspect' if top > 0.20 else 'aperiodic'} (excess={top:+.3f})")

# DROPOUT. Distinct failure from periodicity: the effect intermittently vanishes.
# A healthy fire varies smoothly; a culled one is bimodal, sitting near zero for
# stretches. Measured 25.7% dim at p10=9.9 against median 42.7 before the
# EmitterState distance-cull override.
med = statistics.median(fire)
dim = [i for i, x in enumerate(fire) if x < med * 0.45]
runs, longest = [], 0
if dim:
    st = pr = dim[0]
    for i in dim[1:]:
        if i != pr + 1:
            runs.append(pr - st + 1); st = i
        pr = i
    runs.append(pr - st + 1)
    longest = max(runs) / fps
q = statistics.quantiles(fire, n=10)
print(f"  dropout: {len(dim)} of {len(fire)} frames below 45% of median "
      f"({100*len(dim)/len(fire):.1f}%), longest run {longest:.2f}s")
print(f"           p10 {q[0]:.1f}  median {med:.1f}  p90 {q[8]:.1f}  "
      f"{'BIMODAL - effect is being culled' if q[0] < med*0.45 else 'unimodal - healthy'}")
