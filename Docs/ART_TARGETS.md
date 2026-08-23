# ASHES OF HEAVEN — ART TARGET MANIFEST

This manifest records implementation evidence without claiming subjective visual approval. Each target is a real runtime target in `L_ChapterOne_Greybox`; the approved PNGs remain reference images only.

## TARGET: Erebus Battlefield

REFERENCE PATH: `References/ArtTargets/01_Erebus_Battlefield.png`

GAME LOCATION: Runtime target around X 250–12,600 in `L_ChapterOne_Greybox`; preview with `ArtTarget=Erebus` or `ArtTarget=M91`.

IMPLEMENTED:

- layered foreground, midground, and distant silhouettes;
- non-colliding fortifications, blast-wall/wreck/pipe scaffolding, fire lights, and dust;
- cold daylight, war fog, and restrained warm destruction accents;
- Cathedral-scale distant forms remain visible over the playable route;
- M91 first-person framing and a small capacitor scaffold;
- stable normal-renderer preview script.

PARTIALLY IMPLEMENTED:

- wet/puddle/mud/soot/debris look is represented by available project materials and shape layering;
- atmosphere is represented by cold fog, layered silhouettes, authored `NS_AshField`, `NS_EmberDrift`,
  `NS_DustSheet`, and restrained practical lights;
- battlefield beyond the route is visual lighting/silhouette simulation, not background AI.

NOT IMPLEMENTED:

- authored ruined-city meshes, wet material master/instances, puddle decals, smoke columns, embers, distant artillery VFX, and authored soundscape/music.

PHASE 4.2 AUDIO: Runtime event cues now resolve project-local WAV, SoundCue, and serialized
MetaSound assets through `UAHAudioSubsystem`, with authored attenuation/concurrency/submix routing.
This is still an integration palette, not final battlefield ambience, weapon layering, dialogue/voice,
footsteps, or mix approval.

KNOWN GAP: Current geometry is still procedural/greybox-derived and will not subjectively match the approved image until authored environment/material/VFX work is supplied.

PERFORMANCE: Non-colliding runtime primitives and a small number of movable non-shadow-casting lights. Measure on target hardware; desktop 60 FPS/mobile 30 FPS remain budgets, not yet device measurements.

HUMAN REVIEW REQUIRED: Composition, Cathedral dominance, weapon occupancy, fog density, enemy/pickup readability, route clarity, and whether the scene communicates war rather than a dressed test space.

## TARGET: Transit Station

REFERENCE PATH: `References/ArtTargets/02_Transit_Station.png`

GAME LOCATION: Runtime target around X 3,150–3,980; preview with `ArtTarget=Transit`.

IMPLEMENTED:

- platform floor, rails, entrance frames, overhead pipes, benches, control equipment, cases, and route-sign geometry;
- authored project signage: `TRANSIT STATION`, `NORTH LINE / PLATFORM 02`, `CIVIL DEFENSE / EVACUATION ROUTE`;
- amber and red practical lights, cold ambient fog, and dust;
- route-facing composition that supplements the objective HUD.

PARTIALLY IMPLEMENTED:

- sudden evacuation is represented by a small, composed case/seat/equipment set;
- wet reflections and failing fixtures are scaffolded by existing materials/lights.

NOT IMPLEMENTED:

- final station kit, ticket machines, route map material, luggage/clothing props, wet-surface master, leaking water, electrical hum, and authored damaged fluorescent VFX/audio.

PHASE 4.2 AUDIO: Objective, interaction, pickup, and dialogue timing now have project-local runtime
feedback; the station still needs authored electrical hum, failing-fixture detail, room tone, and
environmental mix.

KNOWN GAP: The station is materially more readable than the old open greybox, but it is not yet the full dense abandoned concourse in the reference.

PERFORMANCE: No visual collision/nav cost; limited lights and primitive count. Requires real GPU/draw-call/texture validation on desktop and mobile.

HUMAN REVIEW REQUIRED: Navigation without HUD dependence, practical-light readability, clutter density, and the emotional transition into silence/unease.

## TARGET: Cathedral Interior

REFERENCE PATH: `References/ArtTargets/03_Cathedral_Interior.png`

GAME LOCATION: Runtime target around X 14,200–18,100; preview with `ArtTarget=Cathedral`.

IMPLEMENTED:

- enormous vertical fins, nested frames, suspended volumes, dark voids, and a small human expedition walkway;
- cold selective light and haze/dust;
- three original procedural glyph families using the documented primitive grammar;
- expedition equipment and walkway provide scale references;
- no literal church windows, medieval buttresses, or copied protected symbols.

PARTIALLY IMPLEMENTED:

- Cathedral matter is represented by existing dark prototype instances;
- internal illumination is represented by restrained existing glow material and point lights.

NOT IMPLEMENTED:

- authored black ceramic/obsidian master, microstructure, seams, volumetric shafts, suspended architectural kit, final glyph materials/decals, Cathedral ambience, and cinematic-scale production meshes.

PHASE 4.2 AUDIO: The Cathedral resolves a project-local ambient source and event feedback, but no
authored resonance, tonal pressure, transmission voice, or production music is claimed.

