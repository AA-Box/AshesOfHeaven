# Phase 4.2 — Audio pipeline

Gameplay calls semantic events through `UAHAudioSubsystem`; gameplay does not know whether a cue
is a SoundCue, MetaSound source, authored voice asset, or a later mix revision.

## Semantic contract

The default palette contains `Weapon.M91.Fire`, `Weapon.M91.Reload`, `Weapon.M91.Empty`,
`Weapon.M91.Impact`, combat feedback, `UI.Objective`, `UI.Dialogue`, `UI.Pickup`,
`Player.Footstep`, and environment events `Environment.Erebus`, `Environment.Transit`,
`Environment.Cathedral`, and `Environment.Manticore`.

`DA_AudioPalette_Default`, `DA_AudioPalette_Human`, and `DA_AudioPalette_Veil` are real saved
data assets. The default palette resolves to project-contained serialized `MetaSoundSource`
assets after normal-editor generation; SoundCues remain explicit graph-based compatibility
assets. Raw source WAVs are imported under `/Game/Ashes/Audio/Raw`, and every world/UI SoundCue
is routed through project attenuation, concurrency, and submix assets.

The M91 no longer points at any engine template path. Its semantic source is the project-local
`MS_M91_Fire` MetaSound, with `/Game/Ashes/Audio/Cues/SC_M91_Fire` as the explicit SoundCue route.

## Fallback policy

There is no runtime PCM generator or silent template substitution. If an authored event is absent,
playback is skipped and the missing semantic event is logged. The checked-in WAVs are static source
media for this prototype target and can be replaced by a sound designer without changing semantic
event names or gameplay code.

## Region behavior

Chapter-stage changes select and cross-fade authored environment components for Erebus, Transit,
Manticore, and Cathedral contexts. The system supports attenuation through normal SoundCue/
MetaSound authoring, concurrency limits, and separate world/UI submix routes.

## Drop-in replacement spec

Every source in the game is a synthesized placeholder from `Scripts/GeneratePhase42Audio.py`.
Replacing one needs no code, no palette edit and no asset creation: write the WAV over the file
of the same name in `Content/Ashes/Audio/Raw/` and reimport.

| Semantic id | WAV name | Length | Notes |
|---|---|---|---|
| `Environment.Erebus` | `SC_Erebus_Ambience.wav` | 60s, seamless loop | battlefield distance, wind, debris |
| `Environment.Transit` | `SC_Transit_Ambience.wav` | 60s, seamless loop | electrical decay, station tonal hum |
| `Environment.Cathedral` | `SC_Cathedral_Ambience.wav` | 60s, seamless loop | long reverb tail, choral air |
| `Environment.Manticore` | `SC_Manticore_Engine.wav` | 60s, seamless loop | machinery layers, load variation |

Beds: 48kHz **stereo**, played through `SpawnSound2D` (no spatialization, no attenuation),
crossfaded 0.6s out / 0.8s in on stage change. Loop the file itself — nothing at runtime hides a
seam. One-shots (weapon, impact, footstep, UI) stay **mono** on purpose: they are spatialized
through project attenuation, and a stereo source cannot be panned in 3D.

```bash
python3 Scripts/CheckAmbienceBeds.py
```
```bash
"/Users/Shared/Epic Games/UE_5.8/Engine/Binaries/Mac/UnrealEditor-Cmd" AshesOfHeaven.uproject -run=pythonscript -script="Scripts/ReimportAmbience.py" -unattended -nop4 -nosplash -nullrhi -nosound
```

`CheckAmbienceBeds.py` runs on plain python before anything is imported and rejects the five
failure modes that a waveform view hides: wrong channel count, wrong sample rate, under 55s,
clipping, identical L/R (a mono bed in a stereo container), and a loop seam whose sample step is
more than 8x a normal one. `ReimportAmbience.py` then replaces both the Raw and the Environment
SoundWave copies — the palette binds Environment, so replacing one is the classic silent no-op —
marks each looping and `LoadOnDemand` (a minute of stereo has no business resident in memory for a
once-per-stage bed), and logs duration, channels and rate as they landed in the engine.

The placeholder beds are synthesized stereo: tonal layers are identical in both channels, so hum,
resonance and engine orders sit centred, while each channel gets its own noise stream for wind and
machinery. Measured channel correlation is 0.80 (Erebus) to 0.96 (Manticore) — centred core, real
width in the noise. It is placeholder sound design with production *format*, not production audio.

## What remains human/audio-authoring work

The saved integration sources are not claimed as final AAA sound design. A sound designer still
needs to replace the temporary sources with authored M91 layers, material impacts, footsteps,
enemy vocalizations, battlefield distance, transit electrical decay, Cathedral resonance, Veil
spatial language, Manticore machinery, dialogue/voice, music, ducking, tails, and silence. Human
listening review is required; “audio device initialized” is not sound-design approval.
