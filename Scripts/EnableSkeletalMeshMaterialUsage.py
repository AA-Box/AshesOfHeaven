"""Allow the faction body materials to be used on skeletal meshes.

A material has to opt in to every mesh type it is applied to. These were authored for static
environment geometry, so applying one to a combatant's skeletal body silently fell back to the
engine default material - the characters rendered as untextured grey mannequins and the log
filled with "Material with missing usage flag" warnings. The flag is a property of the parent
material; instances inherit it.
"""

import unreal

MATERIALS = (
    "/Game/Ashes/Materials/M_HumanMetal",
    "/Game/Ashes/Materials/M_VeilObsidian",
)

failures = 0
for path in MATERIALS:
    material = unreal.load_asset(path)
    if not material:
        unreal.log_error("[Material] missing asset " + path)
        failures += 1
        continue
    material.set_editor_property("used_with_skeletal_mesh", True)
    unreal.EditorAssetLibrary.save_asset(path)
    enabled = material.get_editor_property("used_with_skeletal_mesh")
    if not enabled:
        unreal.log_error("[Material] %s still rejects skeletal meshes" % path)
        failures += 1
    else:
        unreal.log_warning("[Material] OK %s used_with_skeletal_mesh=True" % path)

if failures:
    unreal.log_error("[Material] %d material(s) failed" % failures)
