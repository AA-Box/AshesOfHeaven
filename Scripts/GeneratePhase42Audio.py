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


def write_ambience(name: str, length: float, renderer, fade: float = 3.0) -> None:
    """Write a bed that survives being played on repeat for a whole chapter.

    Three things make a loop audible, and all three were present in the 8 second beds:
    the loop was short enough to memorise, the event layers ran on a fixed modulo so the
    same pattern landed in the same place every pass, and the noise layer had different
    values at the head and the tail so the seam clicked.  This renders length+fade seconds
    and equal-power crossfades the overhang back over the opening, so the join is
    continuous in both the tonal and the noise layers.  The renderers take care of the
    other two by quantising their partials to the loop and scheduling events irregularly.
    """
    ROOT.mkdir(parents=True, exist_ok=True)
    rng = random.Random(name)
    total = int(length * RATE)
    overlap = int(fade * RATE)
    samples = [renderer(index / RATE, length, rng) for index in range(total + overlap)]

    for index in range(overlap):
        # cos/sin equal-power pair: constant energy through the seam, no dip, no click.
        theta = (index / overlap) * (math.pi / 2.0)
        samples[index] = samples[index] * math.sin(theta) + samples[total + index] * math.cos(theta)

    frames = b"".join(
        struct.pack("<h", int(max(-1.0, min(1.0, value)) * 32767.0)) for value in samples[:total]
    )
    with wave.open(str(ROOT / f"{name}.wav"), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(frames)


def wrap_tone(hz: float, t: float, length: float, phase: float = 0.0) -> float:
    """A sine snapped to the nearest whole number of cycles in the loop, so it wraps in phase."""
    cycles = max(1.0, round(hz * length))
    return math.sin((math.tau * cycles * t / length) + phase)


def event_times(seed: str, length: float, mean_gap: float, spread: float) -> tuple[float, ...]:
    """Irregular one-shot schedule across the whole bed.

    A `t % period` train repeats on a beat the ear locks onto within two passes.  These
    gaps are drawn once, deterministically, and never line up with the loop length.
    """
    rng = random.Random(f"{seed}:schedule")
    times = []
    cursor = rng.uniform(0.0, mean_gap)
    while cursor < length:
        times.append(cursor)
        cursor += max(0.35, rng.gauss(mean_gap, spread))
    return tuple(times)


def bursts(t: float, times: tuple[float, ...], width: float) -> float:
    """Gaussian envelope over the nearest scheduled event; zero cost away from one."""
    value = 0.0
    for start in times:
        local = t - start
        if -width * 3.0 < local < width * 3.0:
            value += math.exp(-(local * local) / (width * width))
    return value


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


def melee_sound(t: float, length: float, rng: random.Random) -> float:
    """Short, close-range body impact with a transient scrape layer."""
    transient = math.exp(-t * 82.0) * (0.38 * noise(rng) + 0.24 * tone(1460.0, t))
    body = math.exp(-t * 15.0) * (0.42 * tone(58.0, t) + 0.18 * tone(116.0, t))
    scrape = math.exp(-max(0.0, t - 0.035) * 24.0) * 0.12 * noise(rng)
    return transient + body + scrape


def hurt_sound(t: float, length: float, rng: random.Random) -> float:
    """Radio-filtered pain response; deliberately not interchangeable with impacts."""
    gate = 0.5 + 0.5 * tone(18.0, t)
    voice_band = 0.16 * tone(310.0 + 35.0 * tone(2.2, t), t)
    burst = math.exp(-t * 9.0) * (0.11 * noise(rng) + voice_band)
    return envelope(t, length, 0.006, 0.09) * gate * burst


def armor_sound(t: float, length: float, rng: random.Random) -> float:
    """Hard ceramic/metal ring for armor absorption."""
    ring = math.exp(-t * 8.0) * (0.27 * tone(880.0, t) + 0.18 * tone(1760.0, t))
    strike = math.exp(-t * 75.0) * (0.34 * noise(rng) + 0.16 * tone(2600.0, t))
    return ring + strike


def death_sound(t: float, length: float, rng: random.Random) -> float:
    """Low descending terminal cue with a clipped radio tail."""
    sweep = math.sin(math.tau * (420.0 * t - 220.0 * t * t))
    sub = 0.24 * tone(54.0, t) * math.exp(-t * 5.0)
    radio = 0.08 * noise(rng) * math.exp(-t * 12.0)
    return envelope(t, length, 0.02, 0.16) * (0.34 * sweep * math.exp(-t * 4.2) + sub + radio)


def grenade_sound(t: float, length: float, rng: random.Random) -> float:
    """Wide blast/air displacement layer for grenade events."""
    blast = math.exp(-t * 7.0) * (0.46 * noise(rng) + 0.30 * tone(42.0, t) + 0.16 * tone(86.0, t))
    crack = math.exp(-t * 48.0) * (0.22 * noise(rng) + 0.14 * tone(1180.0, t))
    tail = math.exp(-t * 2.5) * 0.07 * noise(rng)
    return blast + crack + tail


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


EREBUS_ARTILLERY = None
EREBUS_DEBRIS = None
TRANSIT_RELAYS = None
CATHEDRAL_PULSES = None


def erebus_ambience(t: float, length: float, rng: random.Random) -> float:
    distant = 0.08 * wrap_tone(43.0, t, length) + 0.045 * wrap_tone(87.0, t, length)
    # Two wind envelopes on coprime cycle counts: the pair only realigns once per loop, so the
    # bed never settles into a rhythm the ear can follow.
    swell = 0.65 + 0.22 * wrap_tone(0.19, t, length) + 0.13 * wrap_tone(0.071, t, length)
    wind = 0.07 * noise(rng) * swell
    artillery = 0.15 * bursts(t, EREBUS_ARTILLERY, 0.20) * (noise(rng) + 0.35 * wrap_tone(61.0, t, length))
    debris = 0.05 * bursts(t, EREBUS_DEBRIS, 0.06) * noise(rng)
    return distant + wind + artillery + debris


def transit_ambience(t: float, length: float, rng: random.Random) -> float:
    electrical = 0.09 * wrap_tone(59.0, t, length) + 0.035 * wrap_tone(118.0, t, length)
    rail = 0.08 * noise(rng) * (0.5 + 0.32 * wrap_tone(0.11, t, length) + 0.18 * wrap_tone(0.043, t, length))
    relay = 0.08 * bursts(t, TRANSIT_RELAYS, 0.09) * wrap_tone(740.0, t, length)
    return electrical + rail + relay


def cathedral_ambience(t: float, length: float, rng: random.Random) -> float:
    resonance = (
        0.08 * wrap_tone(31.0, t, length)
        + 0.06 * wrap_tone(62.0, t, length)
        + 0.025 * wrap_tone(137.0, t, length)
    )
    air = 0.04 * noise(rng)
    pulse = 0.07 * bursts(t, CATHEDRAL_PULSES, 0.30) * wrap_tone(211.0, t, length)
    return resonance + air + pulse


def manticore_engine(t: float, length: float, rng: random.Random) -> float:
    # The RPM wobble has to wrap too, or the engine audibly re-pitches at the seam.
    rpm_cycles = max(1.0, round(38.0 * length))
    wobble = 4.0 * wrap_tone(0.13, t, length) + 1.5 * wrap_tone(0.037, t, length)
    phase = math.tau * (rpm_cycles * t / length + wobble * t / length)
    engine = 0.15 * math.sin(phase) + 0.09 * math.sin(2.0 * phase) + 0.04 * math.sin(3.0 * phase)
    machinery = 0.08 * noise(rng) * (0.5 + 0.30 * wrap_tone(0.27, t, length) + 0.20 * wrap_tone(0.083, t, length))
    return engine + machinery


SOURCES = {
    "SC_M91_Fire": (0.65, m91_fire),
    "SC_M91_Reload": (0.95, reload_sound),
    "SC_M91_Empty": (0.20, empty_sound),
    "SC_M91_Impact": (0.45, impact_sound),
    "SC_Combat_Melee": (0.36, melee_sound),
    "SC_Combat_Hurt": (0.42, hurt_sound),
    "SC_Combat_Armor": (0.48, armor_sound),
    "SC_Combat_Death": (0.72, death_sound),
    "SC_Combat_Grenade": (0.95, grenade_sound),
    "SC_UI_Objective": (0.72, objective_sound),
    "SC_UI_Dialogue": (0.40, dialogue_sound),
    "SC_UI_Pickup": (0.36, pickup_sound),
    "SC_Player_Footstep": (0.22, footstep_sound),
}

# Beds are played on repeat for as long as a chapter stage lasts. Eight seconds was short
# enough to memorise on the second pass; a minute reads as continuous atmosphere.
AMBIENCE_LENGTH = 60.0

AMBIENCE = {
    "SC_Erebus_Ambience": erebus_ambience,
    "SC_Transit_Ambience": transit_ambience,
    "SC_Cathedral_Ambience": cathedral_ambience,
    "SC_Manticore_Engine": manticore_engine,
}


if __name__ == "__main__":
    span = AMBIENCE_LENGTH + 3.0
    EREBUS_ARTILLERY = event_times("erebus.artillery", span, 5.1, 2.2)
    EREBUS_DEBRIS = event_times("erebus.debris", span, 2.3, 1.1)
    TRANSIT_RELAYS = event_times("transit.relay", span, 3.4, 1.6)
    CATHEDRAL_PULSES = event_times("cathedral.pulse", span, 6.7, 2.9)

    for source_name, (duration, renderer) in SOURCES.items():
        write_wave(source_name, duration, renderer)
    for source_name, renderer in AMBIENCE.items():
        write_ambience(source_name, AMBIENCE_LENGTH, renderer)
    print(f"generated {len(SOURCES) + len(AMBIENCE)} static source WAV files in {ROOT}")
