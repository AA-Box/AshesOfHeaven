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

## What remains human/audio-authoring work

The saved integration sources are not claimed as final AAA sound design. A sound designer still
needs to replace the temporary sources with authored M91 layers, material impacts, footsteps,
enemy vocalizations, battlefield distance, transit electrical decay, Cathedral resonance, Veil
spatial language, Manticore machinery, dialogue/voice, music, ducking, tails, and silence. Human
listening review is required; “audio device initialized” is not sound-design approval.
