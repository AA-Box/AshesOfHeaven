# Phase 4.2 — Material and VFX pipeline

Phase 4.2 moves presentation identity away from runtime color assignment and prototype-grid-only
materials. The saved asset family is isolated below `/Game/Ashes/Materials`.

## Material masters and instances

Masters:

`M_HumanMetal`, `M_HumanPaintedMetal`, `M_HumanArmor`, `M_Concrete`, `M_WetConcrete`, `M_Glass`,
`M_CathedralMatter`, `M_VeilObsidian`, `M_EmissiveGlyph`, `M_Decal_Master`, `M_Scorch`, and
`M_Grime`.

The masters have real material expression graphs with named BaseTint/Roughness/Metallic,
GrimeAmount, WearAmount, EdgeVariation, DamageMaskStrength, Wetness, and MicroDetailStrength
parameters, plus world-position noise, tint blending, roughness variation, normal input, and
emissive/decal branches where appropriate. Saved instances include `MI_HumanMetal_Dark`, `MI_HumanArmor_Black`,
`MI_Concrete_Wet`, `MI_CathedralMatter_Dark`, `MI_VeilObsidian_Black`, and
`MI_EmissiveGlyph_Cyan`.

The Chapter One director loads these assets first and retains explicit old greybox fallbacks so a
missing optional content asset cannot break gameplay construction.

### `M_EnemyCreature`

The masters above are flat tints with no texture inputs, which is why the mannequin enemies read
as untextured no matter which one was applied. `M_EnemyCreature` is the textured character master
the four imported enemy bodies use: `BaseColorTex`, `NormalTex`, `RoughnessTex`, `MetallicTex`,
`AOTex` and `EmissiveMask` texture parameters, each gated by a `Use*`/`*Strength` scalar so a model
that shipped without a given map falls back to `BaseTint` and the scalar instead of sampling a
white square. Metallic takes a map as well as a scalar because the quadruped ships a real metal
mask - plating over hide - and collapsing that to one value turns the whole animal into either
rubber or a mirror depending on which way the scalar is pushed.
It carries `used_with_skeletal_mesh`, without which a character body silently renders as the
engine default grey and only logs a warning.

Instances are generated one per material slot (`MI_<Model>_NN`) by `Scripts/ImportEnemyModels.py`.
The slot's own name, taken from the source FBX, decides which texture set dresses it and whether
it is the part that glows - `LIGHT_EYE` and `Red-Eye-Alien-Animal` get the emissive treatment,
`rock` and `LEATHER` get their own maps.

`Docs/ENEMY_CREATURE_PIPELINE.md` covers where those maps come from and the order the authoring
scripts have to run in.

## Niagara family

Authored project emitter/system pairs exist for `NS_AshField`, `NS_EmberDrift`, `NS_ImpactSparks`,
`NS_FireSmall`, `NS_FireLarge`, `NS_SmokeColumn`, `NS_DustSheet`, and `NS_CathedralMotes`.
They are created with Niagara editor factories under `/Game/Ashes/VFX/Emitters` and
`/Game/Ashes/VFX`; no `/Niagara/DefaultAssets/Templates` duplicate is used.

Every system must keep scalable spawn/update rates and must be profiled on desktop and mobile.

## Authoring direction

Human/Erebus uses dirty painted metal, concrete, heat, wetness and practical fire. Veil uses
low-reflectance obsidian/ceramic matter and restrained internal illumination. Cathedral uses dark
engineered mass and monumental negative space. Decals, scorch, grime, fog and emissive glyphs are
separate authored families rather than hard-coded runtime colors.

The current assets are an integration target, not a claim that final texture scans, mesh detail,
character materials, smoke, fire, lighting grade, or VFX polish are complete. Human visual review
and production art authoring remain required.
