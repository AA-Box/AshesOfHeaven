"""Capture the fixed Erebus review poses from the packaged build and measure them.

The Erebus visual gate is judged on numbers as well as on the frames, because "too dark" and
"the materials do nothing" are both measurable and both were argued about from impressions before.

Round 4 fixed two measurement bugs that had been quietly deciding the verdict:

  * Framing. `screencapture -R95,95,1300,800` was capturing a 54% left-biased CROP of the frame,
    not the frame. The game runs fullscreen (1728x1117 points on this display, 2x backing), so the
    old region cut the right 19% and bottom 20% - which is where the HUD and the two largest fires
    are. Whole-frame numbers now come from the whole frame. `legacy_crop_*` reproduces the old
    rectangle so the previous passes remain comparable instead of being silently reinterpreted.

  * Whole-frame crushed% cannot tell "authored detail collapsed to black" from "a building
    silhouette is deliberately black". Shot 2 proved it: the frame became obviously more readable
    while crushed% moved 75.4 -> 76.3, because a 1.5x-scaled blast wall sits ~160uu in front of
    that camera. The director now projects named world boxes into normalised viewport space and
    logs them, and each region is scored on its own.

Per-region numbers are the acceptance criteria the frame average never could be:

    road, facade_left/right   median V - the things the player has to be able to walk and fight in
    hero_wreck, gate          median V - the battlefield identity props
    aperture                  median V - must STAY bright; the composition depends on it
    cathedral                 median V and contrast against its surround, for landmark separation
    rust_pipe_*, red_sign     sat% - proves a rust/red object is IN FRAME before its chroma is
                              called broken
    combatant                 median V - character material and fill light, unmeasured until a
                              capture actually has a body in it
    blastwall_fg              median V - the legitimately black foreground mass, reported so it
                              stops contaminating the frame verdict

Usage:
    python3 Scripts/ErebusAcceptance.py --out Saved/Acceptance/after2
    python3 Scripts/ErebusAcceptance.py --out Saved/Acceptance/after2 --compare Saved/Acceptance/after/metrics.json
    python3 Scripts/ErebusAcceptance.py --out Saved/Acceptance/battle --only battle

Requires the packaged Development build (CLIENT_CONFIG=Development ./Scripts/Build-Mac.sh) and Pillow.
macOS only: it drives the packaged app and `screencapture`, because there is no synthetic keyboard
or mouse input into this build on this machine - the camera is placed with -ArtCam instead.
"""

from __future__ import annotations

import argparse
import colorsys
import json
import re
import statistics
import subprocess
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
APP = REPO / "Builds/macOS-Development/AshesOfHeaven.app"
EXE = APP / "Contents/MacOS/AshesOfHeaven"
SAVE = Path.home() / (
    "Library/Containers/com.YourCompany.AshesOfHeaven/Data/Library/Application Support"
    "/Epic/AshesOfHeaven/Saved/SaveGames/AshesOfHeaven_Slot_0.sav"
)

# X, Y, Z, pitch, yaw. Fixed for the life of the gate: the whole point is comparability between
# passes, so these are never tuned to flatter a build.
POSES = {
    "shot1_route_entry": (-1800, -120, 300, -3, 0),
    "shot2_defensive_line": (600, -120, 290, -2, 0),
    "shot3_mid_route": (2600, -120, 320, -1, 0),
    # shot4_elevated was dropped: its pose asks for Z=1400 but the character falls, so the camera
    # landed at 436 one run and 685 the next. A vantage that moves between passes cannot be a
    # comparison shot.
    # The Cathedral check has to be a NORMAL viewpoint, not an art camera: this is the actual
    # Objective 01 spawn (X=-1400) at standing eye height with the camera level, which is what a
    # player sees on the first frame they get control.
    "shot5_cathedral_gameplay": (-1400, -120, 180, 0, 0),
}

# The character pass runs the Battle target - the only review path that spawns bodies - but from
# shot2's exact pose, so the world-box regions still mean what they say and the frame is directly
# comparable to shot2 with bodies added. Letting Battle pick its own vantage put the fixed regions
# somewhere else entirely and reported a burning barrel as the aperture.
EXTRA_TARGETS = {
    "shot6_battle_characters": ("Battle", (600, -120, 290, -2, 0)),
    # The encounter puts its combatants in the left-flank pocket behind the fortification row,
    # around X 1150-1330 / Y -330 to -620, which is the darkest part of the scene and which shot6
    # sees only edge-on. This pose stands on that line and looks straight down it, so there is a
    # lit body large enough in frame to actually score the material and the fill.
    "shot7_battle_closeup": ("Battle", (500, -480, 290, 0, 0)),
    # Deterministic character bench: one body, fixed spot, fixed camera, clear sight line, real
    # Erebus lighting. The target sets its own camera, so no -ArtCam.
    "shot8_combatant_bench": ("Combatant", None),
}
# Narrow window on purpose. Too short and the bodies have not reached the pose; at 30s the
# firefight is over and every candidate is a corpse lying under the floor at Z=-32, which measures
# as ground. 14s catches them upright and fighting.
BATTLE_SETTLE_SECONDS = 14