KNOWN GAP: This is the highest-priority target but remains a procedural architectural scaffold rather than final art.

PERFORMANCE: Mostly static-looking non-colliding primitives, no nav impact, sparse lights, and asset-free fog/lighting atmosphere. Profile fog/light/shadow cost on each target tier.

HUMAN REVIEW REQUIRED: Perceived scale, negative space, silence, unfamiliar-but-designed geometry, glyph legibility, and human insignificance.

## TARGET: Lucian / Maya

REFERENCE PATH: `References/ArtTargets/04_Lucian_Maya.png`

GAME LOCATION: Runtime target around X 29,700–30,200; preview with `ArtTarget=LucianMaya`.

IMPLEMENTED:

- separated present-day composition with table/platform, cold key, restrained warm practical, dark negative space, and two character display proxies;
- original Lucian/Maya integration tags and stable preview viewpoint;
- legal UE mannequin assets used as an explicit interim scaffold.

PARTIALLY IMPLEMENTED:

- Lucian/Maya visual contrast is established through placement, material choice, and lighting;
- first-person gauntlet scaffold and M91 framing connect the gameplay target to Lucian's direction.

NOT IMPLEMENTED:

- final original Lucian face/body/hair/face paint, gauntlets, clothing, emblem/pendant, Maya production character, facial animation, cinematic animation, and authored dialogue staging.

PHASE 4.2 AUDIO: Dialogue timing resolves a project-local cue when no voice asset is assigned; final
voice performance, room tone, and cinematic mix remain open.

KNOWN GAP: The current shot still visibly contains mannequin proxies; it must not be presented as final character art.

PERFORMANCE: Two non-colliding skeletal display actors and two lights; measure animation/material cost after production meshes are supplied.

HUMAN REVIEW REQUIRED: Originality, silhouette, face-paint narrative, Maya's institutional contrast, body language, and whether the shot carries the approved seriousness without copying a real person.

## Phase 4.2 content status

The four art targets now consume the saved Phase 4.2 presentation boundary: UMG HUD assets,
semantic audio palettes, saved material masters/instances, cooked-safe Niagara systems, and
reusable environment Blueprint props. `Erebus`, `Transit`, `Cathedral`, `LucianMaya`, `M91`, `UI`,
and `Audio` are available through the development art-target launcher.

Machine validation proves asset existence, class loading, buildability, and packaged integration.
It does not prove subjective reference match, final sound quality, final character quality, mobile
performance, or human gameplay approval. Those remain explicitly `HUMAN REVIEW REQUIRED`.

## Phase 4.4 runtime integration status — 2026-08-22

The four targets are now integrated into the normal `L_ChapterOne_Greybox` runtime path as a
presentation layer. The gameplay collision and navigation layer remains separate and intact:
greybox collision actors are hidden visually, while project-owned props, meshes, materials, fog,
lighting, Niagara systems, environment profiles, and stage-mapped audio are spawned for presentation.

| Target | Asset pipeline | Normal runtime | Packaged smoke | Human visual approval |
| --- | --- | --- | --- | --- |
| Erebus battlefield | **READY** — project-owned starter mesh/material/prop/VFX/audio assets | **INTEGRATED** — war lighting, fog, props, ash/ember/dust, Erebus ambience | **PASS** — Development package logged the target on `L_ChapterOne_Greybox` | **UNTESTED** |
| Transit Station | **READY** — station props, signage, profile, VFX/audio references | **INTEGRATED** — normal stage path consumes the Transit profile and props | **PASS** — included in the fresh cooked package | **UNTESTED** |
| Cathedral interior | **READY** — fins, glyph scaffold, dark material/profile, VFX/audio references | **INTEGRATED** — normal stage path consumes the Cathedral profile and props | **PASS** — included in the fresh cooked package | **UNTESTED** |
| Present-day Lucian / Maya | **READY** — mannequin scaffold, profile, props, present-day ambience mapping | **INTEGRATED** — normal stage mapping reaches the present-day presentation layer | **PASS** — included in the fresh cooked package | **UNTESTED** |

This is runtime ownership/integration evidence, not final art approval. The environment meshes are
still replaceable starter geometry, the Lucian/Maya figures are mannequin proxies, and the Manticore
area still combines authored material/profile treatment with a gameplay scaffold vehicle. Final
environment kits, character art, animation, authored sound design/voice/music, and the visual match
to the approved images remain open art-production and human-review work. No normal-runtime screenshot
was available from this execution environment, so none is presented as evidence.

## Phase 4.8 visual gate iteration (2026-08-23)

Erebus-only iteration after the human rejected the Phase 4.6 result as "a dark
dressed blockout". Changes: gen-3 large architecture (fortress slabs, elevated
fortress block, near tower slab, checkpoint gate, banner drapes, debris fields),
metric-sane Cathedral relocation to ~265-335m with ground-hugging fog, full
tonal re-anchor (bright storm sky, backlit key, AE bias), retuned fire/smoke
Niagara, legacy transit overhead wires removed from the comparison frame.
Status: **UNTESTED — awaiting human visual review of Saved/Phase48Evidence.**
Erebus is NOT human-approved. Transit/Cathedral/Lucian-Maya art untouched by design.
