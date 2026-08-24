"""Validate the four environment beds on disk, before they are imported.

Runs on plain python - no Unreal, no editor.  It exists because every property that makes a bed
work is invisible in a waveform view: whether the file is stereo at all, whether the loop seam
clicks, whether the two channels are actually different or a mono bed pretending to be stereo.
The same checks apply to a sound designer's real recording, which is the point: drop the WAV in
Content/Ashes/Audio/Raw and run this before ReimportAmbience.py.

    python3 Scripts/CheckAmbienceBeds.py
"""

from __future__ import annotations

import math
import struct
import sys
import wave
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1] / "Content/Ashes/Audio/Raw"

BEDS = (
    "SC_Erebus_Ambience",
    "SC_Transit_Ambience",
    "SC_Cathedral_Ambience",
    "SC_Manticore_Engine",
)

MINIMUM_SECONDS = 55.0
EXPECTED_RATE = 48_000
EXPECTED_CHANNELS = 2
FULL_SCALE = 32767
# Above this the two channels carry the same signal and the bed is mono in a stereo container.
MAX_CHANNEL_CORRELATION = 0.99
# A loop seam is a single sample step. Allow it to be a few times a normal step, but not a jump
# an order of magnitude past what the signal does everywhere else - that is an audible click.
MAX_SEAM_RATIO = 8.0


def mean(values) -> float:
    return sum(values) / len(values)


def rms(values) -> float:
    return math.sqrt(mean([v * v for v in values]))


def correlation(left, right) -> float:
    ml, mr = mean(left), mean(right)
    dl = [v - ml for v in left]
    dr = [v - mr for v in right]
    spread = rms(dl) * rms(dr)
    if spread <= 0.0:
        return 1.0
    return mean([a * b for a, b in zip(dl, dr)]) / spread


def check(name: str) -> list[str]:
    path = ROOT / f"{name}.wav"
    if not path.is_file():
        return [f"{name}: missing {path}"]

    with wave.open(str(path)) as source:
        channels = source.getnchannels()
        rate = source.getframerate()
        width = source.getsampwidth()
        frames = source.getnframes()
        if width != 2:
            return [f"{name}: {width * 8}-bit source; these checks assume 16-bit PCM"]
        raw = struct.unpack(f"<{frames * channels}h", source.readframes(frames))

    problems = []
    duration = frames / rate
    if channels != EXPECTED_CHANNELS:
        problems.append(f"{name}: {channels}ch, beds must be {EXPECTED_CHANNELS}ch (mono collapses to the centre in 2D playback)")
    if rate != EXPECTED_RATE:
        problems.append(f"{name}: {rate}Hz, expected {EXPECTED_RATE}Hz")
    if duration < MINIMUM_SECONDS:
        problems.append(f"{name}: {duration:.2f}s, under the {MINIMUM_SECONDS:.0f}s floor - the loop stays audible")

    peak = max(max(raw), -min(raw))
    clipped = sum(1 for v in raw if abs(v) >= FULL_SCALE - 8)
    if clipped:
        problems.append(f"{name}: {clipped} clipped samples")

    if channels == EXPECTED_CHANNELS:
        left = raw[0::2]
        right = raw[1::2]
        corr = correlation(left, right)
        if corr > MAX_CHANNEL_CORRELATION:
            problems.append(f"{name}: channel correlation {corr:.4f} - stereo container, mono content")

        # Sample-to-sample step across the loop join, against a sparse sample of normal steps.
        steps = [abs(left[i + 1] - left[i]) for i in range(0, len(left) - 1, 97)]
        typical = mean(steps)
        seam = abs(left[0] - left[-1])
        if typical > 0.0 and seam > typical * MAX_SEAM_RATIO:
            problems.append(f"{name}: loop seam step {seam} is {seam / typical:.1f}x a normal step - audible click")

        print(
            f"{name:<24} {channels}ch@{rate}Hz {duration:5.2f}s "
            f"peak={100.0 * peak / FULL_SCALE:5.1f}%FS rms={rms(left):6.0f} "
            f"corr={corr:.3f} seam={seam / typical:4.1f}x"
        )
    return problems


if __name__ == "__main__":
    failures = [problem for bed in BEDS for problem in check(bed)]
    for problem in failures:
        print(f"FAIL {problem}", file=sys.stderr)
    print(f"{len(BEDS) - len({p.split(':')[0] for p in failures})}/{len(BEDS)} beds pass")
    sys.exit(1 if failures else 0)
