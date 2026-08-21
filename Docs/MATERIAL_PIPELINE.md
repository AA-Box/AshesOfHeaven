# Phase 4.2 — Material and VFX pipeline

Phase 4.2 moves presentation identity away from runtime color assignment and prototype-grid-only
materials. The saved asset family is isolated below `/Game/Ashes/Materials`.

## Material masters and instances

Masters:

`M_HumanMetal`, `M_HumanPaintedMetal`, `M_HumanArmor`, `M_Concrete`, `M_WetConcrete`, `M_Glass`,
`M_CathedralMatter`, `M_VeilObsidian`, `M_EmissiveGlyph`, `M_Decal_Master`, `M_Scorch`, and
`M_Grime`.

The masters have real material expression graphs with named BaseTint/Roughness/Metallic
parameters. Saved instances include `MI_HumanMetal_Dark`, `MI_HumanArmor_Black`,
`MI_Concrete_Wet`, `MI_CathedralMatter_Dark`, `MI_VeilObsidian_Black`, and
`MI_EmissiveGlyph_Cyan`.

The Chapter One director loads these assets first and retains explicit old greybox fallbacks so a
missing optional content asset cannot break gameplay construction.

## Niagara family

Cooked-safe saved systems exist for `NS_Ash`, `NS_Embers`, `NS_Sparks`, `NS_FireSmall`,
`NS_FireLarge`, `NS_SmokeColumn`, `NS_Dust`, and `NS_CathedralParticles`. The director uses
`NS_Dust` only when it resolves; it no longer relies on the unsafe prototype Stateless emitter.

Every system must keep scalable spawn/update rates and must be profiled on desktop and mobile.

## Authoring direction

Human/Erebus uses dirty painted metal, concrete, heat, wetness and practical fire. Veil uses
low-reflectance obsidian/ceramic matter and restrained internal illumination. Cathedral uses dark
engineered mass and monumental negative space. Decals, scorch, grime, fog and emissive glyphs are
separate authored families rather than hard-coded runtime colors.

The current assets are an integration target, not a claim that final texture scans, mesh detail,
character materials, smoke, fire, lighting grade, or VFX polish are complete. Human visual review
and production art authoring remain required.
