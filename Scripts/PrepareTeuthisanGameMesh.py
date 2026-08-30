"""Make SKM_Teuthisan_rig_v001 affordable as a gameplay enemy.

- Nanite ON (desktop path): LOD0 keeps all 809,150 verts, Nanite renders it.
- Authored LOD chain (non-Nanite fallback): LOD1 ~20k @ 0.15, LOD2 ~8k @ 0.045,
  LOD3 ~3k @ 0.02. Screen sizes are AUTHORED - this project measured engine-auto
  skeletal LOD screen sizes dropping to LOD2 at ~7m, inside the engagement band.
- LOD1+ strips the tentacle/facial/tongue auxiliary chains, caps influences at 4,
  and drops high-precision tangents / full-precision UVs.

API notes (measured on this 5.8 install, see tmesh/discover.txt, probe2.txt):
- USkeletalMesh.lod_info is NOT reflected to python ("Failed to find property"),
  so per-LOD screen size / reduction / bone removal is authored on a
  SkeletalMeshLODSettings asset and assigned to the mesh; AddLODInfo pulls new
  LODs' settings from that asset when regenerate_lod creates them.
- SkeletalMeshLODGroupSettings / SkeletalMeshOptimizationSettings / BoneFilter
  fields are invisible to dir() and to_dict() but fully readable/writable via
  get_editor_property/set_editor_property.
- nanite_settings is returned BY COPY: mutate the copy, then write the struct back.
- Struct arrays also copy: build the whole lod_groups array, write it back once.

Idempotent: re-running converges - the heavy regenerate is skipped once the
saved mesh already matches the authored chain.

Run with UnrealEditor-Cmd -run=pythonscript. Reports land in Saved/TeuthisanMeshPrep.
"""
import json
import unreal

# Film look-dev vs the game's lighting rig: every combatant carries a 15cd warm fill light,
# and the source MIs arrive quarter-METALLIC (Metalic 0.25-0.5) at roughness 0.10-0.35 with
# spec 0.6-1.0 - a wet bronze mirror under that light, which is exactly how the body rendered
# on the lineup bench: bright, warm, and with none of its 4K detail visible. These overrides
# conform the five zone MIs to the game tier while keeping every film map: organic dielectric,
# matte-but-varying roughness, the roster's spec range, and the baked AO/cavity pulled into
# base colour so the darkening preserves detail instead of flattening it. Limbs also ships
# Global Roughness = -157.447 in the source - a garbage value this pass overwrites.
MATERIAL_GAME_TIER = {
    "Metalic": 0.0,
    "MinRoughness": 0.55,
    "MaxRoughness": 0.90,
    "GlobalRoughness": 1.0,
    "Global Roughness": 1.0,
    "MinSpec": 0.10,
    "MaxSpec": 0.30,
    "Global Spec": 1.0,
    "BaseColorAO": 0.65,
    "BaseColorCavity": 0.5,
}
MATERIAL_ZONES = ("Torso", "Arms", "Legs", "Limbs", "Tentacles")


def conform_materials():
    library = unreal.MaterialEditingLibrary
    for zone in MATERIAL_ZONES:
        path = "/Game/Characters/Teuthisan/Materials/MI_Alien_%s" % zone
        instance = unreal.load_asset(path)
        if not instance:
            raise RuntimeError("missing zone MI: " + path)
        for name, value in MATERIAL_GAME_TIER.items():
            library.set_material_instance_scalar_parameter_value(instance, name, value)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        w("conformed " + path)


TAG = "TMESHPREP"
import os
D = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "TeuthisanMeshPrep")) + "/"
os.makedirs(D, exist_ok=True)
MESH_PATH = "/Game/Characters/Teuthisan/Rig/SKM_Teuthisan_rig_v001"
LODS_DIR = "/Game/Characters/Teuthisan/Rig"
LODS_NAME = "LODS_Teuthisan_GameMesh"
LODS_PATH = LODS_DIR + "/" + LODS_NAME

# (lod_index, target_verts, authored_screen_size). Never engine-auto.
TARGETS = [(1, 20000, 0.15), (2, 8000, 0.045), (3, 3000, 0.02)]
LOD0_SCREEN = 1.0
# Auxiliary chains, classified against the mannequin core (root/pelvis/spine/neck/
# head/clavicle/arm/hand/finger/leg sets incl. the front_/back_ extra limb pairs).
# mouth_* is the 482-bone mouth-tentacle mass; jaw_l/r_N, tongue_N, top_N,
# topEyeNN and throat are the facial rig. All measured in tmesh/bones.json.
STRIP_PREFIXES = ("mouth_", "jaw_", "tongue", "top_", "topeye", "throat")