# Learned timings: the packaged build needs ~18s to reach the stage, and it renders a black scene
# while it is not the frontmost app, so it has to be fronted and then given time for auto-exposure
# to settle before the frame means anything. The director logs its ROI projections at 12s.
LOAD_SECONDS = 18
SETTLE_SECONDS = 16

# The old capture rectangle, in screen points, kept only so previous passes stay comparable.
LEGACY_REGION_POINTS = (95, 95, 1300, 800)

CRUSHED_V = 0.08
# "Blown" in 8-bit terms: V >= 250/255. The white-mannequin complaint is about this number, not
# about the median.
CLIP_V = 250.0 / 255.0
# MI_Erebus_Rust sits at hue 16.4 and MI_Erebus_PaintRed at 3.0; the fire VFX are yellower. A 15
# degree split filed the rust material itself under "fire" and is why rust% read 0.000 for three
# passes while the material was in frame and saturated.
RUST_HUE_MAX = 22.0
COLOR_S = 0.25
COLOR_V = 0.15
# Regions are small and often dim; proving a rust object is present is a different question from
# "does the frame read as colourful", so it gets a lower brightness gate.
ROI_COLOR_V = 0.10
# Downsample before the per-pixel pass: the full frame is 3456x2168 and pure python is slow.
SAMPLE_DIVISOR = 4
ROI_MIN_PIXELS = 60

ROI_LINE = re.compile(
    r"\[Phase4\]\[ArtRoi\] (\w+) corners=(\d+) x0=([-\d.]+) y0=([-\d.]+) x1=([-\d.]+) y1=([-\d.]+)")
VIEWPORT_LINE = re.compile(r"\[Phase4\]\[ArtRoi\] viewport=(\d+)x(\d+) camera=(.+?) pitch=([-\d.]+) yaw=([-\d.]+)")


