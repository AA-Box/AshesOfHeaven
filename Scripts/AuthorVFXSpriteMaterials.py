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


def author(name, blend_mode, multiply_color_by_radial):
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

    alpha_mult = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, 150)
    MEL.connect_material_expressions(pc, "A", alpha_mult, "A")
    MEL.connect_material_expressions(radial, "", alpha_mult, "B")
    MEL.connect_material_property(alpha_mult, "", unreal.MaterialProperty.MP_OPACITY)

    if multiply_color_by_radial:
        color_mult = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -350, -100)
        MEL.connect_material_expressions(pc, "", color_mult, "A")
        MEL.connect_material_expressions(radial, "", color_mult, "B")
        MEL.connect_material_property(color_mult, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    else:
        MEL.connect_material_property(pc, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("[VFXMat] authored " + name)
    return True


ok = author("M_AH_SmokeSoft", unreal.BlendMode.BLEND_TRANSLUCENT, False)
ok = author("M_AH_FireSprite", unreal.BlendMode.BLEND_ADDITIVE, True) and ok
if not ok:
    unreal.log_error("[VFXMat] sprite material authoring failed")
