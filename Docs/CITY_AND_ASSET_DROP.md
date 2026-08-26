# City kit and external asset drop

Two external sources feed the project's environment and animation content. Neither is
committed: both are large third-party drops and this repository has no LFS, so the content
folders are gitignored and the scripts below are how they are rebuilt.

## CitySample city art kit

Source: the CitySample sample project (UE 5.8) at `$AH_CITYSAMPLE`, default
`~/Documents/Unreal Projects/CitySample`.

`Scripts/MigrateCitySampleKit.py` copies **5,935 packages / ~30 GB** of city art into this
project, preserving `/Game` paths so every internal reference resolves without redirectors.

The set is the closure of what both CitySample maps actually reference, minus everything that
belongs to the sample's own game: `Crowd`, `Vehicle`, `Character`, `AI`, `Cinematics`, `UI`,
`Audio`, `Gameplay`, `Input`, the maps themselves and their external actors. What lands here is
`Building`, `Prop`, `Road`, `Megascans`, `Material`, `Environment`, `Effect`, `Textures`,
`City`, `Lighting`, `WFC`.

Three things are worth knowing before touching this pipeline:

* **CitySample's own editor does not open here.** Its C++ module fails to load against this
  engine install (`The game module 'CitySample' could not be loaded`), so the asset registry
  is unavailable and the closure is computed by reading package paths straight out of the
  uasset name tables instead. That is also why the migration is a file copy rather than the
  editor's Migrate action.
* **The art is almost free of the sample's C++.** Only the gameplay framework blueprints
  (game mode, player character, cameras, drone) reference `/Script/CitySample`, and they are
  excluded. One vehicle material function lives in the Traffic plugin; rather than build that
  C++ plugin, the single asset is republished as the content-only `Plugins/Traffic` stub so
  the `/Traffic/` mount point resolves.
* **Two shared assets sit under excluded folders.** `MF_NormalStrength` and
  `WhiteSquareTexture` live under `Crowd/Character/Shared` but are referenced by kit
  materials, so they are pulled in explicitly. CitySample's doppelganger/dissolve demo FX are
  pruned, being the only kit content that still pointed at the excluded character art.

Re-run at any time; it is idempotent and never overwrites an existing destination file.

## Completing LV_Soul_Slum

`Scripts/CompleteSoulCity.py` is the one that matters: it grows a war-broken city around the
project's existing city instead of building a new one. `LV_Soul_Slum` is the only authored city
in the project - 2,377 actors, 1,818 static meshes, 134 lights, 165 emitters, an authored
~1144 x 469 m slum strip in a dam valley with a *painted* city backdrop
(`SM_LV_Soul_City_Matte01`, 2.4-3.6 km out). The script replaces that painted skyline with real
ruined buildings.

**Nothing authored is touched, by construction:**

* every actor the script creates is tagged `AH_CityCompletion` and lives in the outliner folder
  `CityCompletion`, and each run begins by deleting every actor with that tag. Re-running
  rebuilds; deleting the folder returns the map to its authored state exactly.
* candidates are rejected against the real positions and radii of the authored actors, so a new
  building cannot land on existing geometry. A default run places 24 and **rejects 18** on that
  test.
* no light, fog, post-process or sky actor is created or edited. The slum's own lighting stays
  authoritative.
* the .umap is copied to `Saved/MapBackups/` before the first save.

Run it through the RUNNING editor, never a commandlet:

```bash
python3 Scripts/RunInEditor.py Scripts/CompleteSoulCity.py
```

`Scripts/RunInEditor.py` drives the editor's python remote-execution protocol (unicast UDP to
127.0.0.1:6766, TCP connect-back; multicast does not work on this machine). It is required
because placement is decided by tracing for real ground and **a `-nullrhi` commandlet has no
ticked physics scene** - every `line_trace_single` misses, and the first attempt hung the whole
ring in the air over the reservoir.

Placement traces a 2.6 km grid at 40 m steps around the slum and keeps only points that hit
terrain: 1,654 of 4,225 samples are land, the rest are water or void. Survivors are filtered
against the authored actors, thinned to a 115 m minimum spacing, sorted nearest-first so the
city grows outward from the slum edge, and tiered by distance - near ruins stay closest to
plumb and take dry concrete, far ones tilt hardest and take burnt.

Four measured traps shaped it:

* **`SM_SkySphere_Cloudy` has a 32,768 m radius** centred on the slum. Included in the clearance
  test it rejects every candidate, so anything above `BACKDROP_RADIUS` (500 m - measured p99 for
  real geometry is 600 m) is treated as backdrop, along with the merged dam walls, the water
  splash planes and the mattes.
* **`HitResult` exposes nothing through `get_editor_property` in 5.8** - `blocking_hit` raises
  "Failed to find property". `hit.to_dict()` is the accessor that works.
* **The ground under the slum centre is at Z = -96.9 m**, not 0. There is a `Landscape_0` in the
  map that the class histogram hides (a single instance). Assuming z=0 floats everything ~97 m.
