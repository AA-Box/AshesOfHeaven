# ASHES OF HEAVEN — ART DIRECTION

Phase 4 establishes the visual identity for four representative playable target areas. The approved references are canonical visual targets, not replacement screenshots:

- `References/ArtTargets/01_Erebus_Battlefield.png`
- `References/ArtTargets/02_Transit_Station.png`
- `References/ArtTargets/03_Cathedral_Interior.png`
- `References/ArtTargets/04_Lucian_Maya.png`

The current implementation is an in-engine procedural target layer over the Phase 3 greybox. It preserves the existing collision, route, objective, encounter, checkpoint, navigation, Manticore, dialogue, and completion systems. It is deliberately honest about the areas that still require authored production assets.

## Core pillars

ASHES OF HEAVEN is industrial military realism, brutalist megastructure, black-metal severity, sacred alien geometry, and cosmic archaeological horror. It is not neon cyberpunk, glossy science fiction, heroic space opera, fantasy gothic, or generic space marine art.

The visual hierarchy is:

1. readable human-scale gameplay and enemy silhouettes;
2. large believable manufactured structure;
3. controlled atmosphere and negative space;
4. sparse warm practical/emergency light or cold Cathedral light;
5. restrained detail and environmental story evidence.

## Locked palette

| Area | Base | Accents | Avoid |
| --- | --- | --- | --- |
| Erebus | charcoal, steel, cold gray, dirty concrete, desaturated blue-gray | fire, ember, emergency amber, warning red | clean sky, saturated color, neon strips |
| Transit | black, dirty metal, dirty white | amber signage, red emergency fixtures, wet reflections | flashlight-only horror, clutter soup |
| Cathedral | near-black, charcoal, deep cool gray/blue | sparse cold white internal light | glossy black plastic, colored RGB light |
| Present day | black, charcoal, soft cold practical light | restrained institutional markings | romantic framing, gothic ornament |

## Human language

Human/Erebus construction is heavy, repairable, and physically legible: armored panels, concrete, blast walls, pipes, conduits, barricades, crates, platforms, rails, utility equipment, painted warnings, wet surfaces, soot, and localized damage. The current procedural kit maps the available Unreal/LevelPrototyping assets into this language without introducing collision-bearing decoration.

The temporary soldiers use faction materials and a readable body proxy. Human soldiers share a steel/gray family; Veil soldiers use matte obsidian; Warden has a wider body and shoulder armor. The final human soldier set still needs authored helmets, packs, textiles, markings, animation, and weapon sockets.

## Veil language

The Veil is reconstructed, ancient, synthetic, and haunted by biological memory. The target is matte dark ceramic, pale metal only where it helps silhouette, and rare cold internal illumination. It must not become a generic robot, zombie, or neon alien. Pilgrim remains the fast humanoid infantry silhouette; Warden is deliberately broader, slower, and more defensive.

## Cathedral language

The Cathedral is engineered monumentality rather than a literal church. The reusable vocabulary is:

- monolithic vertical fins;
- deep slots and nested rectangular voids;
- suspended volumes;
- precise seams and narrow gaps;
- human expedition walkways and equipment as scale references;
- cold shafts and internal light in a black engineered mass.

The runtime glyph grammar is original and primitive-based. Each glyph is built from a central axis, crossbar, eight interrupted radial segments, and optional orbit marks. The same family can later be authored as a material/decal/mesh system. It is not a copy of the reference symbols and is not a literal inverted cross.

## Erebus target

Erebus must read as a destroyed industrial city under active war: layered foreground fortifications, midground defensive walls and wreckage, distant ruin silhouettes, fire sources, smoke/ash, wet ground, and a Cathedral landmark that dwarfs human construction. The current target implements layered non-colliding fortification, pipe, wreck, wall, fire-light, dust, and distant silhouette scaffolding while leaving the proven gameplay floor and cover authoritative.

## Transit target

Transit is the emotional transition from battlefield chaos to human infrastructure, silence, and wrongness. Platforms, rails, door frames, overhead pipes, benches, cases, control equipment, authored route labels, amber/red practicals, and dust make the route readable through the environment instead of the HUD alone. The current signage uses project-authored strings such as `EREBUS TRANSIT AUTHORITY`, `NORTH LINE`, `PLATFORM 02`, `CIVIL DEFENSE`, and `EVACUATION ROUTE` rather than copying accidental reference text.

## Lucian and Maya

Lucian is an original fictional character: worn, capable, severe, burdened, and visually distinct. The desired final direction is worn black combat textile, ceramic armor, gauntlets, long dark hair, a ritual face-paint mask tied to soldiers lost under his command, and an original sacrifice/burden emblem. The emblem must remain a one-color fictional shape and must not be a literal inverted cross.

Maya is the visual counterpoint: a contemporary UNS VIGIL captain with cleaner institutional military construction, controlled posture, and less wear. The present-day target uses an industrial table/platform composition, separation, cold key light, restrained warm practical, and negative space. Existing UE mannequin assets are used only as a legal interim integration scaffold; final faces, hair, face paint, clothing, gauntlets, materials, and facial performance remain external character-art work.

## M91 Revenant

The M91 is heavy, industrial, electromagnetic, and believable. The existing rifle mesh remains the gameplay source. The Phase 4 target narrows its first-person occupancy, preserves the camera-aligned hold rotation, marks the mesh as a first-person primitive, adds a small capacitor/power-architecture scaffold, and keeps recoil interpolation anchored to the hold pose. A final authored M91 model, hand animations, ADS/reload/sprint poses, and restrained status indicators remain an art/animation gap.

