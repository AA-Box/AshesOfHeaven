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

### `M_ErebusSurface` channel masks

The imported external prop packs pack roughness into an ORM green channel and metalness into
blue, while the Erebus texture family keeps roughness in red. `RoughChannelMask` and
`MetallicChannelMask` select which channel of `RoughTex` feeds each output. Their defaults -
`(1,0,0)` and `(0,0,0)` - reproduce the previous behaviour exactly, so every instance authored
before them is unchanged; the ORM packs set `(0,1,0)` and `(0,0,1)`. `UVTile` also matters
here: the Erebus kit's box-projected UVs want 0.25, imported props carry their own UVs and
want 1.0.

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