* **Overriding only the InstancedStaticMeshComponents is not enough.** A packed level actor keeps
  some parts - notably tower glass - on plain StaticMeshComponents, and a mirror-finish
  skyscraper in a bombed city is worse than no override at all.

"Semi-broken" is: buildings tilted off plumb (1.5 deg near, 6.5 deg far), sunk 2-16 m so the
lower floors read as collapsed, skirted in Erebus rubble/ruin meshes and imported rocks, scorched
with decals, and re-surfaced in Erebus dry or burnt concrete.

### Roads, culling and navigation

The same pass then ties the ruins to the slum and makes the result playable:

* **Roads.** A spur from every ruin back toward the slum, plus links between angular neighbours
  closer than 420 m, tiled from `SM_ROAD_19_20_0_0_road` (a 21 m carriageway, 2004 cm per tile).
  Each tile traces its own ground, pitches to the local slope from a trace fore and aft, and beds
  45 cm into it. Where the trace finds water, void or authored geometry the tile is skipped, so
  roads break where the war broke them. Roads use a 9 m clearance against **authored** actors
  only - the 60 m building clearance is exactly what a road leading to a building must ignore.
  The mesh's bounds origin sits at local y = +1000, so the actor is pushed half a road-width back
  or every tile lands 10 m off its path.
* **Culling.** `ld_max_draw_distance` = 450 m on all 717 road/rubble/debris actors. Proper HLOD is
  not available: `unreal.HierarchicalLODUtilities` and `unreal.MeshMergeFunctionLibrary` **do not
  exist** in this build's python API, so HLOD stays a manual World Settings + Build > Build HLODs
  job. The buildings are packed level actors and instance themselves already.
* **Navigation.** The authored map ships **no NavMeshBoundsVolume at all** - nothing in it was
  ever navigable. One is added over the slum plus a 300 m skirt (1744 x 1069 x 400 m; covering
  every traced ruin would be a 2.0 x 2.2 km navmesh for silhouette geometry) and
  `RebuildNavigation` is issued. The `RecastNavMesh` the engine spawns alongside it is tagged
  too, or it would survive a revert and leave an actor the authored map never had.

A default run: **839 actors - 40 buildings, 514 road tiles, 188 rubble, 81 decals, 15 debris
props, 1 nav volume + 1 navmesh.** Authored actor count is unchanged at 2,377.

Knobs: `AH_CITY_SEED`, `AH_CITY_BUILDINGS`, `AH_CITY_SPACING_M`.

## Erebus tonal values for a large district

A standalone `L_ErebusCity` sandbox was built from the kit and then deleted - it was a separate
map, not the game's city, and keeping a script that regenerates it is not a fix. The lighting
values measured on it are worth keeping, because the Erebus corridor recipe in
[ashes-build-validation-pipeline] is tuned for a 350 m diorama and does not survive a 700 m+
district: at sun 32 lux with fog 0.022 and auto-exposure clamped to 3.0 EV, the whole city
collapses to black silhouettes under a white sky.

| Knob | 350 m corridor | 730 m district |
| --- | --- | --- |
| Sun | 32 lux, pitch -15 | 1500 lux, pitch -34, colour (235,240,255) |
| Skylight | 0.55 | 6.0 |
| Fog density / falloff | 0.022 / 0.30 | 0.006 / 0.25 |
| Fog inscattering | 0.42 warm | (0.10, 0.11, 0.13) |
| Auto-exposure | bias -0.55, min 0.03, max 3.0 | **pinned** min = max = 7.0 EV100, bias -0.25 |
| Rayleigh / Mie | 2.2 / default | 1.6 / 0.008, absorption 0.003 |

Two rules that came out of it: pin exposure in any map where most of the frame is sky, or the
histogram keys off the sky and crushes the geometry; and judge exposure only from a **cold load**,
because Lumen carries radiance over and a scene retuned in place reads about a stop brighter than
it will on reopen (measured 37/255 mean luma retuned vs 16/255 after reload, identical values).

## External asset drop

Source: `$AH_ASSET_DROP`, default `~/Downloads/new`.

`Scripts/ImportAssetDrop.py` brings in 52 static meshes, their textures and material
instances, and a 50-clip sound library:

| Pack | Destination | Notes |
| --- | --- | --- |
| Environment Rock Collection 04 | `/Game/Ashes/Environment/Rocks/Collection04` | 7 rocks, ORM maps, Nanite |
| Rock Pack Vol 01 | `/Game/Ashes/Environment/Rocks/PackVol01` | 12 rocks, 4 texture sets dealt round-robin, Nanite |
| Stylized NYC Street Props | `/Game/Ashes/Props/Street` | 20 props, no source textures - they take a flat Erebus instance |
| Military Radio | `/Game/Ashes/Props/Radio` | 13 props, ORM maps |
| 50 Free Game Sounds | `/Game/Ashes/Audio/Library` | 20 SoundWaves kept; see the sound library section below |
| VanillaLoop ladder set | `/Game/Ashes/Props/Ladder` | 3 modules, unpacked from the unitypackage by the animation script - run that one first |

