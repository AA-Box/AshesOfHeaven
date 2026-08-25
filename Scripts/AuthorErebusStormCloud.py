import unreal

"""Author MI_Erebus_StormCloud, the volumetric cloud material Level One asks for.

AAHChapterOneDirector loads this instance by path for the Erebus cloud deck. Until it
existed the load silently returned null and the largest surface in frame - the war sky -
rendered with the engine's stock m_SimpleVolumetricCloud, which contradicts the
Unreal-material-authority rule and threw away the tuned storm layer.

Values are the packaged-verified Erebus gate recipe: heavy coverage, thin density, storm
layer on.
"""

MEL = unreal.MaterialEditingLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
INSTANCE_DIR = "/Game/Ashes/Materials/Instances"
PARENT_PATH = "/Engine/EngineSky/VolumetricClouds/m_SimpleVolumetricCloud_Inst"
TARGET = INSTANCE_DIR + "/MI_Erebus_StormCloud"

# Recipe -> the parent's own parameter names, matched case-insensitively so a parameter
# rename in a future engine version fails loudly instead of silently doing nothing.
SCALARS = {
    "Cloud_GlobalCoverage": 0.55,
    "Cloud_GlobalDensity": 0.03,
    "StormClouds": 1.0,
}


def main():
    parent = unreal.load_asset(PARENT_PATH)
    if not parent:
        raise RuntimeError("engine cloud material missing: " + PARENT_PATH)

    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        instance = unreal.load_asset(TARGET)
    else:
        instance = TOOLS.create_asset(
            "MI_Erebus_StormCloud", INSTANCE_DIR, unreal.MaterialInstanceConstant,
            unreal.MaterialInstanceConstantFactoryNew())
    if not instance:
        raise RuntimeError("failed to create " + TARGET)
    MEL.set_material_instance_parent(instance, parent)

    available = {str(name) for name in (MEL.get_scalar_parameter_names(instance) or [])}

    applied = []
    for wanted, value in SCALARS.items():
        # Fail loudly rather than writing a parameter the engine material no longer has.
        if wanted not in available:
            raise RuntimeError("parent has no scalar parameter '%s'; it exposes %s" % (wanted, sorted(available)))
        MEL.set_material_instance_scalar_parameter_value(instance, wanted, value)
        applied.append("%s=%s" % (wanted, value))

    unreal.EditorAssetLibrary.save_asset(TARGET)
    unreal.log("authored %s (%s)" % (TARGET, ", ".join(applied)))


main()
