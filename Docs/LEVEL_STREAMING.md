# Chapter One streaming plan

Keep the same campaign and save structure on every platform. Divide Chapter One into streamable sections, using World Partition or level streaming as the content grows:

- `Erebus_Opening`
- `Erebus_Transit`
- `Erebus_Battlefield02`
- `Erebus_CathedralExterior`
- `Erebus_CathedralInterior`
- `Erebus_Escape`
- `PresentDay_Lucian`

Load the next section before corridors, elevators, battlefield transitions, cinematics, doors, or other natural occlusion points. Unload completed sections on mobile after checkpoint state is committed. Keep texture/audio streaming enabled, bound Niagara/projectile pools, and avoid keeping the full chapter resident.

The player-adjacent combatants are full actors. Mid-distance actors reduce tick, perception, animation, and decision frequency. Distant war scale uses animated meshes, Niagara, tracers, explosions, sound, vehicles, scripted aircraft, smoke columns, and silhouettes. Destruction must use a scripted/pre-fractured fallback where real-time Chaos is too expensive; the gameplay result remains equivalent.
