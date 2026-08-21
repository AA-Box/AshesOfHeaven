import unreal

"""Create authored Phase 4.2 Unreal presentation assets.

This script orchestrates project content authoring.  The editor-only C++ authoring library owns
the saved Widget Blueprint and Niagara hierarchies; this script handles audio, materials, props,
mix assets, and data assets. Runtime C++ receives state and updates named widgets; it does not
manufacture presentation layout.
"""

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def ensure_folder(path):
    unreal.EditorAssetLibrary.make_directory(path)


def load(path):
    # Direct object loads do not report the expected pre-create cache miss as an editor error.
    return unreal.load_asset(path)


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
for folder in ["/Game/Ashes/UI/HUD", "/Game/Ashes/UI/Terminal", "/Game/Ashes/Audio/Raw", "/Game/Ashes/Audio/Cues", "/Game/Ashes/Audio/Weapons/M91", "/Game/Ashes/Audio/Environment", "/Game/Ashes/Audio/UI", "/Game/Ashes/Audio/MetaSounds", "/Game/Ashes/Audio/Mix", "/Game/Ashes/Audio/Submixes", "/Game/Ashes/Materials/Instances", "/Game/Ashes/VFX", "/Game/Ashes/Blueprints/Environment", "/Game/Ashes/Presentation"]:
    ensure_folder(folder)

for stale_asset in ["/Game/Ashes/Audio/Probe/SC_Probe", "/Game/Ashes/Audio/Probe/MS_Built"]:
    if load(stale_asset):
        unreal.EditorAssetLibrary.delete_asset(stale_asset)

authoring = getattr(unreal, "AHPresentationAuthoringLibrary", None)
if authoring and hasattr(authoring, "author_phase42_widgets"):
    if not authoring.author_phase42_widgets():
        unreal.log_error("[Phase4.2] C++ authored widget generation reported failure")
    if hasattr(authoring, "author_phase42_niagara") and not authoring.author_phase42_niagara():
        unreal.log_error("[Phase4.2] C++ authored Niagara generation reported failure")
