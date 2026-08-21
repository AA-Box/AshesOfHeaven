# Chapter One level and streaming architecture

Chapter One currently ships as one greybox map, `/Game/ChapterOne/L_ChapterOne_Greybox`. The map selects `AHChapterOneGameMode`; the director builds the prototype geometry, encounters, objectives, checkpoints, dialogue hooks, Manticore, terminal, and navigation bounds at runtime.

The logical sections are:

- `Erebus_Opening` — black opening, defensive line, and opening battle.
- `Erebus_Transit` — transit station and Veil revelation.
- `Erebus_Battlefield` — open battlefield and Manticore section.
- `Erebus_CathedralApproach` — approach, order, and countdown.
- `Erebus_CathedralInterior` — Cathedral interior, Sael, and terminal.
- `Erebus_Escape` — escape, other Lucian, and Erebus destruction.
- `PresentDay_Lucian` — ten-year transition, Maya, Nysa, fleet departure, disappearing stars, and title reveal.

`UAHChapterSubsystem` owns persistent chapter state independently of the current world: stage, objective index, checkpoint identifier, countdown/failsafe flags, completed narrative/section/encounter history, and Manticore state. `UAHCheckpointSubsystem` serializes that state with player health, equipment, and encounter state so death/restart and checkpoint reload do not depend on transient actor lifetime.

The current one-map layout is intentional for the first greybox. When authored content grows, each logical section can become a streamed level or World Partition region. The existing stage boundaries are the streaming seams: load the next section before the transit, battlefield, Cathedral, destruction, or present-day transition; commit checkpoint state before unloading completed content; and preserve the `GameInstance` chapter state across travel.

The target streaming policy is:

- keep the player-adjacent combatants as full actors;
- reduce tick, perception, animation, and decision frequency for mid-distance actors;
- represent distant war-scale activity with low-cost silhouettes, Niagara, tracers, explosions, sound, vehicles, aircraft, and smoke;
- use scripted or pre-fractured destruction where real-time Chaos is too expensive;
- keep texture/audio streaming enabled and bound projectile/VFX pools;
- unload completed sections on mobile after checkpoint state is committed.

The architecture preserves equivalent gameplay results across platforms while allowing the final art pass to split the logical sections without rewriting the mission state machine.
