import unreal

"""Author soft sprite materials for the Erebus near-camera Niagara systems.

Opaque surface masters (M_Grime) on sprites render as solid squares — the
'floating cube cloud' artifact the visual gate rejected. These two unlit masters
read ParticleColor and fade by a radial gradient so sprites are soft billows:

  M_AH_SmokeSoft  — translucent, ParticleColor.rgb, alpha * radial falloff
  M_AH_FireSprite — additive, ParticleColor.rgb * radial, alpha * radial
"""

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MAT_DIR = "/Game/Ashes/Materials"
RADIAL_FN = "/Engine/Functions/Engine_MaterialFunctions01/Gradient/RadialGradientExponential"


EROSION_TEX = "/Game/Ashes/Textures/Erebus/T_Erebus_Erosion"


def author(name, blend_mode, multiply_color_by_radial, pan_a, pan_b, gain_value=2.6, lift_value=0.3):
    path = MAT_DIR + "/" + name
    mat = unreal.load_asset(path)
    if not mat:
        factory = unreal.MaterialFactoryNew()
        mat = TOOLS.create_asset(name, MAT_DIR, unreal.Material, factory)
    if not mat:
        unreal.log_error("[VFXMat] failed to create " + name)
        return False
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("blend_mode", blend_mode)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_niagara_sprites", True)
    MEL.delete_all_material_expressions(mat)

    pc = MEL.create_material_expression(mat, unreal.MaterialExpressionParticleColor, -700, -100)
    radial_fn = unreal.load_asset(RADIAL_FN)
    if not radial_fn:
        unreal.log_error("[VFXMat] radial gradient function missing at " + RADIAL_FN)
        return False
    radial = MEL.create_material_expression(mat, unreal.MaterialExpressionMaterialFunctionCall, -700, 250)
    radial.set_editor_property("material_function", radial_fn)

    # Two counter-panning erosion samples break the radial blob into licking,
    # flickering shapes (flame tongues / roiling smoke) instead of a static glow.
    erosion_tex = unreal.load_asset(EROSION_TEX)
    erosion = None
    if erosion_tex:
        texcoord = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1250, 450)
        samples = []
        for index, (sx, sy) in enumerate((pan_a, pan_b)):
            panner = MEL.create_material_expression(mat, unreal.MaterialExpressionPanner, -1050, 400 + index * 180)
            panner.set_editor_property("speed_x", sx)
            panner.set_editor_property("speed_y", sy)
            MEL.connect_material_expressions(texcoord, "", panner, "Coordinate")
            sample = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -850, 400 + index * 180)
            sample.set_editor_property("texture", erosion_tex)
            sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
            MEL.connect_material_expressions(panner, "", sample, "UVs")
            samples.append(sample)
        combined = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -650, 480)
        MEL.connect_material_expressions(samples[0], "R", combined, "A")
        MEL.connect_material_expressions(samples[1], "R", combined, "B")
        gain = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -480, 480)
        gain_const = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -650, 620)
        gain_const.set_editor_property("r", gain_value)
        MEL.connect_material_expressions(combined, "", gain, "A")
        MEL.connect_material_expressions(gain_const, "", gain, "B")
        erosion = gain
    else:
        unreal.log_error("[VFXMat] erosion texture missing at " + EROSION_TEX + "; soft-blob fallback")

    alpha_mult = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 150)
    MEL.connect_material_expressions(pc, "A", alpha_mult, "A")
    MEL.connect_material_expressions(radial, "", alpha_mult, "B")
    alpha_out = alpha_mult
    if erosion:
        eroded_alpha = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -180, 200)
        MEL.connect_material_expressions(alpha_mult, "", eroded_alpha, "A")
        MEL.connect_material_expressions(erosion, "", eroded_alpha, "B")
        alpha_out = eroded_alpha
    MEL.connect_material_property(alpha_out, "", unreal.MaterialProperty.MP_OPACITY)

    if multiply_color_by_radial:
        color_mult = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, -100)
        MEL.connect_material_expressions(pc, "", color_mult, "A")
        MEL.connect_material_expressions(radial, "", color_mult, "B")
        color_out = color_mult
        if erosion:
            # flames brighten where erosion peaks: color * (erosion + 0.3)
            lift = MEL.create_material_expression(mat, unreal.MaterialExpressionAdd, -350, 420)
            lift_const = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -480, 560)
            lift_const.set_editor_property("r", lift_value)
            MEL.connect_material_expressions(erosion, "", lift, "A")
            MEL.connect_material_expressions(lift_const, "", lift, "B")
            shaped = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -180, -60)
            MEL.connect_material_expressions(color_mult, "", shaped, "A")
            MEL.connect_material_expressions(lift, "", shaped, "B")
            color_out = shaped
        MEL.connect_material_property(color_out, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        MEL.connect_material_property(pc, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("[VFXMat] authored " + name)
    return True


ok = author("M_AH_SmokeSoft", unreal.BlendMode.BLEND_TRANSLUCENT, False, (0.015, 0.09), (-0.025, 0.055))
ok = author("M_AH_FireSprite", unreal.BlendMode.BLEND_ADDITIVE, True, (0.04, 0.5), (-0.06, 0.33), gain_value=5.0, lift_value=0.55) and ok
if not ok:
    unreal.log_error("[VFXMat] sprite material authoring failed")