log = open(D + "prep_report.txt", "w")
def w(s):
    log.write(str(s) + "\n"); log.flush()
    unreal.log_warning("%s %s" % (TAG, str(s)[:400]))

def fail(msg):
    w("FAIL " + msg)
    raise RuntimeError(msg)

sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
mesh = unreal.load_asset(MESH_PATH)
if not mesh:
    fail("mesh not found: " + MESH_PATH)

# ---------------------------------------------------------------- 1. Nanite
ns = mesh.get_editor_property("nanite_settings")  # struct copy
want = {"enabled": True,
        # Pin the fallback to 100% triangles: the fallback IS the non-Nanite LOD0,
        # and the task keeps LOD0 at the full source (death-closeup mesh).
        "fallback_target": unreal.NaniteFallbackTarget.PERCENT_TRIANGLES,
        "fallback_percent_triangles": 1.0}
changed = [k for k, v in want.items() if ns.get_editor_property(k) != v]
if changed:
    for k, v in want.items():
        ns.set_editor_property(k, v)
    mesh.set_editor_property("nanite_settings", ns)  # write-back or it drops
    w("nanite: set %s" % changed)
else:
    w("nanite: already enabled, fallback already pinned to 100%")

# ------------------------------------------------- 2. strip list from skeleton
skel = mesh.get_editor_property("skeleton")
pose = unreal.AnimPoseExtensions.get_reference_pose(skel)
all_bones = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
strip = [n for n in all_bones if n.lower().startswith(STRIP_PREFIXES)]
keep = len(all_bones) - len(strip)
w("bones: %d total, stripping %d on LOD1+, keeping %d" % (len(all_bones), len(strip), keep))
if len(all_bones) != 717:
    w("WARNING: expected 717 bones, found %d" % len(all_bones))
for core in ("root", "pelvis", "head", "spine_01"):
    if core in strip:
        fail("core bone %s classified for stripping" % core)

def bone_filter(name):
    bf = unreal.BoneFilter()
    bf.set_editor_property("bone_name", name)      # fails loudly if field renamed
    bf.set_editor_property("exclude_self", False)  # remove the bone itself too
    return bf

# --------------------------------------------- 3. author the LODSettings asset
lods = unreal.load_asset(LODS_PATH)
if not lods:
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    lods = tools.create_asset(LODS_NAME, LODS_DIR, unreal.SkeletalMeshLODSettings, None)
    if not lods:
        fail("could not create SkeletalMeshLODSettings at " + LODS_PATH)
    w("lods asset: created " + LODS_PATH)
else:
    w("lods asset: reusing " + LODS_PATH)

def make_group(screen, target_verts, max_influences, bones=None):
    g = unreal.SkeletalMeshLODGroupSettings()
    ss = unreal.PerPlatformFloat()
    ss.set_editor_property("default", screen)
    g.set_editor_property("screen_size", ss)
    r = g.get_editor_property("reduction_settings")  # copy
    r.set_editor_property("termination_criterion",
                          unreal.SkeletalMeshTerminationCriterion.SMTC_ABS_NUM_OF_VERTS)
    r.set_editor_property("max_num_of_verts", target_verts)
    r.set_editor_property("max_bones_per_vertex", max_influences)
    r.set_editor_property("base_lod", 0)  # always reduce from the full source
    g.set_editor_property("reduction_settings", r)  # struct write-back
    if bones:
        g.set_editor_property("bone_filter_action_option", unreal.BoneFilterActionOption.REMOVE)
        g.set_editor_property("bone_list", [bone_filter(n) for n in bones])
    return g

# LOD0 group: measured on run 2, leaving group 0's reduction at struct defaults
# (50% triangles) HALVED the base LOD to 406k on the next editor load - the
# PostLoad sync applies group 0 to LODInfo[0] and the DDC rebuild reduces with
# it. Author it as an explicit no-op instead: terminate at 2M verts (above the
# 809k source, collapses nothing) and 12 influences (MAX_TOTAL_INFLUENCES, caps
# nothing). LOD0 keeps the full source.
groups = [make_group(LOD0_SCREEN, 2000000, 12)]
for _idx, verts, screen in TARGETS:
    groups.append(make_group(screen, verts, 4, strip))
