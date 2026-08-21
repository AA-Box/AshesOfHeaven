import unreal

"""Create the checked-in Unreal presentation asset inventory.

The widget layout itself is intentionally native UMG (UAHHUDRootWidget) so it stays event
driven and cross-platform. These saved WidgetBlueprints are the designer-facing entry points
and can be replaced with authored layouts without changing gameplay code.
"""

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def ensure_folder(path):
    unreal.EditorAssetLibrary.make_directory(path)


def load(path):
    # Direct object loads do not report the expected pre-create cache miss as an editor error.
    return unreal.load_asset(path)


def create_widget(name, path, parent_path):
    full = path + "/" + name
    existing = load(full)
    if existing:
        return existing
    factory = unreal.WidgetBlueprintFactory()
    factory.set_editor_property("parent_class", unreal.load_class(None, parent_path))
    asset = TOOLS.create_asset(name, path, unreal.WidgetBlueprint, factory)
    if not asset:
        unreal.log_error("[Phase4.2] failed widget " + full)
    return asset


def create_data_asset(name, path, class_path):
    full = path + "/" + name
    existing = load(full)
    if existing:
        return existing
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.load_class(None, class_path))
    asset = TOOLS.create_asset(name, path, unreal.DataAsset, factory)
    if not asset:
        unreal.log_error("[Phase4.2] failed data asset " + full)
    return asset


def duplicate(name, source, path):
    full = path + "/" + name
    existing = load(full)
    if existing:
        return existing
    source_asset = load(source) or unreal.load_asset(source)
    if not source_asset:
        unreal.log_warning("[Phase4.2] missing duplicate source " + source)
        return None
    return TOOLS.duplicate_asset(name, path, source_asset)


ensure_folder("/Game/Ashes")
for folder in ["/Game/Ashes/UI/HUD", "/Game/Ashes/UI/Terminal", "/Game/Ashes/Audio/Weapons/M91", "/Game/Ashes/Audio/Environment", "/Game/Ashes/Audio/UI", "/Game/Ashes/Audio/MetaSounds", "/Game/Ashes/Audio/Submixes", "/Game/Ashes/Materials/Instances", "/Game/Ashes/VFX", "/Game/Ashes/Blueprints/Environment", "/Game/Ashes/Presentation"]:
    ensure_folder(folder)

widgets = [
    "WBP_HUD_Root", "WBP_Objective", "WBP_PlayerStatus", "WBP_WeaponStatus",
    "WBP_Crosshair", "WBP_InteractionPrompt", "WBP_DamageIndicator", "WBP_Countdown",
    "WBP_Dialogue", "WBP_TerminalIntel", "WBP_ManticoreHUD", "WBP_ChapterTitle",
]
for widget in widgets:
    create_widget(widget, "/Game/Ashes/UI/HUD", "/Script/AshesOfHeaven.AHHUDRootWidget")
create_widget("WBP_TerminalWorld", "/Game/Ashes/UI/Terminal", "/Script/AshesOfHeaven.AHHUDRootWidget")

prop_assets = [
    "BP_Erebus_BlastWall", "BP_Erebus_PipeCluster", "BP_Erebus_Barricade", "BP_Erebus_Wreck",
    "BP_Transit_Sign", "BP_Transit_Bench", "BP_Cathedral_Fin", "BP_Cathedral_GlyphPanel",
    "BP_Human_ExpeditionLight",
]
for prop in prop_assets:
    if not load("/Game/Ashes/Blueprints/Environment/" + prop):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/AshesOfHeaven.AHPresentationPropActor"))
        TOOLS.create_asset(prop, "/Game/Ashes/Blueprints/Environment", unreal.Blueprint, factory)

# Presentation data assets are real designer-editable assets even when they carry only the
# stable contract in this pass. The maps are populated by a later authored-audio import.
create_data_asset("DA_AudioPalette_Default", "/Game/Ashes/Audio", "/Script/AshesOfHeaven.AHAudioPaletteData")
create_data_asset("DA_AudioPalette_Human", "/Game/Ashes/Audio", "/Script/AshesOfHeaven.AHAudioPaletteData")
create_data_asset("DA_AudioPalette_Veil", "/Game/Ashes/Audio", "/Script/AshesOfHeaven.AHAudioPaletteData")