def launch_and_capture(name: str, pose, target: str, out_dir: Path) -> Path:
    subprocess.run(["pkill", "-f", "macOS-Development/AshesOfHeaven.app"], check=False)
    time.sleep(2)
    SAVE.unlink(missing_ok=True)

    argv = [str(EXE), "-freshchapter", f"-ArtTarget={target}"]
    if pose is not None:
        argv.append("-ArtCam=" + ",".join(str(v) for v in pose))
    argv += ["-windowed", "-ResX=1280", "-ResY=720", "-WinX=100", "-WinY=100",
             "-stdout", "-FullStdOutLogOutput"]

    log_path = out_dir / f"{name}.log"
    with open(log_path, "wb") as log:
        subprocess.Popen(argv, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    time.sleep(LOAD_SECONDS)
    # Fronts the running instance rather than starting a second one.
    subprocess.run(["open", "-a", str(APP)], check=False)
    time.sleep(BATTLE_SETTLE_SECONDS if target.lower().startswith("battle") else SETTLE_SECONDS)

    png = out_dir / f"{name}.png"
    # Whole screen, not a region: the game is fullscreen and the old -R rectangle was a crop.
    subprocess.run(["screencapture", "-x", str(png)], check=True)
    subprocess.run(["pkill", "-f", "macOS-Development/AshesOfHeaven.app"], check=False)

    log_text = log_path.read_text(errors="replace")
    if pose is not None and "camera_override" not in log_text:
        raise SystemExit(f"{name}: -ArtCam was not applied; the frame is not the pose it claims to be")
    if "[Phase4][ArtRoi]" not in log_text:
        raise SystemExit(f"{name}: the build did not log ROI projections; rebuild before measuring")
    return png


def read_rois(log_path: Path) -> dict:
    """Normalised viewport rects the game projected, plus the view it projected them from."""
    if not log_path.is_file():
        return {}
    rois, view = {}, {}
    for line in log_path.read_text(errors="replace").splitlines():
        match = VIEWPORT_LINE.search(line)
        if match:
            view = {
                "viewport": [int(match.group(1)), int(match.group(2))],
                "camera": match.group(3),
                "pitch": float(match.group(4)),
                "yaw": float(match.group(5)),
            }
            continue
        match = ROI_LINE.search(line)
        if match:
            rois[match.group(1)] = {
                "corners": int(match.group(2)),
                "rect": [float(match.group(index)) for index in (3, 4, 5, 6)],
            }
    return {"view": view, "rois": rois}


def game_frame(png: Path):
    """The game viewport, with the black menu-bar/notch band above it removed.

    Detected rather than hardcoded: the band height depends on the display, and a wrong constant
    would silently shift every ROI.
    """
    from PIL import Image

    image = Image.open(png).convert("RGB")
    width, height = image.size
    probe = image.resize((1, height))
    top = 0
    for y in range(min(200, height)):
        if sum(probe.getpixel((0, y))) > 3:
            top = y
            break
    return image.crop((0, top, width, height))


def hsv_metrics(image, color_v: float) -> dict:
    raw = image.tobytes()
    total = len(raw) // 3
    if total == 0:
        return {}
    values, sats = [], []
    crushed = clipped = rust = fire = other_hue = 0
    for index in range(0, total * 3, 3):
        hue, sat, val = colorsys.rgb_to_hsv(raw[index] / 255, raw[index + 1] / 255, raw[index + 2] / 255)
        values.append(val)
        sats.append(sat)
        if val < CRUSHED_V:
            crushed += 1
        if val >= CLIP_V:
            clipped += 1
        if sat > COLOR_S and val > color_v:
            degrees = hue * 360.0
            if degrees < RUST_HUE_MAX or degrees >= 340.0:
                rust += 1
            elif degrees < 50.0:
                fire += 1
            else:
                other_hue += 1
    return {
        "pixels": total,
        "median_v": round(statistics.median(values), 4),
        "p90_v": round(sorted(values)[int(total * 0.90)], 4),
        "crushed_pct": round(100.0 * crushed / total, 2),
        "clip250_pct": round(100.0 * clipped / total, 3),
        "mean_s": round(statistics.mean(sats), 4),
        "rust_pct": round(100.0 * rust / total, 3),
        "fire_pct": round(100.0 * fire / total, 3),
        "other_hue_pct": round(100.0 * other_hue / total, 3),
    }


def measure(png: Path, log_path: Path) -> dict:
    frame = game_frame(png)
    width, height = frame.size
    small = frame.resize((width // SAMPLE_DIVISOR, height // SAMPLE_DIVISOR))

    result = hsv_metrics(small, COLOR_V)
    result["frame_size"] = [width, height]

    values_image = small.convert("HSV")
    raw = values_image.tobytes()
    total = len(raw) // 3
    histogram = [0] * 10
    for index in range(2, total * 3, 3):
        histogram[min(9, raw[index] * 10 // 256)] += 1
    result["histogram_deciles_pct"] = [round(100.0 * h / total, 2) for h in histogram]

    # The bright fog aperture, measured as a highlight population rather than a fixed rectangle:
    # an earlier fixed top-centre crop landed on a dark building after the level was rebuilt and
    # reported the aperture as lost while the frame plainly still had it.
    sorted_values = sorted(raw[index] / 255.0 for index in range(2, total * 3, 3))
    result["aperture_p99_v"] = round(sorted_values[int(total * 0.99)], 4)
    result["bright_pct"] = round(100.0 * sum(1 for v in sorted_values if v >= 0.70) / total, 3)

    # Same rectangle the first three passes used, so their numbers still mean something.
    scale = frame.width / 1728.0
    left, top, region_w, region_h = LEGACY_REGION_POINTS
    legacy = small.crop((
        int(left * scale) // SAMPLE_DIVISOR,
        max(0, int((top - 33) * scale)) // SAMPLE_DIVISOR,
        int((left + region_w) * scale) // SAMPLE_DIVISOR,
        int((top - 33 + region_h) * scale) // SAMPLE_DIVISOR,
    ))
    legacy_metrics = hsv_metrics(legacy, COLOR_V)
    result["legacy_crop_median_v"] = legacy_metrics.get("median_v")
    result["legacy_crop_crushed_pct"] = legacy_metrics.get("crushed_pct")

    projected = read_rois(log_path)
    result["view"] = projected.get("view", {})
    result["rois"] = {}
    for roi_name, roi in projected.get("rois", {}).items():
        x0, y0, x1, y1 = roi["rect"]
        if roi["corners"] == 0 or x1 <= 0.0 or y1 <= 0.0 or x0 >= 1.0 or y0 >= 1.0:
            result["rois"][roi_name] = {"onscreen": False}
            continue
        box = (
            int(max(0.0, x0) * small.width),
            int(max(0.0, y0) * small.height),
            int(min(1.0, x1) * small.width),
            int(min(1.0, y1) * small.height),
        )
        if (box[2] - box[0]) * (box[3] - box[1]) < ROI_MIN_PIXELS:
            result["rois"][roi_name] = {"onscreen": False, "reason": "too small on screen"}
            continue
        roi_metrics = hsv_metrics(small.crop(box), ROI_COLOR_V)
        roi_metrics["onscreen"] = True
        roi_metrics["rect"] = [round(v, 4) for v in roi["rect"]]
        result["rois"][roi_name] = roi_metrics

    body = result["rois"].get("combatant", {})
    road = result["rois"].get("road", {})
    if body.get("onscreen") and road.get("onscreen") and road.get("median_v"):
        # The white-mannequin failure was a ratio, not a level: a 92% suit beside a 2% road reads
        # as an emissive cutout however the exposure is set. Under 2.5 was the target.
        result["body_to_road_ratio"] = round(body["median_v"] / road["median_v"], 2)
        # The body box necessarily contains background between the limbs and around the silhouette,
        # so the median understates the body itself. p90 is the lit body.
        result["body_to_road_p90"] = round(body["p90_v"] / road["median_v"], 2)
    return result


def bar(percent: float) -> str:
    return "#" * int(percent / 2.0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--out", required=True, help="directory for PNGs, logs and metrics.json")
    parser.add_argument("--compare", help="a previous metrics.json to diff against")
    parser.add_argument("--measure-only", action="store_true", help="re-measure existing PNGs, do not launch the game")
    parser.add_argument("--only", help="capture one entry only, e.g. shot2_defensive_line or battle")
    args = parser.parse_args()

    out_dir = Path(args.out)
    if not out_dir.is_absolute():
        out_dir = REPO / out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    if not args.measure_only and not EXE.is_file():
        raise SystemExit(f"packaged build missing: {EXE}\nRun CLIENT_CONFIG=Development ./Scripts/Build-Mac.sh")

    work = [(name, pose, "Erebus") for name, pose in POSES.items()]
    work += [(name, pose, target) for name, (target, pose) in EXTRA_TARGETS.items()]
    if args.only:
        needle = args.only.lower()
        work = [entry for entry in work if needle in entry[0].lower() or needle == entry[2].lower()]
        if not work:
            raise SystemExit(f"--only {args.only} matched nothing")

    metrics = {}
    for name, pose, target in work:
        png = out_dir / f"{name}.png"
        log_path = out_dir / f"{name}.log"
        if not args.measure_only:
            png = launch_and_capture(name, pose, target, out_dir)
        elif not png.is_file():
            # A pose added after a baseline was captured has no old frame; skip it rather than
            # refusing to re-measure the ones that do exist.
            print(f"{name:<26} (no frame in this set, skipped)")
            continue
        metrics[name] = measure(png, log_path)
        metrics[name]["pose"] = list(pose) if pose else None
        metrics[name]["target"] = target

    (out_dir / "metrics.json").write_text(json.dumps(metrics, indent=2))

    previous = json.loads(Path(args.compare).read_text()) if args.compare else {}
    print(f"{'pose':<26} {'medianV':>8} {'crushed%':>9} {'rust%':>6} {'apertP99':>9} {'legacyMedV':>11}")
    for name, m in metrics.items():
        line = (f"{name:<26} {m['median_v']:>8.4f} {m['crushed_pct']:>9.2f} {m['rust_pct']:>6.3f}"
                f" {m['aperture_p99_v']:>9.4f} {m['legacy_crop_median_v']:>11.4f}")
        old = previous.get(name)
        if old:
            line += f"   was {old['median_v']:.4f} / {old['crushed_pct']:.2f}"
        print(line)

    print("\nper-region readability (median V, shadow clip%, saturated%):")
    for name, m in metrics.items():
        print(f"  {name}   view={m.get('view', {}).get('camera', '?')}")
        for roi_name, roi in sorted(m.get("rois", {}).items()):
            if not roi.get("onscreen"):
                print(f"    {roi_name:<16} offscreen")
                continue
            saturated = roi["rust_pct"] + roi["fire_pct"] + roi["other_hue_pct"]
            print(f"    {roi_name:<16} medV {roi['median_v']:.4f}  p90 {roi['p90_v']:.4f}"
                  f"  shadow {roi['crushed_pct']:5.1f}%  blown {roi['clip250_pct']:5.2f}%"
                  f"  meanS {roi['mean_s']:.3f}  sat {saturated:5.2f}%  rust {roi['rust_pct']:5.2f}%")
        if "body_to_road_ratio" in m:
            print(f"    {'body:road':<16} median {m['body_to_road_ratio']:.2f}x   p90 "
                  f"{m.get('body_to_road_p90', float('nan')):.2f}x  (target < 2.5)")

    print("\nbrightness histogram, percent of frame per V decile (0.0-0.1 first):")
    for name, m in metrics.items():
        print(f"  {name}")
        for index, pct in enumerate(m["histogram_deciles_pct"]):
            print(f"    V {index / 10:.1f}-{(index + 1) / 10:.1f} {pct:6.2f} {bar(pct)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
