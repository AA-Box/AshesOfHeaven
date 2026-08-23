import unreal

"""Author M_ErebusSurface: the texture-driven PBR master for the Erebus kit.

Replaces flat tint+shader-noise shading with baked tileable texture sets
(albedo/normal/roughness + damage mask, all procedurally generated). Texture
maps are MI parameters, so one master serves concrete/metal/mud/asphalt.
Scalar/vector parameter NAMES match the old Phase 4.2 masters (BaseTint,
Roughness, Metallic, GrimeAmount, WearAmount, DamageMaskStrength, Wetness) so
existing instances re-parent without losing their tuned values.

Kit meshes carry box-projected UVs at a uniform 100uu scale, so plain
tangent-space sampling with a UV tile factor gives consistent world texel
density without triplanar cost.
"""

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_DIR = "/Game/Ashes/Materials"
TEX_DIR = "/Game/Ashes/Textures/Erebus"

def load(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def author_surface_master():
    full = MAT_DIR + "/M_ErebusSurface"
    material = load(full)
    if not material:
        factory = unreal.MaterialFactoryNew()
        material = TOOLS.create_asset("M_ErebusSurface", MAT_DIR, unreal.Material, factory)
    if not material:
        raise RuntimeError("failed to create M_ErebusSurface")

    def expr(class_name, x, y):
        return MEL.create_material_expression(material, getattr(unreal, class_name), x, y)

    def scalar(name, default, x, y):
        node = expr("MaterialExpressionScalarParameter", x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    def connect(src, out, dst, inp):
        if not MEL.connect_material_expressions(src, out, dst, inp):
            unreal.log_error("[ErebusPBR] connect failed %s -> %s.%s" % (src.get_name(), dst.get_name(), inp))

    # --- UVs -------------------------------------------------------------
    texcoord = expr("MaterialExpressionTextureCoordinate", -1500, -300)
    uv_tile = scalar("UVTile", 0.25, -1500, -180)  # 0.25 => one texture repeat per 4m
    uv_mul = expr("MaterialExpressionMultiply", -1300, -260)
    connect(texcoord, "", uv_mul, "A")
    connect(uv_tile, "", uv_mul, "B")

    # --- texture parameters ----------------------------------------------
    def tex_param(name, default_tex, sampler, x, y):
        node = expr("MaterialExpressionTextureSampleParameter2D", x, y)
        node.set_editor_property("parameter_name", name)
        tex = load(TEX_DIR + "/" + default_tex)
        if tex:
            node.set_editor_property("texture", tex)
        node.set_editor_property("sampler_type", sampler)
        connect(uv_mul, "", node, "UVs")
        return node

    albedo_tex = tex_param("AlbedoTex", "T_Erebus_Concrete_D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1100, -320)
    normal_tex = tex_param("NormalTex", "T_Erebus_Concrete_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1100, 260)
    rough_tex = tex_param("RoughTex", "T_Erebus_Concrete_R", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1100, 20)
    damage_tex = tex_param("DamageTex", "T_Erebus_DamageMask", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1100, 520)

    # --- base color: tint x detail, darkened by grime/wear noise + damage --
    base = expr("MaterialExpressionVectorParameter", -1500, -520)
    base.set_editor_property("parameter_name", "BaseTint")
    base.set_editor_property("default_value", unreal.LinearColor(0.5, 0.5, 0.5, 1.0))
    normalize = expr("MaterialExpressionConstant", -1300, -420)
    normalize.set_editor_property("r", 1.75)  # detail albedo mean ~0.57; keep tint authoritative
    detail_norm = expr("MaterialExpressionMultiply", -880, -360)
    connect(albedo_tex, "RGB", detail_norm, "A")
    connect(normalize, "", detail_norm, "B")
    tinted = expr("MaterialExpressionMultiply", -700, -420)
    connect(detail_norm, "", tinted, "A")
    connect(base, "RGB", tinted, "B")

    # Noise with an unconnected Position input samples absolute world position
    # already; the explicit WorldPosition connect is rejected in 5.8 (input pin
    # is "Position"-less on this node) and unnecessary.
    noise = expr("MaterialExpressionNoise", -1300, 700)
    noise.set_editor_property("scale", 3.6)
    grime = scalar("GrimeAmount", 0.35, -1500, 840)
    wear = scalar("WearAmount", 0.35, -1500, 920)
    grime_mul = expr("MaterialExpressionMultiply", -1100, 760)
    connect(noise, "", grime_mul, "A")
    connect(grime, "", grime_mul, "B")
    wear_mul = expr("MaterialExpressionMultiply", -1100, 880)
    connect(noise, "", wear_mul, "A")
    connect(wear, "", wear_mul, "B")
    weather = expr("MaterialExpressionAdd", -920, 800)
    connect(grime_mul, "", weather, "A")
    connect(wear_mul, "", weather, "B")
    dark = expr("MaterialExpressionMultiply", -700, -280)
    dark_amount = expr("MaterialExpressionConstant", -880, -240)
    dark_amount.set_editor_property("r", 0.30)
    connect(tinted, "", dark, "A")
    connect(dark_amount, "", dark, "B")
    weathered = expr("MaterialExpressionLinearInterpolate", -480, -380)
    connect(tinted, "", weathered, "A")
    connect(dark, "", weathered, "B")
    connect(weather, "", weathered, "Alpha")

    damage_amt = scalar("DamageMaskStrength", 0.0, -1500, 1000)
    damage_mul = expr("MaterialExpressionMultiply", -700, 540)
    connect(damage_tex, "R", damage_mul, "A")
    connect(damage_amt, "", damage_mul, "B")
    based = expr("MaterialExpressionLinearInterpolate", -260, -340)
    connect(weathered, "", based, "A")
    connect(dark, "", based, "B")
    connect(damage_mul, "", based, "Alpha")
    MEL.connect_material_property(based, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # --- roughness: map x param, wetness pulls to glossy -------------------
    rough_param = scalar("Roughness", 0.9, -880, 120)
    rough_mul = expr("MaterialExpressionMultiply", -640, 60)
    connect(rough_tex, "R", rough_mul, "A")
    connect(rough_param, "", rough_mul, "B")
    wetness = scalar("Wetness", 0.0, -880, 200)
    wet_target = expr("MaterialExpressionConstant", -640, 200)
    wet_target.set_editor_property("r", 0.14)
    rough_wet = expr("MaterialExpressionLinearInterpolate", -420, 100)
    connect(rough_mul, "", rough_wet, "A")
    connect(wet_target, "", rough_wet, "B")
    connect(wetness, "", rough_wet, "Alpha")
    MEL.connect_material_property(rough_wet, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # wet surfaces also darken base color slightly via specular boost only; keep simple.
    metallic = scalar("Metallic", 0.0, -880, 420)
    MEL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)

    # --- normal: flatten by NormalStrength ---------------------------------
    strength = scalar("NormalStrength", 1.0, -880, 300)
    flat = expr("MaterialExpressionConstant3Vector", -880, 620)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    normal_mix = expr("MaterialExpressionLinearInterpolate", -420, 320)
    connect(flat, "", normal_mix, "A")
    connect(normal_tex, "RGB", normal_mix, "B")
    connect(strength, "", normal_mix, "Alpha")
    MEL.connect_material_property(normal_mix, "", unreal.MaterialProperty.MP_NORMAL)

    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(full)
    unreal.log("[ErebusPBR] authored M_ErebusSurface")
    return material


author_surface_master()
