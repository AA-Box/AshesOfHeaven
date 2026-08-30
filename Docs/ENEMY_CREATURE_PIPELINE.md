# Enemy creature pipeline

Three enemy archetypes are built from external models. Two come from the drop in `~/Downloads/enemy`
(override with `AH_ENEMY_SOURCE`); the crawler comes from a separate Sketchfab diorama in
`~/Downloads/spider-new` (override with `AH_FACEHUGGER_SOURCE`). None of them arrives game-ready:
one is an FBX version no current DCC will open, one ships its maps only inside a `.blend`, and the
crawler is scene dressing with no skeleton at all. The scripts below turn those drops into the
roster, in this order.

Two archetypes have left. The `Warden` / Veil Revenant was built from the armoured-figure model and
its 4.0 threat was split across the bodies that remain. The `Spider` archetype kept its id but
changed body entirely: it was a 6,955-vert bio-mech spider wearing procedural noise, and it is now
the crawler, which ships a painted 2048 trim sheet.

| # | Script | Runs under | Produces |
|---|--------|------------|----------|
| 1 | `Scripts/BakeCreatureTextures.py` | plain python + numpy | Procedural chitin, the shared detail normal, and the crawler's split maps, in `Saved/CreatureTextureSource` |
| 2 | `Scripts/PrepareCreatureSources.py` | Blender (headless) | The quadruped's 4K maps, unpacked from its `.blend` into `Saved/CreatureSource` |
| 3 | `Scripts/RigFacehugger.py` | Blender (headless) | `Saved/CreatureSource/Facehugger_Mesh.fbx` - the crawler, rigged, because the drop ships it unskinned |
| 4 | `Scripts/ImportEnemyModels.py` | `UnrealEditor-Cmd` | `SKM_*`, `PA_*`, `T_*`, `MI_*`, `M_EnemyCreature`, and `Saved/EnemyModelManifest.json` |
| 5 | `Scripts/AuthorCreatureAnimations.py` | `UnrealEditor-Cmd` | `AS_*` takes for the skeletons that shipped without any, and `Saved/CreatureAnimations.json` |
| 6 | `Scripts/AuthorEnemyDefinitions.py` | `UnrealEditor-Cmd` | `DA_Enemy_*`, `DA_Encounter_*` |
| 7 | `Scripts/AuthorEncounterDirectorAssets.py` | `UnrealEditor-Cmd` | The directed encounters and their pools |

Steps 4-7 take a project path, and the project path contains a space; invoke them through a
symlink (`ln -sfn "<project>" /tmp/aoh`) or the `-script=` argument is split.

The order is load-bearing. Step 4 wipes and rebuilds each model's content folder, so it destroys
anything step 5 wrote; step 5 reads the skeletons step 4 imported; step 6 reads both manifests.

## The roster

| Archetype | Model | Weapon | Animation source | Textures |
|-----------|-------|--------|------------------|----------|
| `Pilgrim` - Veil Stalker | x-com alien | rifle | one idle take in the FBX, plus authored walk/run/attack/death | procedural chitin |
| `Hound` | Alien-Animal | none - bites | six takes in the FBX, plus a rate-scaled walk | 4096 albedo / normal / roughness / metallic, unpacked from the `.blend` |
| `Spider` - Veil Crawler | facehugger, from the alien-eggs diorama | none - bites | all five takes authored | 2048 painted trim sheet + its packed roughness, normal derived from albedo luminance |

Only the Stalker carries a weapon. `AshesOfHeaven.Assets.Enemies.RosterArmamentAndLocomotion`
pins that, because a loadout is a soft array in a data asset and nothing in the engine objects to
a hound holding a rifle.

## Why each script exists

**`PrepareCreatureSources.py`** covers the two things Unreal cannot do for itself.

*The quadruped's glTF export packs metallic and roughness into one image.* In the glTF convention
that is occlusion in red, roughness in green, metal in blue - and a roughness sampler reads red,
which is 1.0 nearly everywhere. The map did nothing and the animal had one flat roughness value
over hide and plating alike. The `.blend` still carries the separate 4096 originals, packed.

**`RigFacehugger.py`** exists because the crawler is scene dressing, not a character. Its diorama
has no armature, no vertex groups and no animation in the entire file, and the enemy pipeline
consumes skeletal meshes. The skeleton - 8 legs of 3 joints in mirrored pairs, plus a 4-joint tail
- is fitted to the geometry rather than hardcoded: limb tips come from farthest-point sampling with
an angular separation test (a pure distance threshold merges the fused finger pairs), and each
joint comes from walking the mesh edge graph back toward the body and averaging the limb's
cross-section, so bones land on the centreline instead of the skin. Bone names are a contract that
`AuthorCreatureAnimations.py` pattern-matches on: `root`, `body`, `leg_{L,R}_{1..4}_{a,b,c}`,
`tail_{1..4}`.

