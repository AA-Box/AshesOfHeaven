"""Allow the Erebus environment materials to be used on Nanite meshes.

Same class of defect as EnableSkeletalMeshMaterialUsage.py: a material has to opt in to every
mesh type it is applied to. Enabling Nanite on the static architecture without this flag makes the
packaged build log

    Material ... missing usage flag Nanite! Default Material will be used in game.

and silently drop the meshes back to the non-Nanite path - which is why "Nanite Clusters HW" read
0 in r.Shadow.Virtual.ShowStats while 50,291 non-Nanite instances were still being processed, and
why the [VSM] Non-Nanite Marking Job Queue overflow survived the conversion.

Only opaque environment masters are flagged. Sprite/smoke/decal/glass masters are deliberately
left alone: they are translucent or decal-domain and never render as Nanite geometry.
"""

import unreal

MATERIALS = (
    "/Game/Ashes/Materials/M_ErebusSurface",
    "/Game/Ashes/Materials/M_Concrete",
    "/Game/Ashes/Materials/M_WetConcrete",
    "/Game/Ashes/Materials/M_CathedralMatter",
    "/Game/Ashes/Materials/M_Grime",
    "/Game/Ashes/Materials/M_HumanArmor",
    "/Game/Ashes/Materials/M_HumanMetal",
    "/Game/Ashes/Materials/M_HumanPaintedMetal",
    "/Game/Ashes/Materials/M_VeilObsidian",
    # Opaque despite the name - it is the damaged canopy/lens glass on the gunship wreck and the
    # work light, both Nanite meshes. Without the flag those slots fell back to the Default
    # Material, which is a visual regression rather than just a lost optimisation.
    "/Game/Ashes/Materials/M_Glass",
)

report = []
failures = 0
for path in MATERIALS:
    material = unreal.load_asset(path)
    if not material:
        report.append("MISSING " + path)
        failures += 1
        continue
    material.set_editor_property("used_with_nanite", True)
    unreal.EditorAssetLibrary.save_asset(path)
    enabled = unreal.load_asset(path).get_editor_property("used_with_nanite")
    if not enabled:
        failures += 1
    report.append("%s used_with_nanite=%s" % (path.rsplit("/", 1)[-1], enabled))

report.append("total=%d failures=%d" % (len(MATERIALS), failures))
with open("/tmp/nanite_mat_report.txt", "w") as handle:
    handle.write("\n".join(report))
