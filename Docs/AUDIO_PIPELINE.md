# Phase 4.2 — Audio pipeline

Gameplay calls semantic events through `UAHAudioSubsystem`; gameplay does not know whether a cue
is a SoundCue, MetaSound source, authored voice asset, or a later mix revision.

## Semantic contract

The default palette contains `Weapon.M91.Fire`, `Weapon.M91.Reload`, `Weapon.M91.Empty`,
`Weapon.M91.Impact`, combat feedback, `UI.Objective`, `UI.Dialogue`, `UI.Pickup`,
`Player.Footstep`, and environment events `Environment.Erebus`, `Environment.Transit`,
`Environment.Cathedral`, and `Environment.Manticore`.

`DA_AudioPalette_Default`, `DA_AudioPalette_Human`, and `DA_AudioPalette_Veil` are real saved
data assets. The default palette currently resolves to project-contained SoundCue integration
assets so normal packaged builds produce audible feedback. The corresponding MetaSound targets
(`MS_M91_Fire`, `MS_M91_Impact`, `MS_Erebus_Ambience`, `MS_Transit_Ambience`,
`MS_Cathedral_Ambience`, `MS_Manticore_Engine`, `MS_UI_Objective`) are also saved and ready for
the authored graph/mix handoff.

The M91 no longer points directly at the template grenade-launcher-family path. Its semantic
source is `/Game/Ashes/Audio/Weapons/M91/SC_M91_Fire`.

## Fallback policy

The old procedural PCM generator remains only as an emergency development diagnostic. It is gated
by `UAHAudioSettings::bAllowGeneratedAudioFallback`, defaults to `False`, and is disabled in
Shipping even if misconfigured. If an authored event is absent on the normal path, playback is
skipped and the missing semantic event is logged; the build does not silently synthesize a tone.

## Region behavior

Chapter-stage changes select and cross-fade authored environment components for Erebus, Transit,
Manticore, and Cathedral contexts. The system supports attenuation through normal SoundCue/
MetaSound authoring, and the saved submix folder is reserved for the production mix graph.

## What remains human/audio-authoring work

The saved integration sources are not claimed as final AAA sound design. A sound designer still
needs to replace the temporary sources with authored M91 layers, material impacts, footsteps,
enemy vocalizations, battlefield distance, transit electrical decay, Cathedral resonance, Veil
spatial language, Manticore machinery, dialogue/voice, music, ducking, tails, and silence. Human
listening review is required; “audio device initialized” is not sound-design approval.