`SM_Trash_Bin` is skipped: it ships broken in that pack (the FBX holds an empty `Circle_028`
object and the OBJ is a 98-byte header). `SM_Bin` from the same pack is the intact bin.

These packs ship roughness in an ORM green channel while the Erebus texture family keeps it in
red, so `M_ErebusSurface` gained `RoughChannelMask` and `MetallicChannelMask`. Both default to
the Erebus layout, so existing instances are unchanged.

## Animation drop

`Scripts/ImportAnimationDrop.py` imports **1,826 clips** into `/Game/Ashes/Animations`: 879 rifle
and 879 pistol locomotion clips, 8 falling/rolling traversal clips, 16 look-through-window mocap
clips, 30 dead-body poses, and the 14-clip VanillaLoop sample set (cover, DBNO, ladder, stairs,
push, lift, mining, roll, item pickup, narrow passage, male/female locomotion, flashlight and
locker).

The VanillaLoop set arrives as `freesampleanimationset.unitypackage`, which is a gzipped tar of
hash folders, each holding the raw asset beside a `pathname` file naming it. The script unpacks
it to `Saved/UnityDropSource` and imports from there; that same tree is where
`Scripts/ImportAssetDrop.py` finds the ladder meshes, so run the animation script first on a
clean machine. Three skeletal interactables from the pack - two lockers with their open/close
clips and a flashlight - land in `/Game/Ashes/Props/Interactables` on their own skeletons.

An existing clip is skipped rather than re-imported; set `AH_ANIM_FORCE=1` to rebuild the set
(about 25 minutes).

They bind to the UEFN mannequin the locomotion pack ships, imported to
`/Game/Ashes/Characters/UEFNMannequin`, **not** to the project's own
`/Game/Characters/Mannequins/Meshes/SK_Mannequin`: the locomotion clips carry an extra
`attach` bone track, and importing them against a skeleton without that bone aborts the entire
run with `Unable to retrieve bone index for track: attach`. The UEFN hierarchy is a superset of
the project mannequin's, so all four packs land on it; moving them onto another body is an IK
Retargeter step on top of this import, not part of it.

A dropped bone track raises during import but still writes a usable clip, so the importer
treats a missing asset - not a raised exception - as the failure condition.

## Sound library

`Scripts/AuthorSoundLibraryCues.py` turns the 50 imported SoundWaves into playable content.
A bare SoundWave is not usable in this project: gameplay only asks `UAHAudioSubsystem` for a
semantic id, and a sound played outside the project's attenuation/concurrency/submix assets
plays at full volume through the master bus and is audible anywhere in the level.

The clips are grouped into banks, each authored as one `SC_Lib_<Bank>` SoundCue with a
`SoundNodeRandom` over its variations (`randomize_without_replacement`, so nothing repeats back
to back), routed through `ATT_World3D`/`CONC_World`/`SM_World`, or `ATT_UI`/`CONC_UI`/`SM_UI` for
the Interface and Pickup banks, or `SM_Ambience` for the five loops. Loops additionally get a
`SoundNodeLooping` and their SoundWave marked looping; one-shots are `FORCE_INLINE` (a streamed
miss is a silent hit) while loops stay `LOAD_ON_DEMAND`.

`BANKS` in the script is the authority on what ships. A wave in the library folder that no bank
claims is deleted, along with any cue and palette id left behind - that is how clips pulled from
the source pack leave the project instead of lingering as orphans.

The pack was trimmed from 50 clips to the **20** kept on disk, which left 13 banks: Explosion,
GunshotHeavy, WeaponHandling, ImpactRock, Melee, Whoosh, CreatureVoice, Door, Pickup, Industry,
and the AmbienceWind / AmbienceBirds / AmbienceFire loops. Seven banks lost every member and were
removed: GunshotEnergy, ImpactMetal, ImpactWood, Interface, Water, AmbienceRain, AmbienceWater.

Because `ImpactMetal` is gone, `Weapon.M91.Impact` is handed back to `SC_M91_Impact` via
`PALETTE_RESTORE` rather than left pointing at a deleted cue. The two surviving repoints stay:
`Combat.Grenade` -> Explosion, `Combat.Melee` -> Melee. Weapon fire, footsteps and the UI stingers
keep the SciFi pack, and the player's own hurt/death voices are deliberately untouched.

**`EditorAssetLibrary.delete_asset` leaves the .uasset on disk.** It drops the asset from the
registry, so a later run cannot even see the file to retry and the asset reappears on the next
editor start. Pruning deletes the file too, and sweeps the folder for files the registry has
already forgotten. The final state is asserted by reading the palette back: if its `Library.*`
ids do not match `BANKS` exactly, the script raises.

The cues are gitignored along with the waves they play; the palette is committed, so on a fresh
clone its `Library.*` entries are unresolved soft pointers until this script is re-run.