for name in [
    "DA_WeaponPresentation_M91", "DA_HUDStyle_Default", "DA_EnvironmentStyle_Erebus",
    "DA_EnvironmentStyle_Cathedral",
]:
    class_path = "/Script/AshesOfHeaven.AHWeaponPresentationData" if name.startswith("DA_Weapon") else \
        "/Script/AshesOfHeaven.AHHUDStyleData" if name.startswith("DA_HUD") else \
        "/Script/AshesOfHeaven.AHEnvironmentStyleData"
    existing = load("/Game/Ashes/Presentation/" + name)
    if existing and existing.get_class().get_name() in ("DataAsset", "None"):
        unreal.EditorAssetLibrary.delete_asset("/Game/Ashes/Presentation/" + name)
    create_data_asset(name, "/Game/Ashes/Presentation", class_path)


def create_material(name, path, tint, roughness, metallic=0.0, emissive=False, decal=False):
    full = path + "/" + name
    material = load(full)
    if not material:
        factory = unreal.MaterialFactoryNew()
        material = TOOLS.create_asset(name, path, unreal.Material, factory)
    if not material:
        unreal.log_error("[Phase4.2] failed material " + full)
        return None
    try:
        if decal:
            material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
            material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ALPHA_COMPOSITE)
        base = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -420, -80)
        base.set_editor_property("parameter_name", "BaseTint")
        base.set_editor_property("default_value", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        unreal.MaterialEditingLibrary.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -420, 100)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", roughness)
        unreal.MaterialEditingLibrary.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
        metal = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -420, 260)
        metal.set_editor_property("parameter_name", "Metallic")
        metal.set_editor_property("default_value", metallic)
        unreal.MaterialEditingLibrary.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
        if emissive:
            unreal.MaterialEditingLibrary.connect_material_property(base, "RGB", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        unreal.MaterialEditingLibrary.recompile_material(material)
        unreal.EditorAssetLibrary.save_asset(full)
    except Exception as exc:
        unreal.log_warning("[Phase4.2] material graph warning " + name + ": " + str(exc))
    return material


material_specs = {
    "M_HumanMetal": ((0.16, 0.18, 0.18), 0.48, 0.78, False, False),
    "M_HumanPaintedMetal": ((0.20, 0.22, 0.21), 0.62, 0.55, False, False),
    "M_HumanArmor": ((0.07, 0.08, 0.08), 0.38, 0.82, False, False),
    "M_Concrete": ((0.24, 0.25, 0.24), 0.86, 0.0, False, False),
    "M_WetConcrete": ((0.12, 0.14, 0.14), 0.32, 0.0, False, False),
    "M_Glass": ((0.18, 0.23, 0.24), 0.16, 0.05, False, False),
    "M_CathedralMatter": ((0.035, 0.04, 0.045), 0.72, 0.18, False, False),
    "M_VeilObsidian": ((0.008, 0.012, 0.014), 0.24, 0.35, False, False),
    "M_EmissiveGlyph": ((0.08, 0.34, 0.36), 0.28, 0.1, True, False),
    "M_Decal_Master": ((0.25, 0.06, 0.025), 0.9, 0.0, False, True),
    "M_Scorch": ((0.035, 0.02, 0.012), 0.96, 0.0, False, True),
    "M_Grime": ((0.12, 0.10, 0.07), 0.96, 0.0, False, False),
}
for material_name, values in material_specs.items():
    create_material(material_name, "/Game/Ashes/Materials", *values)

for instance_name, parent_name in [
    ("MI_HumanMetal_Dark", "M_HumanMetal"), ("MI_HumanArmor_Black", "M_HumanArmor"),
    ("MI_Concrete_Wet", "M_WetConcrete"), ("MI_CathedralMatter_Dark", "M_CathedralMatter"),
    ("MI_VeilObsidian_Black", "M_VeilObsidian"), ("MI_EmissiveGlyph_Cyan", "M_EmissiveGlyph"),
]:
    full = "/Game/Ashes/Materials/Instances/" + instance_name
    if not load(full):
        factory = unreal.MaterialInstanceConstantFactoryNew()
        instance = TOOLS.create_asset(instance_name, "/Game/Ashes/Materials/Instances", unreal.MaterialInstanceConstant, factory)
        parent = load("/Game/Ashes/Materials/" + parent_name)
        if instance and parent:
            instance.set_editor_property("parent", parent)
            unreal.EditorAssetLibrary.save_asset(full)


niagara_sources = [
    ("NS_Ash", "DirectionalBurst"), ("NS_Embers", "RadialBurst"), ("NS_Sparks", "DirectionalBurstLightweight"),
    ("NS_FireSmall", "SimpleExplosion"), ("NS_FireLarge", "SimpleExplosion"), ("NS_SmokeColumn", "FountainLightweight"),
    ("NS_Dust", "FountainLightweight"), ("NS_CathedralParticles", "MinimalLightweight"),
]
for target_name, source_name in niagara_sources:
    target = "/Game/Ashes/VFX/" + target_name
    if load(target):
        continue
    source = unreal.load_asset("/Niagara/DefaultAssets/Templates/Systems/" + source_name)
    if source:
        TOOLS.duplicate_asset(target_name, "/Game/Ashes/VFX", source)
    else:
        unreal.log_warning("[Phase4.2] Niagara template unavailable " + source_name)

# Use actual engine SoundCue assets as temporary integration sources rather than runtime
# synthesized PCM. The paths are isolated so they can be replaced by authored recordings.
sound_sources = {
    "SC_M91_Fire": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_M91_Reload": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_M91_Empty": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_M91_Impact": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_UI_Objective": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_UI_Dialogue": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_Erebus_Ambience": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_Transit_Ambience": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_Cathedral_Ambience": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
    "SC_Manticore_Engine": "/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02.FirstPersonTemplateWeaponFire02",
}
for name, source in sound_sources.items():
    target = "/Game/Ashes/Audio/Environment" if "Ambience" in name or "Manticore" in name else "/Game/Ashes/Audio/Weapons/M91" if name.startswith("SC_M91") else "/Game/Ashes/Audio/UI"
    duplicate(name, source, target)

# MetaSound source assets are created by the editor factory when supported by the installed
# engine. They are presentation targets; SoundCue integration sources remain the safe path.
for name in ["MS_M91_Fire", "MS_M91_Impact", "MS_Erebus_Ambience", "MS_Transit_Ambience", "MS_Cathedral_Ambience", "MS_Manticore_Engine", "MS_UI_Objective"]:
    full = "/Game/Ashes/Audio/MetaSounds/" + name
    if not load(full):
        try:
            factory = unreal.MetaSoundSourceFactory()
            asset = TOOLS.create_asset(name, "/Game/Ashes/Audio/MetaSounds", unreal.MetaSoundSource, factory)
            if not asset:
                unreal.log_warning("[Phase4.2] MetaSound factory returned no asset " + full)
        except Exception as exc:
            unreal.log_warning("[Phase4.2] MetaSound unavailable " + name + ": " + str(exc))

# Bind every semantic event to a saved MetaSound source. This is the runtime contract; the
# palette can later be reassigned to authored recordings without changing gameplay call sites.
palette = load("/Game/Ashes/Audio/DA_AudioPalette_Default")
if palette:
    event_assets = {
        "Weapon.M91.Fire": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Fire",
        "Weapon.M91.Reload": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Reload",
        "Weapon.M91.Empty": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Empty",
        "Weapon.M91.Impact": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
        "Combat.Melee": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
        "Combat.Hurt": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
        "Combat.Armor": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
        "Combat.Death": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
        "Combat.Grenade": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
        "UI.Objective": "/Game/Ashes/Audio/UI/SC_UI_Objective",
        "UI.Dialogue": "/Game/Ashes/Audio/UI/SC_UI_Dialogue",
        "UI.Pickup": "/Game/Ashes/Audio/UI/SC_UI_Objective",
        "Player.Footstep": "/Game/Ashes/Audio/Weapons/M91/SC_M91_Impact",
    }
    environments = {
        "Environment.Erebus": "/Game/Ashes/Audio/Environment/SC_Erebus_Ambience",
        "Environment.Transit": "/Game/Ashes/Audio/Environment/SC_Transit_Ambience",
        "Environment.Cathedral": "/Game/Ashes/Audio/Environment/SC_Cathedral_Ambience",
        "Environment.Manticore": "/Game/Ashes/Audio/Environment/SC_Manticore_Engine",
    }
    event_map = {}
    for key, path in event_assets.items():
        asset = unreal.load_asset(path)
        if asset:
            event_map[unreal.Name(key)] = asset
    environment_map = {}
    for key, path in environments.items():
        asset = unreal.load_asset(path)
        if asset:
            environment_map[unreal.Name(key)] = asset
    palette.set_editor_property("events", event_map)
    palette.set_editor_property("environments", environment_map)
    unreal.EditorAssetLibrary.save_asset("/Game/Ashes/Audio/DA_AudioPalette_Default")

unreal.EditorAssetLibrary.save_directory("/Game/Ashes", only_if_is_dirty=True, recursive=True)
unreal.log("[Phase4.2] presentation asset inventory generated")
