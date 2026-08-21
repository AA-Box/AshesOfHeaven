"""Generate the checked-in Phase 4.2 source audio palette.

These are static source recordings for the prototype presentation target.  They are deliberately
different, layered, and imported by Unreal as SoundWave assets; the game never synthesizes these
signals at runtime.  A sound designer can replace the WAVs without changing semantic event
names or gameplay code.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1] / "Content/Ashes/Audio/Raw"
RATE = 48_000


def envelope(t: float, length: float, attack: float = 0.004, release: float = 0.12) -> float:
    return min(1.0, t / max(attack, 1e-5)) * min(1.0, (length - t) / max(release, 1e-5))


def noise(rng: random.Random) -> float:
    return rng.uniform(-1.0, 1.0)


def tone(hz: float, t: float, phase: float = 0.0) -> float:
    return math.sin((math.tau * hz * t) + phase)


def write_wave(name: str, length: float, renderer) -> None:
    ROOT.mkdir(parents=True, exist_ok=True)
    rng = random.Random(name)
    frames = []
    total = int(length * RATE)
    for index in range(total):
        t = index / RATE
        value = max(-1.0, min(1.0, renderer(t, length, rng)))
        frames.append(struct.pack("<h", int(value * 32767.0)))
    with wave.open(str(ROOT / f"{name}.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(b"".join(frames))


def m91_fire(t: float, length: float, rng: random.Random) -> float:
    body = math.exp(-t * 16.0) * (0.58 * tone(66.0 + 18.0 * math.exp(-t * 24.0), t))
    crack = math.exp(-t * 92.0) * (0.28 * noise(rng) + 0.18 * tone(1840.0, t))
    mechanical = math.exp(-t * 38.0) * 0.12 * tone(410.0, t)
    tail = math.exp(-t * 5.5) * 0.07 * tone(118.0, t)
    return body + crack + mechanical + tail


def reload_sound(t: float, length: float, rng: random.Random) -> float:
    clicks = 0.0
    for start, frequency, gain in ((0.05, 1900.0, 0.55), (0.22, 1120.0, 0.42), (0.43, 2350.0, 0.48), (0.69, 820.0, 0.36)):
        local = max(0.0, t - start)
        clicks += math.exp(-local * 52.0) * gain * (tone(frequency, local) + 0.25 * noise(rng))
    scrape = math.exp(-max(0.0, t - 0.28) * 3.2) * 0.10 * noise(rng)
    return clicks + scrape


def empty_sound(t: float, length: float, rng: random.Random) -> float:
    return math.exp(-t * 30.0) * (0.58 * tone(530.0, t) + 0.18 * tone(1680.0, t) + 0.12 * noise(rng))


def impact_sound(t: float, length: float, rng: random.Random) -> float:
    return math.exp(-t * 11.0) * (0.52 * noise(rng) + 0.32 * tone(76.0, t) + 0.18 * tone(232.0, t))


def objective_sound(t: float, length: float, rng: random.Random) -> float:
    notes = ((0.02, 420.0), (0.20, 630.0), (0.38, 940.0))
    value = 0.0
    for start, frequency in notes:
        local = t - start
        if local >= 0.0:
            value += math.exp(-local * 7.0) * 0.22 * (tone(frequency, local) + 0.35 * tone(frequency * 2.01, local))
    return value


def dialogue_sound(t: float, length: float, rng: random.Random) -> float:
    radio_gate = 0.5 + 0.5 * tone(7.0, t)
    texture = 0.11 * noise(rng) + 0.05 * tone(178.0, t) + 0.03 * tone(356.0, t)
    return envelope(t, length, 0.02, 0.16) * radio_gate * texture


def pickup_sound(t: float, length: float, rng: random.Random) -> float:
    first = max(0.0, t - 0.02)
    second = max(0.0, t - 0.17)
    return 0.24 * math.exp(-first * 8.0) * tone(520.0, first) + 0.18 * math.exp(-second * 9.0) * tone(780.0, second)


def footstep_sound(t: float, length: float, rng: random.Random) -> float:
    return math.exp(-t * 32.0) * (0.66 * noise(rng) + 0.26 * tone(74.0, t) + 0.12 * tone(142.0, t))


def erebus_ambience(t: float, length: float, rng: random.Random) -> float:
    distant = 0.08 * tone(43.0, t) + 0.045 * tone(87.0, t)
    wind = 0.07 * noise(rng) * (0.65 + 0.35 * tone(0.19, t))
    artillery = 0.15 * math.exp(-((t % 3.7) - 1.9) ** 2 * 24.0) * (noise(rng) + 0.35 * tone(61.0, t))
    return distant + wind + artillery


def transit_ambience(t: float, length: float, rng: random.Random) -> float:
    electrical = 0.09 * tone(59.0, t) + 0.035 * tone(118.0, t)
    rail = 0.08 * noise(rng) * (0.5 + 0.5 * tone(0.11, t))
    relay = 0.08 * math.exp(-((t % 2.6) - 0.7) ** 2 * 34.0) * tone(740.0, t)
    return electrical + rail + relay


def cathedral_ambience(t: float, length: float, rng: random.Random) -> float:
    resonance = 0.08 * tone(31.0, t) + 0.06 * tone(62.0, t) + 0.025 * tone(137.0, t)
    air = 0.04 * noise(rng)
    pulse = 0.07 * math.exp(-((t % 4.8) - 2.0) ** 2 * 11.0) * tone(211.0, t)
    return resonance + air + pulse


def manticore_engine(t: float, length: float, rng: random.Random) -> float:
    rpm = 38.0 + 4.0 * tone(0.13, t)
    engine = 0.15 * tone(rpm, t) + 0.09 * tone(rpm * 2.0, t) + 0.04 * tone(rpm * 3.0, t)
    machinery = 0.08 * noise(rng) * (0.5 + 0.5 * tone(0.27, t))
    return engine + machinery


SOURCES = {
    "SC_M91_Fire": (0.65, m91_fire),
    "SC_M91_Reload": (0.95, reload_sound),
    "SC_M91_Empty": (0.20, empty_sound),
    "SC_M91_Impact": (0.45, impact_sound),
    "SC_UI_Objective": (0.72, objective_sound),
    "SC_UI_Dialogue": (0.40, dialogue_sound),
    "SC_UI_Pickup": (0.36, pickup_sound),
    "SC_Player_Footstep": (0.22, footstep_sound),
    "SC_Erebus_Ambience": (8.0, erebus_ambience),
    "SC_Transit_Ambience": (8.0, transit_ambience),
    "SC_Cathedral_Ambience": (8.0, cathedral_ambience),
    "SC_Manticore_Engine": (8.0, manticore_engine),
}


if __name__ == "__main__":
    for source_name, (duration, renderer) in SOURCES.items():
        write_wave(source_name, duration, renderer)
    print(f"generated {len(SOURCES)} static source WAV files in {ROOT}")