## HUD and typography

The HUD is minimal, military, quiet, and scalable. It uses the existing Unreal medium font so redistribution does not depend on an unlicensed font. Health/armor, ammo/reserve, grenades, interaction, crosshair, hit/damage feedback, objective progress, countdown, and Manticore status remain present. Objective changes receive a short amber prominence pulse and then settle into a quieter near-black bracket frame. Armor is cyan, vitals/danger are red or amber, weapon/ammo is bone white with amber status, and all panels use hairline rules rather than generic blue-grey blocks. Layout uses safe margins and a 0.85–1.35 scale range based on the smaller viewport dimension for 1280×720 through 2560×1440; ultrawide and landscape mobile remain implementation-review targets.

## Audio direction and runtime integration

The sound identity follows the same restraint as the image direction: human hardware is heavy,
electromagnetic, dry, and mechanical; the Veil is low, synthetic, and almost spatially empty; the
Cathedral carries cold tonal pressure rather than a conventional fantasy choir; present-day scenes
use controlled room tone and dialogue space. Gameplay feedback must be immediate even before the
production sound pass exists.

Phase 4.1 implements `UAHAudioSubsystem` as a packaged-safe integration palette. It owns short PCM
fallback cues for fire, reload, dry fire, impacts, melee, armor, damage, death, grenades, pickups,
footsteps, objective changes, dialogue timing, and a low mechanical ambient bed. A project weapon-fire asset is
used where available, and authored Blueprint sound properties always take precedence over fallback
cues. This makes the event graph audible now without pretending that synthesized integration tones
are final music, voice acting, footsteps, enemy performance, environmental ambience, or mix design.

The production audio pass should replace the fallback palette cue by cue, keeping the same event
contracts so audio authoring does not alter gameplay code. It should establish a human/Veil contrast,
transit electrical decay, Erebus battle distance, Cathedral negative-space resonance, Manticore
mechanical mass, and a restrained Lucian/Maya dialogue bed.

## Lighting, atmosphere, and VFX

The generated map now provides a cold desaturated daylight sun, real-time sky capture, Exponential Height Fog, and sparse battlefield/practical point lights. The approved prototype Stateless Niagara dust asset was intentionally not loaded because it asserts while deserializing in the installed Mac package; fog, silhouettes, and lighting provide the current atmosphere hook. Fire, ash, ember, smoke-column, sparks, distant battle, and Cathedral particle families are planned extension points; any future Niagara family must be cooked-safe, expose quality scalability, and lower spawn/update cost for mobile.

Post-process tuning remains intentionally restrained: preserve enemy/pickup readability, avoid crushed blacks, avoid full-screen bloom, and keep letterboxing out of normal gameplay. The current runtime layer is a target scaffold, not a final post-process grade.

## Material architecture and provenance

The current procedural mapping is:

| Directional family | Current legal project asset | Purpose |
| --- | --- | --- |
| Human metal / painted metal | `MI_PrototypeGrid_Gray_02` | temporary steel/armor family |
| Concrete / damaged concrete | `MI_PrototypeGrid_Gray_Round` | temporary concrete/wall family |
| Cathedral matter / Veil obsidian | `MI_PrototypeGrid_TopDark` and `MI_PrototypeGrid_Gray` | dark engineered mass |
| restrained technology light | `MI_GlowNT` | sparse glyph/sign/indicator accent |
| character scaffold | UE mannequin meshes/materials already in `Content/Characters` | interim Lucian/Maya/display proxy |
| weapon | project rifle assets in `Content/Weapons/Rifle` | M91 gameplay and target scaffold |

These are project/Unreal content already present in the repository. No scraped commercial-game assets or protected logos were added. Production master materials (`M_HumanMetal`, `M_Concrete`, `M_WetSurface`, `M_CathedralMatter`, `M_Scorch`, `M_Grime`, and their authored instances) remain an external material-authoring task.

## Scalability and cross-platform rules

The identity must survive on cheaper renderers through silhouette, palette, lighting direction, composition, and limited fog. It must not require Nanite, Lumen, Virtual Shadow Maps, or desktop-only Niagara. Desktop may add richer shadow/fog/VFX tiers; baseline mobile targets stable 30 FPS. All visual target shapes are non-colliding and do not affect navigation. Gameplay collision remains simplified and authoritative.

Desktop performance target is 60 FPS; high-end mobile is 60 FPS where sustainable; baseline supported mobile is stable 30 FPS. Record GPU/game/render thread, draw calls, geometry, material/texture memory, shadows, fog, Niagara, and streaming measurements when the target is profiled on real hardware.

## Stable review viewpoints

Development launch examples:

```text
./Scripts/Run-Mac-ArtTarget.sh Erebus
./Scripts/Run-Mac-ArtTarget.sh Transit
./Scripts/Run-Mac-ArtTarget.sh Cathedral
./Scripts/Run-Mac-ArtTarget.sh LucianMaya
./Scripts/Run-Mac-ArtTarget.sh M91
```

The viewpoint system only selects real in-engine positions. It never substitutes an offline render or generated image for a playable scene. Human visual review is still required for subjective match, route readability, enemy readability, and final art approval.