Measured limits, not assumed ones: about 20% of a leg's motion bleeds into its immediate
neighbour, and the unweighted `root` bone sits about 5% of the model's size outside the mesh.

**`AuthorCreatureAnimations.py`** writes `UAnimSequence` bone tracks directly through the
animation data controller. No FBX round trip, because the alien's source FBX is version 6000 and
Blender rejects it; working on the imported skeleton sidesteps the format and keeps the takes
regenerable from this repo.

The spider gets a real inverse-kinematics pass. Its bind pose is not a stance - one leg is folded
under the belly and another is stretched flat out behind - so the first thing the script does is
solve a symmetric stance, and `Saved/CreatureAnimations.json` reports the resulting foot plane and
height for `AuthorEnemyDefinitions.py` to fit the capsule and mesh offset against. Fitting to the
bind bounds instead describes a 783cm box the creature never fills.

Three things in that script are worth knowing before editing it:

* Bone parents are recovered numerically from the reference pose, not hard-coded, because two of
  these skeletons name every bone `Bone_017`. Only bones earlier in the skeleton's own order are
  candidates - a parent always has a lower bone index - which makes a parent cycle impossible
  rather than merely unlikely.
* `Rig._verify_hierarchy` re-composes every bone and compares against the reference pose. A wrong
  parent or a flipped quaternion order produces a rig that still evaluates, just with the legs
  somewhere else, and the only downstream symptom is a solver that never converges.
* Scale is carried through the composition. The alien's root bone holds a 2.54 inch-to-centimetre
  conversion and the spider's holds 100, so a local translation means a hundred times what it
  says: "bob the body 12 units" moves it twelve metres.

## Known gap

The alien's surface has no local contrast. `BakeCreatureTextures.bake_chitin` writes its albedo
across 0.055-0.185, which is both dark and nearly flat, and `MI_Stalker_00` then multiplies it by
a 0.20/0.11/0.055 tint. The quadruped lands at a comparable surface value (0.316 albedo x 0.14
tint) and still reads, because its 4K map carries real local variation and a metal mask; the
alien's does not, so it renders as a silhouette. The fix is a wider bake range, not a tint.

Resolved, and then made moot. The old spider carried a note here claiming Unreal's merge of its
five sub-meshes had dropped its UVs; it had not - any high-contrast map landed with full detail on
every leg segment. The flat body was a `BaseTint` authored as an albedo back when no map was bound
and left in place after one was, so a dark map multiplied by a dark albedo value bottomed out near
0.05, which looks exactly like a single-texel sample. That body has since been replaced outright:
procedural noise on a 6,955-vert mesh was never going to sit beside the quadruped's authored 4K
set, whatever the tint said.

The crawler that replaced it fits two constraints worth recording. Its stride is bounded by its
shortest chain - the side legs rest near extended, and targets past their reach are clamped onto
the reachable sphere rather than strained at. And it is fitted to the ground by the authored
stance's foot plane rather than by its mesh bounds, because its tail hangs 30 units below its
lowest leg: fitting to the mesh floated the whole creature half a capsule off the floor, while
fitting to the stance outright would have scaled it five times too large, since that stance
measures bones spanning 17.7 units on a body whose mesh spans 88.

## Animation playback

These skeletons share nothing with the UE mannequin, so no AnimBlueprint can drive them and
authoring four by hand is four graphs to maintain for a five-clip state machine.
`FAHCreatureAnimationSet` names the five takes on the archetype, and `AAHCombatantCharacter` plays
them through the single-node instance, selecting on ground speed with
`AHCreatureLocomotion::SelectLocomotionState`.

That selection has a fifth of hysteresis either side of each threshold. Enemies spend a fight
accelerating and braking, so they sit on those boundaries constantly and a bare comparison swaps
clip on alternate frames. There is no blending between clips - a single-node instance has nowhere
to blend - so the gait change is a cut.

An attack or death take owns the body until it finishes. Without that hold, a creature that keeps
walking while it bites re-selects a locomotion clip on the next frame and the bite is never seen.
Archetypes with a death take play it and go limp afterwards; archetypes without one ragdoll
immediately, exactly as before.

## Lighting

Each body carries an unshadowed point light on lighting channel 1 so it reads against Erebus's
fog without pooling light on the street. Two things about it were wrong and are worth not
reintroducing:

* It was tinted cold for the Veil faction. Measured on the enemy bench, the bodies came back at
  RGB (81, 84, 83) against a road at (28, 25, 22) - neutral-to-cool objects in a scene where every
  other surface is warm. The faction read comes from the archetype's body now.
* Its offset was authored against a 96uu mannequin half-height. A hound is 44uu at the capsule, so
  a light pinned at z=125 sat inside the near field of an inverse-square falloff and the animal
  measured three and a half times the brightness of the road while every tint knob said it should
  be darker than it. `ApplyBodyFillLightToCapsule` places and ranges it from the capsule instead.