lods.set_editor_property("lod_groups", groups)  # whole-array write-back

def sync_settings_to_mesh():
    """Assign (or re-assign) the LODSettings asset. The set_editor_property call
    fires USkeletalMesh::PostEditChangeProperty, which runs SetLODSettingsToMesh
    and copies each lod_group into the matching LODInfo entry. Measured on run 1:
    LODs created BY regenerate_lod do NOT pull their own group (LOD2/3 came out
    at LOD1's 20k target), so this must be re-fired after the entries exist."""
    mesh.set_editor_property("lod_settings", lods)

sync_settings_to_mesh()
w("mesh: lod_settings assigned + synced")

# ------------------------------------------------------------- 4. regenerate
def measured():
    n = sub.get_lod_count(mesh)
    return n, [sub.get_num_verts(mesh, i) for i in range(n)]

count, verts = measured()
w("before: lod_count=%d verts=%s" % (count, verts))
# ponytail: coarse 3x convergence band just to skip the multi-minute regenerate
# on reruns; tighten if targets ever move.
def regenerate():
    ok = sub.regenerate_lod(mesh, new_lod_count=4,
                            regenerate_even_if_imported=False, generate_base_lod=False)
    if not ok:
        fail("regenerate_lod returned False (mesh reduction unavailable?)")
    return measured()

# LOD0 is not judged here: regenerate never touches it (generate_base_lod
# stays False) - its full-vert state is restored by the group-0 no-op above
# on the next DDC build and checked by the separate verify run.
converged = count == 4 and all(
    t / 2.0 <= verts[i] <= t * 2.0 for i, t, _s in TARGETS)
if not converged:
    if count != 4:
        count, verts = regenerate()  # creates the LODInfo entries
        w("after create-regenerate: lod_count=%d verts=%s" % (count, verts))
        sync_settings_to_mesh()      # now push each group onto its own new entry
        w("re-synced lod_settings onto the %d entries" % count)
    count, verts = regenerate()      # reduce with the correct per-LOD settings
    w("after regenerate: lod_count=%d verts=%s" % (count, verts))
else:
    w("regenerate: skipped, already converged")

# ------------------------------------- 5. build settings on LOD1+ (precision)
for i in range(1, count):
    bs = sub.get_lod_build_settings(mesh, i)  # copy
    if bs.get_editor_property("use_high_precision_tangent_basis") or \
       bs.get_editor_property("use_full_precision_u_vs"):
        bs.set_editor_property("use_high_precision_tangent_basis", False)
        bs.set_editor_property("use_full_precision_u_vs", False)
        sub.set_lod_build_settings(mesh, i, bs)
        w("lod%d: cleared high-precision tangent basis + full-precision UVs" % i)
    else:
        w("lod%d: precision flags already cleared" % i)

# ----------------------------------------------- 6. conform the zone materials
conform_materials()

# --------------------------------------------------------------------- 6b. save
if not unreal.EditorAssetLibrary.save_loaded_asset(lods, only_if_is_dirty=False):
    fail("save failed: " + LODS_PATH)
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
    fail("save failed: " + MESH_PATH)
w("saved: mesh + lods asset")

# ------------------------------------------------- 7. same-run read-back dump
count, verts = measured()
rb = {
    "lod_count": count,
    "verts": verts,
    "nanite_enabled": bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled")),
    "lod_settings_on_mesh": str(mesh.get_editor_property("lod_settings").get_path_name()) if mesh.get_editor_property("lod_settings") else None,
    "bones_total": len(all_bones),
    "bones_stripped_lod1plus": len(strip),
    "strip_prefixes": list(STRIP_PREFIXES),
    "groups": [],
}
for g in lods.get_editor_property("lod_groups"):
    r = g.get_editor_property("reduction_settings")
    rb["groups"].append({
        "screen_size": float(g.get_editor_property("screen_size").get_editor_property("default")),
        "termination": str(r.get_editor_property("termination_criterion")),
        "max_num_of_verts": int(r.get_editor_property("max_num_of_verts")),
        "max_bones_per_vertex": int(r.get_editor_property("max_bones_per_vertex")),
        "bone_list_len": len(g.get_editor_property("bone_list")),
    })
json.dump(rb, open(D + "prep_readback.json", "w"), indent=1)
w("READBACK " + json.dumps(rb))
w("%s DONE" % TAG)