else:
    unreal.log_error("[Phase4.2] AHPresentationAuthoringLibrary is unavailable; editor module must be rebuilt")

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

        # Do not call DeleteAllMaterialExpressions here: in UE 5.8 commandlets it can
        # attempt to unroot editor-owned expressions.  Add the authored graph once and
        # leave it intact on subsequent runs.
        try:
            if len(material.get_editor_property("expressions")) >= 12:
                return material
        except Exception:
            pass

        def expr(class_name, x, y):
            expression_class = getattr(unreal, class_name, None)
            if not expression_class:
                unreal.log_warning("[Phase4.2] material expression unavailable " + class_name)
                return None
            return unreal.MaterialEditingLibrary.create_material_expression(material, expression_class, x, y)

        def scalar(parameter, default, x, y):
            node = expr("MaterialExpressionScalarParameter", x, y)
            if node:
                node.set_editor_property("parameter_name", parameter)
                node.set_editor_property("default_value", default)
            return node

        base = expr("MaterialExpressionVectorParameter", -900, -120)
        base.set_editor_property("parameter_name", "BaseTint")
        base.set_editor_property("default_value", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        dark = expr("MaterialExpressionConstant3Vector", -900, 20)
        dark.set_editor_property("constant", unreal.LinearColor(tint[0] * 0.28, tint[1] * 0.28, tint[2] * 0.28, 1.0))
        world = expr("MaterialExpressionWorldPosition", -900, 220)
        noise = expr("MaterialExpressionNoise", -680, 220)
        if noise and world:
            try:
                noise.set_editor_property("scale", 3.6)
            except Exception:
                pass
            unreal.MaterialEditingLibrary.connect_material_expressions(world, "", noise, "Position")

        grime = scalar("GrimeAmount", 0.34, -900, 420)
        scalar("WearAmount", 0.22, -900, 500)
        scalar("EdgeVariation", 0.18, -900, 580)
        scalar("DamageMaskStrength", 0.0, -900, 660)
        scalar("Wetness", 0.0 if roughness > 0.5 else 0.2, -900, 740)
        scalar("MicroDetailStrength", 0.16, -900, 820)

        grime_mul = expr("MaterialExpressionMultiply", -460, 260)
        if grime_mul and noise and grime:
            unreal.MaterialEditingLibrary.connect_material_expressions(noise, "", grime_mul, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(grime, "", grime_mul, "B")
        tint_mix = expr("MaterialExpressionLinearInterpolate", -220, -80)
        if tint_mix and base and dark and grime_mul:
            unreal.MaterialEditingLibrary.connect_material_expressions(base, "RGB", tint_mix, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(dark, "", tint_mix, "B")
            unreal.MaterialEditingLibrary.connect_material_expressions(grime_mul, "", tint_mix, "Alpha")
        base_output = tint_mix or base
        if base_output:
            unreal.MaterialEditingLibrary.connect_material_property(base_output, "", unreal.MaterialProperty.MP_BASE_COLOR)

        rough_param = scalar("Roughness", roughness, -420, 420)
        rough_add = expr("MaterialExpressionAdd", -180, 420)
        if rough_add and rough_param and noise:
            unreal.MaterialEditingLibrary.connect_material_expressions(rough_param, "", rough_add, "A")
            unreal.MaterialEditingLibrary.connect_material_expressions(noise, "", rough_add, "B")
        if rough_add or rough_param:
            unreal.MaterialEditingLibrary.connect_material_property(rough_add or rough_param, "", unreal.MaterialProperty.MP_ROUGHNESS)

        metal = scalar("Metallic", metallic, -420, 560)
        if metal:
            unreal.MaterialEditingLibrary.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)

        normal = expr("MaterialExpressionConstant3Vector", -260, 680)
        if normal:
            normal.set_editor_property("constant", unreal.LinearColor(0.5, 0.5, 1.0, 1.0))
            unreal.MaterialEditingLibrary.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

        if emissive and base_output:
            fresnel = expr("MaterialExpressionFresnel", -180, 820)
            emissive_mul = expr("MaterialExpressionMultiply", 40, 820)
            if fresnel and emissive_mul:
                unreal.MaterialEditingLibrary.connect_material_expressions(base_output, "", emissive_mul, "A")
                unreal.MaterialEditingLibrary.connect_material_expressions(fresnel, "", emissive_mul, "B")
                unreal.MaterialEditingLibrary.connect_material_property(emissive_mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if decal:
            opacity = scalar("DecalOpacity", 0.72, -180, 980)
            if opacity:
                unreal.MaterialEditingLibrary.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)

        unreal.MaterialEditingLibrary.layout_material_expressions(material)
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


# Niagara systems are authored by AHPresentationAuthoringLibrary using the engine factories.
# This keeps the checked-in generator free of template duplication and makes the emitter graph
# an editable project asset with its own identity.

# Build a complete authored audio route. Raw WAVs are imported as project SoundWaves, then
# wrapped in actual SoundCues with attenuation/concurrency/submix routing and in MetaSound
# sources for event variation. No engine template or runtime-generated PCM is used.
raw_paths = {
    "SC_M91_Fire": "/Game/Ashes/Audio/Raw/SC_M91_Fire",
    "SC_M91_Reload": "/Game/Ashes/Audio/Raw/SC_M91_Reload",
    "SC_M91_Empty": "/Game/Ashes/Audio/Raw/SC_M91_Empty",
    "SC_M91_Impact": "/Game/Ashes/Audio/Raw/SC_M91_Impact",
    "SC_UI_Objective": "/Game/Ashes/Audio/Raw/SC_UI_Objective",
    "SC_UI_Dialogue": "/Game/Ashes/Audio/Raw/SC_UI_Dialogue",
    "SC_UI_Pickup": "/Game/Ashes/Audio/Raw/SC_UI_Pickup",
    "SC_Player_Footstep": "/Game/Ashes/Audio/Raw/SC_Player_Footstep",
    "SC_Erebus_Ambience": "/Game/Ashes/Audio/Raw/SC_Erebus_Ambience",
    "SC_Transit_Ambience": "/Game/Ashes/Audio/Raw/SC_Transit_Ambience",
    "SC_Cathedral_Ambience": "/Game/Ashes/Audio/Raw/SC_Cathedral_Ambience",
    "SC_Manticore_Engine": "/Game/Ashes/Audio/Raw/SC_Manticore_Engine",
}

def create_mix_asset(name, path, asset_class, factory):
    full = path + "/" + name
    asset = load(full)
    if not asset:
        asset = TOOLS.create_asset(name, path, asset_class, factory)
    if asset:
        unreal.EditorAssetLibrary.save_asset(full)
    return asset


world_attenuation = create_mix_asset("ATT_World3D", "/Game/Ashes/Audio/Mix", unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
ui_attenuation = create_mix_asset("ATT_UI", "/Game/Ashes/Audio/Mix", unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
world_concurrency = create_mix_asset("CONC_World", "/Game/Ashes/Audio/Mix", unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
ui_concurrency = create_mix_asset("CONC_UI", "/Game/Ashes/Audio/Mix", unreal.SoundConcurrency, unreal.SoundConcurrencyFactory())
master_submix = create_mix_asset("SM_Master", "/Game/Ashes/Audio/Mix", unreal.SoundSubmix, unreal.SoundSubmixFactory())
world_submix = create_mix_asset("SM_World", "/Game/Ashes/Audio/Submixes", unreal.SoundSubmix, unreal.SoundSubmixFactory())
ui_submix = create_mix_asset("SM_UI", "/Game/Ashes/Audio/Submixes", unreal.SoundSubmix, unreal.SoundSubmixFactory())

if world_concurrency:
    try:
        world_concurrency.set_max_count(32)
    except Exception:
        pass
if ui_concurrency:
    try:
        ui_concurrency.set_max_count(8)
    except Exception:
        pass
if world_submix and master_submix:
    try:
        world_submix.set_editor_property("parent_submix", master_submix)
    except Exception:
        pass
if ui_submix and master_submix:
    try:
        ui_submix.set_editor_property("parent_submix", master_submix)
    except Exception:
        pass


def create_sound_cue(name, raw_path, attenuation, concurrency, submix):
    cue_path = "/Game/Ashes/Audio/Cues/" + name
    existing = load(cue_path)
    if existing and existing.get_class().get_name() != "SoundCue":
        unreal.EditorAssetLibrary.delete_asset(cue_path)
        existing = None
    if not existing:
        existing = TOOLS.create_asset(name, "/Game/Ashes/Audio/Cues", unreal.SoundCue, unreal.SoundCueFactoryNew())
    wave_asset = load(raw_path)
    if not existing or not wave_asset:
        unreal.log_warning("[Phase4.2][Audio] missing source for cue " + name)
        return None
    try:
        wave_node = unreal.new_object(unreal.SoundNodeWavePlayer, existing)
        wave_node.set_editor_property("sound_wave_asset_ptr", wave_asset)
        if hasattr(unreal, "SoundNodeAttenuation"):
            attenuation_node = unreal.new_object(unreal.SoundNodeAttenuation, existing)
            attenuation_node.set_editor_property("child_nodes", [wave_node])
            try:
                attenuation_node.set_editor_property("attenuation_settings", attenuation)
            except Exception:
                pass
            existing.set_editor_property("first_node", attenuation_node)
        else:
            existing.set_editor_property("first_node", wave_node)
        # Use the authored USoundConcurrency asset rather than the local override struct.
        existing.set_editor_property("override_concurrency", False)
        if concurrency:
            existing.set_editor_property("concurrency_set", {concurrency})
        if submix:
            existing.set_editor_property("sound_submix_object", submix)
        unreal.EditorAssetLibrary.save_asset(cue_path)
        return existing
    except Exception as exc:
        unreal.log_warning("[Phase4.2][Audio] SoundCue graph warning " + name + ": " + str(exc))
        return existing


cue_paths = {}
for source_name, raw_path in raw_paths.items():
    is_ui = source_name.startswith("SC_UI") or source_name == "SC_Player_Footstep"
    cue = create_sound_cue(source_name, raw_path, ui_attenuation if is_ui else world_attenuation, ui_concurrency if is_ui else world_concurrency, ui_submix if is_ui else world_submix)
    if cue:
        cue_paths[source_name] = cue.get_path_name().split(".")[0]


def create_metasound(name, raw_path, submix):
    full = "/Game/Ashes/Audio/MetaSounds/" + name
    try:
        if load(full):
            unreal.EditorAssetLibrary.delete_asset(full)
        editor_subsystem = unreal.get_editor_subsystem(unreal.MetaSoundEditorSubsystem)
        builder_subsystem = unreal.get_engine_subsystem(unreal.MetaSoundBuilderSubsystem)
        builder_tuple = builder_subsystem.create_source_builder("Phase42_" + name)
        builder = builder_tuple[0]
        node_tuple = builder.add_node_by_class_name(unreal.MetasoundFrontendClassName(namespace="UE", name="Wave Player", variant="Mono"))
        node = node_tuple[0]
        wave_asset = load(raw_path)
        literal = builder_subsystem.create_object_meta_sound_literal(wave_asset)
        literal = literal[0] if isinstance(literal, tuple) else literal
        input_handle = builder.find_node_input_by_name(node, "Wave Asset")
        input_handle = input_handle[0] if isinstance(input_handle, tuple) else input_handle
        builder.set_node_input_default(input_handle, literal)
        outputs = builder.get_graph_output_names()[0]
        builder.connect_named_node_output_to_named_graph_output(node, "Out Mono", outputs[1])
        built = editor_subsystem.build_to_asset(builder, "Ashes of Heaven", name, "/Game/Ashes/Audio/MetaSounds", template_sound_wave=wave_asset)
        asset = built[0] if isinstance(built, tuple) else built
        if asset and submix:
            try:
                asset.set_editor_property("sound_submix_object", submix)
            except Exception:
                pass
        if asset:
            unreal.EditorAssetLibrary.save_asset(full)
            return asset
    except Exception as exc:
        unreal.log_warning("[Phase4.2][Audio] MetaSound authoring deferred for " + name + ": " + str(exc))
    return None


meta_paths = {}
for source_name, raw_path in raw_paths.items():
    meta_name = source_name.replace("SC_", "MS_", 1)
    asset = create_metasound(meta_name, raw_path, ui_submix if source_name.startswith("SC_UI") else world_submix)
    if asset:
        meta_paths[source_name] = asset.get_path_name().split(".")[0]

# Bind every semantic event to an authored MetaSound when it is available. SoundCue paths remain
# in the same palette as a packaged-safe fallback for a partially imported editor session.
palette = load("/Game/Ashes/Audio/DA_AudioPalette_Default")
if palette:
    event_assets = {
        "Weapon.M91.Fire": "SC_M91_Fire", "Weapon.M91.Reload": "SC_M91_Reload", "Weapon.M91.Empty": "SC_M91_Empty",
        "Weapon.M91.Impact": "SC_M91_Impact", "Combat.Melee": "SC_M91_Impact", "Combat.Hurt": "SC_M91_Impact",
        "Combat.Armor": "SC_M91_Impact", "Combat.Death": "SC_M91_Impact", "Combat.Grenade": "SC_M91_Impact",
        "UI.Objective": "SC_UI_Objective", "UI.Dialogue": "SC_UI_Dialogue", "UI.Pickup": "SC_UI_Pickup",
        "Player.Footstep": "SC_Player_Footstep",
    }
    environments = {
        "Environment.Erebus": "SC_Erebus_Ambience", "Environment.Transit": "SC_Transit_Ambience",
        "Environment.Cathedral": "SC_Cathedral_Ambience", "Environment.Manticore": "SC_Manticore_Engine",
    }
    event_map = {}
    for key, source_name in event_assets.items():
        path = meta_paths.get(source_name, cue_paths.get(source_name, raw_paths.get(source_name)))
        asset = unreal.load_asset(path) if path else None
        if asset:
            event_map[unreal.Name(key)] = asset
    environment_map = {}
    for key, source_name in environments.items():
        path = meta_paths.get(source_name, cue_paths.get(source_name, raw_paths.get(source_name)))
        asset = unreal.load_asset(path) if path else None
        if asset:
            environment_map[unreal.Name(key)] = asset
    palette.set_editor_property("events", event_map)
    palette.set_editor_property("environments", environment_map)
    unreal.EditorAssetLibrary.save_asset("/Game/Ashes/Audio/DA_AudioPalette_Default")

unreal.EditorAssetLibrary.save_directory("/Game/Ashes", only_if_is_dirty=True, recursive=True)
for stale_asset in ["/Game/Ashes/Audio/Probe/SC_Probe", "/Game/Ashes/Audio/Probe/MS_Built"]:
    if load(stale_asset):
        unreal.EditorAssetLibrary.delete_asset(stale_asset)
unreal.log("[Phase4.2] presentation asset inventory generated")
