import os

import unreal

"""Author the Erebus modular environment kit as real StaticMesh assets.

Geometry Script builds each module (bevels, insets, boolean damage, noise) so the
battlefield reads as manufactured military architecture instead of scaled engine
primitives. Material instances ride the Phase 4.2 master parameters. Idempotent:
existing assets are updated in place (CopyMeshToStaticMesh) so level references
survive regeneration.
"""

PRIM = unreal.GeometryScript_Primitives
BOOL = unreal.GeometryScript_MeshBooleans
MODEL = unreal.GeometryScript_MeshModeling
XFORM = unreal.GeometryScript_MeshTransforms
DEFORM = unreal.GeometryScript_MeshDeformers
NORM = unreal.GeometryScript_Normals
UVS = unreal.GeometryScript_UVs
MATS = unreal.GeometryScript_Materials
NEWASSET = unreal.GeometryScript_NewAssetUtils
ASSET = unreal.GeometryScript_AssetUtils
SEL = unreal.GeometryScript_MeshSelection
EDITS = unreal.GeometryScript_MeshEdits
QUERY = unreal.GeometryScript_MeshQueries

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MESH_DIR = "/Game/Ashes/Environment/Erebus/Meshes"
INST_DIR = "/Game/Ashes/Materials/Instances"
REPORT = []


def ensure_folder(path):
    unreal.EditorAssetLibrary.make_directory(path)


def xf(loc=(0.0, 0.0, 0.0), rot=(0.0, 0.0, 0.0), scale=(1.0, 1.0, 1.0)):
    """rot = (roll, pitch, yaw) degrees."""
    t = unreal.Transform()
    t.translation = unreal.Vector(loc[0], loc[1], loc[2])
    t.rotation = unreal.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]).quaternion()
    t.scale3d = unreal.Vector(scale[0], scale[1], scale[2])
    return t


def new_mesh():
    m = unreal.DynamicMesh()
    MATS.enable_material_i_ds(m)
    return m


def prim_opts(material_id=0):
    o = unreal.GeometryScriptPrimitiveOptions()
    o.material_id = material_id
    return o


def box(mesh, loc, dx, dy, dz, rot=(0, 0, 0), mat=0, steps=0):
    PRIM.append_box(mesh, prim_opts(mat), xf(loc, rot), dx, dy, dz, steps, steps, steps)
    return mesh


def cyl(mesh, loc, radius, height, rot=(0, 0, 0), mat=0, radial_steps=14):
    PRIM.append_cylinder(mesh, prim_opts(mat), xf(loc, rot), radius, height, radial_steps)
    return mesh


def cone(mesh, loc, base_r, top_r, height, rot=(0, 0, 0), mat=0, radial_steps=12):
    PRIM.append_cone(mesh, prim_opts(mat), xf(loc, rot), base_r, top_r, height, radial_steps)
    return mesh


def capsule(mesh, loc, radius, line_length, rot=(0, 0, 0), mat=0):
    PRIM.append_capsule(mesh, prim_opts(mat), xf(loc, rot), radius, line_length, 4, 8)
    return mesh


def sphere(mesh, loc, radius, rot=(0, 0, 0), mat=0, steps=6):
    PRIM.append_sphere_box(mesh, prim_opts(mat), xf(loc, rot), radius, steps, steps, steps)
    return mesh


def tool_box(loc, dx, dy, dz, rot=(0, 0, 0)):
    tool = unreal.DynamicMesh()
    MATS.enable_material_i_ds(tool)
    # Center origin so subtraction tools are easy to aim.
    PRIM.append_box(tool, prim_opts(0), xf(loc, rot), dx, dy, dz, 0, 0, 0,
                    unreal.GeometryScriptPrimitiveOriginMode.CENTER)
    return tool


def subtract(mesh, tool):
    opts = unreal.GeometryScriptMeshBooleanOptions()
    opts.fill_holes = True
    BOOL.apply_mesh_boolean(mesh, xf(), tool, xf(), unreal.GeometryScriptBooleanOperation.SUBTRACT, opts)
    return mesh


# Plane-cut side calibration: engine convention for which half survives is not worth
# memorizing; measure it once with a probe cube and wrap it.
_CUT_KEEPS_POSITIVE = None


def _calibrate_cut():
    global _CUT_KEEPS_POSITIVE
    probe = unreal.DynamicMesh()
    PRIM.append_box(probe, unreal.GeometryScriptPrimitiveOptions(), xf((0, 0, -50)), 100, 100, 100)
    BOOL.apply_mesh_plane_cut(probe, xf(), unreal.GeometryScriptMeshPlaneCutOptions())
    bbox = QUERY.get_mesh_bounding_box(probe)
    _CUT_KEEPS_POSITIVE = bbox.max.z > 1.0
    unreal.log("[ErebusKit] plane cut keeps positive side: %s" % _CUT_KEEPS_POSITIVE)


def cut_keep_below(mesh, loc, rot=(0, 0, 0)):
    """Remove everything on the +Z side of the cut frame."""
    opts = unreal.GeometryScriptMeshPlaneCutOptions()
    opts.fill_holes = True
    opts.flip_cut_side = bool(_CUT_KEEPS_POSITIVE)
    BOOL.apply_mesh_plane_cut(mesh, xf(loc, rot), opts)
    return mesh


def noise(mesh, magnitude, frequency, seed=7, above_z=None):
    options = unreal.GeometryScriptPerlinNoiseOptions()
    layer = unreal.GeometryScriptPerlinNoiseLayerOptions()
    layer.magnitude = magnitude
    layer.frequency = frequency
    layer.random_seed = seed
    options.base_layer = layer
    selection = unreal.GeometryScriptMeshSelection()
    if above_z is not None:
        result = SEL.select_mesh_elements_with_plane(
            mesh, unreal.Vector(0, 0, above_z), unreal.Vector(0, 0, 1),
            unreal.GeometryScriptMeshSelectionType.VERTICES)
        selection = result[1] if isinstance(result, tuple) else result
    DEFORM.apply_perlin_noise_to_mesh2(mesh, selection, options)
    return mesh


def bevel(mesh, distance=3.0):
    opts = unreal.GeometryScriptMeshBevelOptions()
    opts.bevel_distance = distance
    MODEL.apply_mesh_polygroup_bevel(mesh, opts)
    return mesh


def finalize(name, mesh, materials):
    """Split normals, tangents, box UVs, then create/update the StaticMesh asset."""
    split = unreal.GeometryScriptSplitNormalsOptions()
    split.split_by_opening_angle = True
    split.opening_angle_deg = 35.0
    calc = unreal.GeometryScriptCalculateNormalsOptions()
    NORM.compute_split_normals(mesh, split, calc)
    NORM.compute_tangents(mesh, unreal.GeometryScriptTangentsOptions())
    UVS.set_num_uv_sets(mesh, 1)
    UVS.set_mesh_u_vs_from_box_projection(
        mesh, 0, xf(scale=(100.0, 100.0, 100.0)), unreal.GeometryScriptMeshSelection())

    path = MESH_DIR + "/" + name
    existing = unreal.load_asset(path)
    if existing:
        copy_opts = unreal.GeometryScriptCopyMeshToAssetOptions()
        result = ASSET.copy_mesh_to_static_mesh(
            mesh, existing, copy_opts, unreal.GeometryScriptMeshWriteLOD(), False)
        outcome = result[1] if isinstance(result, tuple) else None
        asset = existing
    else:
        create_opts = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        create_opts.enable_collision = False
        create_opts.enable_recompute_normals = False
        create_opts.enable_recompute_tangents = False
        create_opts.enable_nanite = False
        result = NEWASSET.create_new_static_mesh_asset_from_mesh(mesh, path, create_opts)
        asset = result[0] if isinstance(result, tuple) else result
        outcome = result[1] if isinstance(result, tuple) else None
    if not asset:
        unreal.log_error("[ErebusKit] FAILED to author " + name + " outcome=" + str(outcome))
        REPORT.append("%s FAILED" % name)
        return None

    slots = []
    for index, (mi_path, slot_name) in enumerate(materials):
        material = unreal.load_asset(mi_path)
        if not material:
            unreal.log_error("[ErebusKit] missing material %s for %s" % (mi_path, name))
        slot = unreal.StaticMaterial()
        slot.set_editor_property("material_interface", material)
        slot.set_editor_property("material_slot_name", slot_name)
        slots.append(slot)
    asset.set_editor_property("static_materials", slots)
    unreal.EditorAssetLibrary.save_asset(path)

    bbox = QUERY.get_mesh_bounding_box(mesh)
    tris = QUERY.get_num_triangle_i_ds(mesh) if hasattr(QUERY, "get_num_triangle_i_ds") else -1
    if isinstance(tris, tuple):
        tris = tris[1]
    REPORT.append("%s tris=%s bbox=(%.0f,%.0f,%.0f)-(%.0f,%.0f,%.0f) slots=%d" % (
        name, tris, bbox.min.x, bbox.min.y, bbox.min.z, bbox.max.x, bbox.max.y, bbox.max.z, len(slots)))
    return asset


# ---------------------------------------------------------------------------
# Material instances (ride the Phase 4.2 master parameters, no new masters).
# ---------------------------------------------------------------------------
MEL = unreal.MaterialEditingLibrary


def make_instance(name, parent_name, tint, scalars):
    full = INST_DIR + "/" + name
    inst = unreal.load_asset(full)
    if not inst:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        inst = TOOLS.create_asset(name, INST_DIR, unreal.MaterialInstanceConstant, factory)
    if not inst:
        unreal.log_error("[ErebusKit] failed material instance " + name)
        return None
    parent = unreal.load_asset("/Game/Ashes/Materials/" + parent_name)
    if parent:
        inst.set_editor_property("parent", parent)
    if tint:
        MEL.set_material_instance_vector_parameter_value(
            inst, "BaseTint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    for key, value in scalars.items():
        MEL.set_material_instance_scalar_parameter_value(inst, key, value)
    MEL.update_material_instance(inst)
    unreal.EditorAssetLibrary.save_asset(full)
    return inst


INSTANCES = {
    # name: (parent master, BaseTint, scalar params)
    # Values tuned against packaged captures: the first pass read pale/speckled on
    # concrete (tint too high + grime noise) and glossy on mud (wetness too high).
    "MI_Erebus_Concrete_Dry":    ("M_Concrete",        (0.022, 0.022, 0.020), {"Roughness": 0.86, "GrimeAmount": 0.30, "WearAmount": 0.30, "Wetness": 0.0}),
    "MI_Erebus_Concrete_Wet":    ("M_WetConcrete",     (0.055, 0.058, 0.060), {"Roughness": 0.34, "GrimeAmount": 0.45, "Wetness": 0.55}),
    "MI_Erebus_Concrete_Burned": ("M_Concrete",        (0.042, 0.040, 0.037), {"Roughness": 0.92, "GrimeAmount": 0.65, "DamageMaskStrength": 0.55, "Wetness": 0.0}),
    "MI_Erebus_Steel_Dark":      ("M_HumanMetal",      (0.062, 0.065, 0.068), {"Roughness": 0.55, "Metallic": 0.75, "GrimeAmount": 0.45, "WearAmount": 0.40}),
    "MI_Erebus_Steel_Painted":   ("M_HumanPaintedMetal", (0.075, 0.082, 0.075), {"Roughness": 0.62, "Metallic": 0.35, "GrimeAmount": 0.40, "WearAmount": 0.50}),
    "MI_Erebus_Steel_Scorched":  ("M_HumanMetal",      (0.022, 0.020, 0.019), {"Roughness": 0.90, "Metallic": 0.35, "GrimeAmount": 0.85, "DamageMaskStrength": 0.65}),
    "MI_Erebus_Mud":             ("M_WetConcrete",     (0.010, 0.009, 0.008), {"Roughness": 0.85, "GrimeAmount": 0.55, "Wetness": 0.0}),
    "MI_Erebus_WreckMetal":      ("M_HumanMetal",      (0.085, 0.060, 0.045), {"Roughness": 0.80, "Metallic": 0.55, "GrimeAmount": 0.75, "DamageMaskStrength": 0.70}),
    "MI_Erebus_Rubber":          ("M_HumanArmor",      (0.020, 0.020, 0.021), {"Roughness": 0.90, "Metallic": 0.0, "GrimeAmount": 0.60}),
    "MI_Erebus_Glass_Damaged":   ("M_Glass",           (0.030, 0.038, 0.042), {"Roughness": 0.35, "GrimeAmount": 0.50}),
    "MI_Erebus_Puddle":          ("M_Glass",           (0.010, 0.012, 0.016), {"Roughness": 0.05, "Wetness": 1.0}),
    "MI_Erebus_RuinDark":        ("M_Concrete",        (0.036, 0.038, 0.042), {"Roughness": 0.88, "GrimeAmount": 0.60}),
    "MI_Erebus_CathedralSilhouette": ("M_VeilObsidian", (0.020, 0.021, 0.024), {"Roughness": 0.85}),
    "MI_Erebus_BannerCloth":     ("M_HumanArmor",      (0.030, 0.030, 0.032), {"Roughness": 0.95, "Metallic": 0.0}),
    "MI_Erebus_BannerEmblem":    ("M_HumanMetal",      (0.300, 0.310, 0.285), {"Roughness": 0.85, "Metallic": 0.08, "GrimeAmount": 0.35}),
    "MI_Erebus_Decal_Scorch":    ("M_Scorch",          (0.030, 0.018, 0.010), {"DecalOpacity": 0.85}),
    "MI_Erebus_Decal_Grime":     ("M_Decal_Master",    (0.055, 0.050, 0.042), {"DecalOpacity": 0.65}),
}

# Shorthand paths used by the mesh builders.
M = {key: INST_DIR + "/" + key for key in INSTANCES}


# ---------------------------------------------------------------------------
# Mesh builders. All dims in uu, pivot at base center unless noted.
# ---------------------------------------------------------------------------

def build_blastwall_a():
    m = new_mesh()
    box(m, (0, 0, 0), 440, 92, 38, mat=0)              # foot flare
    box(m, (0, 0, 36), 420, 62, 226, mat=0)            # body
    box(m, (0, 0, 258), 442, 82, 40, mat=1)            # steel cap
    for px in (-140, 0, 140):                          # inset panels both faces
        subtract(m, tool_box((px, -32, 150), 112, 14, 150))
        subtract(m, tool_box((px, 32, 150), 112, 14, 150))
    cut_keep_below(m, (200, 0, 288), (0, -18, 8))      # chipped top corner
    bevel(m, 3.0)
    return finalize("SM_Erebus_BlastWall_A", m,
                    [(M["MI_Erebus_Concrete_Dry"], "Concrete"), (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_blastwall_b():
    m = new_mesh()
    box(m, (0, 0, 0), 440, 92, 38, mat=0)
    box(m, (0, 0, 36), 420, 62, 226, mat=0)
    box(m, (0, 0, 258), 442, 82, 40, mat=1)
    subtract(m, tool_box((-170, 0, 300), 200, 140, 160, rot=(12, 0, 24)))  # blast bite
    subtract(m, tool_box((60, -30, 120), 90, 20, 110))
    noise(m, 2.5, 0.015, seed=31)
    return finalize("SM_Erebus_BlastWall_B", m,
                    [(M["MI_Erebus_Concrete_Burned"], "Concrete"), (M["MI_Erebus_Steel_Scorched"], "Steel")])


def build_bunkerwall_a():
    m = new_mesh()
    box(m, (0, 0, 0), 600, 84, 350, mat=0, steps=2)
    for gz in (115, 230):                              # horizontal pour seams
        subtract(m, tool_box((0, -44, gz), 620, 10, 7))
        subtract(m, tool_box((0, 44, gz), 620, 10, 7))
    subtract(m, tool_box((0, 0, 268), 260, 120, 44))   # firing slit
    bevel(m, 3.0)
    return finalize("SM_Erebus_BunkerWall_A", m, [(M["MI_Erebus_Concrete_Dry"], "Concrete")])


def build_bunkercorner_a():
    m = new_mesh()
    box(m, (-130, 0, 0), 340, 84, 350, mat=0)
    box(m, (-42, 128, 0), 84, 340, 350, mat=0)
    cut_keep_below(m, (40, -40, 360), (0, 0, 45))      # chamfered outer corner (vertical cut plane, yaw 45)
    bevel(m, 3.0)
    return finalize("SM_Erebus_BunkerCorner_A", m, [(M["MI_Erebus_Concrete_Dry"], "Concrete")])


def build_bunkerroof_a():
    m = new_mesh()
    box(m, (0, 0, 0), 640, 440, 46, mat=0, steps=2)
    box(m, (0, -210, 44), 640, 22, 30, mat=0)          # lips
    box(m, (0, 210, 44), 640, 22, 30, mat=0)
    box(m, (-308, 0, 44), 24, 440, 30, mat=0)
    noise(m, 2.0, 0.02, seed=17, above_z=30)
    return finalize("SM_Erebus_BunkerRoof_A", m, [(M["MI_Erebus_Concrete_Burned"], "Concrete")])


def build_industrialwall_a():
    m = new_mesh()
    box(m, (0, 0, 0), 800, 52, 520, mat=0)
    for px in (-330, 0, 330):                          # pilasters
        box(m, (px, 0, 0), 64, 76, 520, mat=1)
    for px in (-165, 165):                             # recessed bays
        subtract(m, tool_box((px, -28, 250), 220, 16, 380))
        subtract(m, tool_box((px, 28, 250), 220, 16, 380))
    box(m, (0, 0, 516), 820, 84, 36, mat=1)            # cornice
    bevel(m, 2.5)
    return finalize("SM_Erebus_IndustrialWall_A", m,
                    [(M["MI_Erebus_Concrete_Dry"], "Concrete"), (M["MI_Erebus_Steel_Painted"], "Steel")])


def build_industrialcolumn_a():
    m = new_mesh()
    box(m, (0, 0, 0), 150, 150, 30, mat=0)             # base plate
    box(m, (0, 0, 30), 96, 96, 560, mat=0)
    box(m, (0, 0, 588), 136, 136, 26, mat=0)           # cap plate
    bevel(m, 4.0)
    return finalize("SM_Erebus_IndustrialColumn_A", m, [(M["MI_Erebus_Steel_Painted"], "Steel")])


def build_industrialsupport_a():
    m = new_mesh()
    # I-beam along X, base pivot at bottom flange.
    box(m, (0, 0, 0), 500, 34, 12, mat=0)
    box(m, (0, 0, 12), 500, 12, 56, mat=0)
    box(m, (0, 0, 68), 500, 34, 12, mat=0)
    return finalize("SM_Erebus_IndustrialSupport_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def build_ruinedfacade(name, width, height, window_rows, seed, bite=None):
    m = new_mesh()
    box(m, (0, 0, 0), width, 60, height, mat=0, steps=3)
    for row_z in window_rows:
        for px in (-width * 0.3, 0, width * 0.3):
            subtract(m, tool_box((px, 0, row_z), 130, 90, 180))
    # Jagged collapsed top: three angled cuts.
    cut_keep_below(m, (-width * 0.25, 0, height * 0.94), (0, -14, 6))
    cut_keep_below(m, (width * 0.28, 0, height * 0.88), (8, 12, -4))
    cut_keep_below(m, (0, 0, height * 0.985), (0, -6, 40))
    if bite:
        subtract(m, tool_box(bite, 260, 160, 300, rot=(10, 6, 30)))
    noise(m, 3.0, 0.012, seed=seed)
    return finalize(name, m, [(M["MI_Erebus_Concrete_Burned"], "Concrete")])


def build_brokenfloor_a():
    m = new_mesh()
    box(m, (0, 0, 0), 400, 400, 34, mat=0, steps=3)
    subtract(m, tool_box((30, 0, 34), 10, 460, 14, rot=(0, 0, 25)))     # cracks
    subtract(m, tool_box((-60, 40, 34), 8, 380, 12, rot=(0, 0, -40)))
    noise(m, 2.0, 0.02, seed=23, above_z=20)
    bevel(m, 2.5)
    return finalize("SM_Erebus_BrokenFloor_A", m, [(M["MI_Erebus_Concrete_Dry"], "Concrete")])


def build_pipe_large_a():
    m = new_mesh()
    # Pipe lies along X, resting on ground: axis at z=46. Pitch-90 extrudes toward -X
    # from the base point (measured), so the base sits at +300 for a centered span.
    cyl(m, (300, 0, 46), 42, 600, rot=(0, 90, 0), mat=0, radial_steps=16)
    for fx in (-290, 0, 290):
        cyl(m, (fx + 8, 0, 46), 52, 16, rot=(0, 90, 0), mat=0, radial_steps=16)
    return finalize("SM_Erebus_Pipe_Large_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def build_pipe_elbow_a():
    m = new_mesh()
    a = unreal.DynamicMesh(); MATS.enable_material_i_ds(a)
    cyl(a, (0, 0, 0), 42, 240, mat=0, radial_steps=16)              # vertical leg
    cut_keep_below(a, (0, 0, 220), (0, 45, 0))                       # miter
    b = unreal.DynamicMesh(); MATS.enable_material_i_ds(b)
    cyl(b, (0, 0, 0), 42, 240, rot=(0, 90, 0), mat=0, radial_steps=16)  # horizontal leg along +X... rotated
    union_opts = unreal.GeometryScriptMeshBooleanOptions(); union_opts.fill_holes = True
    EDITS.append_mesh(m, a, xf())
    EDITS.append_mesh(m, b, xf((0, 0, 220)))
    cyl(m, (0, 0, 0), 52, 16, mat=0, radial_steps=16)               # flanges at open ends
    cyl(m, (-232, 0, 220), 52, 16, rot=(0, 90, 0), mat=0, radial_steps=16)
    return finalize("SM_Erebus_Pipe_Elbow_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def build_pipesupport_a():
    m = new_mesh()
    box(m, (0, 0, 0), 170, 130, 18, mat=0)
    box(m, (0, -52, 18), 150, 22, 96, mat=0)
    box(m, (0, 52, 18), 150, 22, 96, mat=0)
    saddle = new_mesh()
    box(saddle, (0, 0, 96), 150, 126, 40, mat=0)
    tool = unreal.DynamicMesh(); MATS.enable_material_i_ds(tool)
    PRIM.append_cylinder(tool, prim_opts(0), xf((90, 0, 136), (0, 90, 0)), 46, 180, 16)
    subtract(saddle, tool)
    EDITS.append_mesh(m, saddle, xf())
    return finalize("SM_Erebus_PipeSupport_A", m, [(M["MI_Erebus_Steel_Painted"], "Steel")])


def build_barricade_a():
    m = new_mesh()
    profile = [unreal.Vector2D(-58, 0), unreal.Vector2D(58, 0), unreal.Vector2D(44, 34),
               unreal.Vector2D(16, 62), unreal.Vector2D(12, 112), unreal.Vector2D(-12, 112),
               unreal.Vector2D(-16, 62), unreal.Vector2D(-44, 34)]
    PRIM.append_simple_extrude_polygon(m, prim_opts(0), xf(), profile, 320)
    # Extrusion runs along +Z; lay it down so length runs along Y, height up.
    XFORM.rotate_mesh(m, unreal.Rotator(roll=-90.0, pitch=0.0, yaw=0.0), unreal.Vector(0, 0, 0))
    bbox = QUERY.get_mesh_bounding_box(m)
    XFORM.translate_mesh(m, unreal.Vector(0, -(bbox.min.y + bbox.max.y) * 0.5, -bbox.min.z))
    subtract(m, tool_box((0, -110, 20), 140, 44, 40))   # forklift slots
    subtract(m, tool_box((0, 110, 20), 140, 44, 40))
    bevel(m, 2.0)
    return finalize("SM_Erebus_Barricade_A", m, [(M["MI_Erebus_Concrete_Dry"], "Concrete")])


def build_armorbarrier_a():
    m = new_mesh()
    box(m, (0, 0, 0), 170, 270, 14, mat=1)                       # base plate
    box(m, (18, 0, 8), 22, 250, 170, rot=(0, -24, 0), mat=0)     # sloped face
    box(m, (-52, -105, 8), 90, 18, 120, rot=(0, 30, 0), mat=1)   # gussets
    box(m, (-52, 105, 8), 90, 18, 120, rot=(0, 30, 0), mat=1)
    return finalize("SM_Erebus_ArmorBarrier_A", m,
                    [(M["MI_Erebus_Steel_Painted"], "Plate"), (M["MI_Erebus_Steel_Dark"], "Frame")])


def build_wreckage_a():
    m = new_mesh()
    box(m, (0, 0, 26), 390, 190, 110, mat=0, steps=2)            # hull
    box(m, (110, 0, 130), 150, 170, 84, mat=0, steps=1)          # cab
    cut_keep_below(m, (0, 0, 205), (7, -9, 0))                   # crushed roof
    cut_keep_below(m, (-160, 0, 170), (0, 25, 0))
    noise(m, 6.0, 0.014, seed=41)
    for wx, wy in ((120, 108), (120, -108), (-120, 108), (-120, -108)):
        cyl(m, (wx, wy - 13, 0), 42, 26, rot=(90, 0, 0), mat=1, radial_steps=12)
    return finalize("SM_Erebus_Wreckage_A", m,
                    [(M["MI_Erebus_WreckMetal"], "Hull"), (M["MI_Erebus_Rubber"], "Wheels")])


def build_wreckage_b():
    m = new_mesh()
    box(m, (0, 0, 0), 430, 180, 56, mat=0, steps=2)              # chassis
    box(m, (-40, 10, 56), 300, 150, 16, rot=(0, 4, 8), mat=0)    # spilled plates
    box(m, (30, -20, 84), 260, 130, 14, rot=(3, -6, -14), mat=0)
    box(m, (-10, 30, 110), 200, 110, 12, rot=(-4, 5, 22), mat=0)
    noise(m, 4.0, 0.016, seed=53)
    return finalize("SM_Erebus_Wreckage_B", m, [(M["MI_Erebus_WreckMetal"], "Hull")])


def build_rubble(name, radius, magnitude, seed):
    # Faceted broken-slab chunks: a smooth noisy sphere reads as a boulder egg in captures.
    m = new_mesh()
    box(m, (0, 0, 0), radius * 1.7, radius * 1.4, radius * 1.05, rot=(6, 4, 18), steps=2)
    box(m, (radius * 0.55, radius * 0.4, 0), radius * 1.1, radius * 0.9, radius * 0.7, rot=(8, -6, 40))
    box(m, (-radius * 0.55, -radius * 0.35, 0), radius * 0.95, radius * 1.15, radius * 0.55, rot=(-5, 9, -25))
    for index, (yaw, tilt) in enumerate([(25, 38), (110, 44), (205, 34), (290, 47), (65, 58), (245, 52)]):
        import math
        rad = math.radians(yaw)
        cx = math.cos(rad) * radius * 0.55
        cy = math.sin(rad) * radius * 0.55
        cut_keep_below(m, (cx, cy, radius * (0.75 + (index % 3) * 0.12)), (0, tilt, yaw))
    noise(m, magnitude * 0.4, 0.9 / radius, seed=seed)
    return finalize(name, m, [(M["MI_Erebus_Concrete_Burned"], "Rubble")])


def build_utilitypole_a():
    m = new_mesh()
    cyl(m, (0, 0, 0), 15, 700, mat=0, radial_steps=10)
    box(m, (0, 0, 640), 16, 230, 18, mat=0)
    cone(m, (0, -90, 658), 8, 3, 22, mat=0, radial_steps=8)
    cone(m, (0, 90, 658), 8, 3, 22, mat=0, radial_steps=8)
    return finalize("SM_Erebus_UtilityPole_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def build_cablesupport_a():
    m = new_mesh()
    box(m, (-70, 0, 0), 18, 18, 320, rot=(0, -12, 0), mat=0)
    box(m, (70, 0, 0), 18, 18, 320, rot=(0, 12, 0), mat=0)
    box(m, (0, 0, 300), 190, 16, 16, mat=0)
    box(m, (0, 0, 150), 150, 14, 14, mat=0)
    return finalize("SM_Erebus_CableSupport_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def build_trenchwall(name, length):
    m = new_mesh()
    box(m, (0, 0, 0), length, 64, 250, mat=0, steps=2)
    rib_count = max(2, int(length / 170))
    start = -length / 2 + 90
    for index in range(rib_count):
        box(m, (start + index * 170, 0, 0), 26, 86, 250, mat=1)
    box(m, (0, 0, 246), length + 24, 92, 26, mat=1)
    noise(m, 1.6, 0.02, seed=61)
    return finalize(name, m,
                    [(M["MI_Erebus_Concrete_Dry"], "Concrete"), (M["MI_Erebus_Steel_Dark"], "Ribs")])


def build_trenchcorner_a():
    m = new_mesh()
    box(m, (-90, 0, 0), 260, 64, 250, mat=0)
    box(m, (38, 128, 0), 64, 320, 250, mat=0)
    box(m, (-90, 0, 246), 280, 92, 26, mat=1)
    box(m, (38, 128, 246), 92, 330, 26, mat=1)
    return finalize("SM_Erebus_TrenchCorner_A", m,
                    [(M["MI_Erebus_Concrete_Dry"], "Concrete"), (M["MI_Erebus_Steel_Dark"], "Ribs")])


def build_sandbagrow_a():
    m = new_mesh()
    stream_positions = []
    for course, course_z, count in ((0, 0, 8), (1, 40, 7), (2, 80, 5)):
        offset = -160 + (course % 2) * 22
        for index in range(count):
            stream_positions.append((offset + index * 44, (course % 2) * 8 - 4, course_z, (index * 37 + course * 91) % 22 - 11))
    for px, py, pz, yaw in stream_positions:
        capsule(m, (px, py, pz + 21), 22, 36, rot=(0, 90, yaw), mat=0)
    noise(m, 2.2, 0.03, seed=71)
    return finalize("SM_Erebus_SandbagRow_A", m, [(M["MI_Erebus_Mud"], "Bags")])


def build_groundslab_a():
    m = new_mesh()
    box(m, (0, 0, 0), 800, 800, 28, mat=0, steps=6)
    subtract(m, tool_box((80, 0, 28), 12, 900, 10, rot=(0, 0, 20)))
    subtract(m, tool_box((-150, 100, 28), 10, 760, 9, rot=(0, 0, -35)))
    noise(m, 2.4, 0.012, seed=83, above_z=16)
    bevel(m, 3.0)
    return finalize("SM_Erebus_GroundSlab_A", m, [(M["MI_Erebus_Concrete_Dry"], "Concrete")])


def build_mudbase_a():
    m = new_mesh()
    box(m, (0, 0, 0), 2400, 2400, 22, mat=0, steps=24)
    noise(m, 8.0, 0.004, seed=97, above_z=12)
    noise(m, 2.6, 0.03, seed=101, above_z=12)
    return finalize("SM_Erebus_MudBase_A", m, [(M["MI_Erebus_Mud"], "Mud")])


def build_puddle_a():
    m = new_mesh()
    PRIM.append_disc(m, prim_opts(0), xf((0, 0, 0)), 130, 20)
    noise(m, 0.0, 0.01)  # keep flat; disc only
    return finalize("SM_Erebus_Puddle_A", m, [(M["MI_Erebus_Puddle"], "Water")])


def build_crate_a(open_lid):
    m = new_mesh()
    box(m, (0, 0, 0), 124, 84, 66, mat=0, steps=1)
    if open_lid:
        subtract(m, tool_box((0, 0, 56), 104, 64, 30))
        box(m, (0, -58, 60), 124, 84, 8, rot=(-64, 0, 0), mat=0)
    else:
        box(m, (0, 0, 64), 130, 90, 10, mat=0)
    box(m, (-36, 0, 0), 10, 92, 72, mat=1)
    box(m, (36, 0, 0), 10, 92, 72, mat=1)
    return finalize("SM_Erebus_CrateOpen_A" if open_lid else "SM_Erebus_Crate_A", m,
                    [(M["MI_Erebus_Steel_Painted"], "Body"), (M["MI_Erebus_Steel_Dark"], "Straps")])


def build_barrel_a():
    m = new_mesh()
    cyl(m, (0, 0, 0), 31, 92, mat=0, radial_steps=14)
    cyl(m, (0, 0, 26), 34, 6, mat=0, radial_steps=14)
    cyl(m, (0, 0, 62), 34, 6, mat=0, radial_steps=14)
    return finalize("SM_Erebus_Barrel_A", m, [(M["MI_Erebus_Steel_Scorched"], "Steel")])


def build_monolith_a():
    m = new_mesh()
    box(m, (0, 0, 0), 980, 780, 220, mat=0, steps=2)             # plinth
    box(m, (0, 0, 218), 900, 700, 1800, mat=0, steps=3)          # body
    for face_sign in (-1, 1):                                    # vertical recesses, front/back
        for px in (-260, 0, 260):
            subtract(m, tool_box((px, face_sign * 348, 1050), 150, 30, 1500))
    for face_sign in (-1, 1):                                    # side recesses
        for py in (-180, 180):
            subtract(m, tool_box((face_sign * 448, py, 1050), 30, 130, 1500))
    box(m, (0, 0, 2010), 950, 750, 170, mat=1)                   # crown band
    box(m, (0, 0, 2178), 700, 560, 420, mat=0, steps=1)          # upper block
    cut_keep_below(m, (300, 260, 2540), (14, -12, 30))           # weathered crown corner
    # Hanging banner and pale emblem plate on the route-facing side.
    box(m, (0, -368, 1080), 360, 10, 900, mat=2)
    box(m, (0, -378, 1330), 220, 6, 220, mat=3)
    noise(m, 3.0, 0.006, seed=113)
    return finalize("SM_Erebus_Monolith_A", m,
                    [(M["MI_Erebus_Concrete_Burned"], "Body"), (M["MI_Erebus_Steel_Dark"], "Crown"),
                     (M["MI_Erebus_BannerCloth"], "Banner"), (M["MI_Erebus_BannerEmblem"], "Emblem")])


def build_cathedral_spire(name, height, seed):
    m = new_mesh()
    cone(m, (0, 0, 0), 430, 150, height, mat=0, radial_steps=6)
    cone(m, (-320, -260, 0), 250, 70, height * 0.62, mat=0, radial_steps=6)
    cone(m, (300, 240, 0), 220, 60, height * 0.74, mat=0, radial_steps=6)
    cone(m, (-180, 330, 0), 180, 50, height * 0.5, mat=0, radial_steps=5)
    for yaw in (0, 90, 180, 270):                                 # vertical fins
        rad = 1.5707963 * yaw / 90.0
        import math
        fx, fy = math.cos(rad) * 330, math.sin(rad) * 330
        box(m, (fx, fy, 0), 54, 260, height * 0.82, rot=(0, 0, yaw), mat=0)
    noise(m, 14.0, 0.0022, seed=seed)
    return finalize(name, m, [(M["MI_Erebus_CathedralSilhouette"], "Matter")])


def build_ruinblock(name, footprint_x, footprint_y, height, seed, windows):
    m = new_mesh()
    box(m, (0, 0, 0), footprint_x, footprint_y, height * 0.55, mat=0, steps=2)
    box(m, (footprint_x * 0.06, -footprint_y * 0.05, height * 0.55),
        footprint_x * 0.82, footprint_y * 0.8, height * 0.45, mat=0, steps=2)
    cut_keep_below(m, (0, 0, height * 0.93), (6, -11, 15))
    cut_keep_below(m, (footprint_x * 0.2, 0, height * 0.85), (-9, 14, -30))
    if windows:
        cols = 4
        rows = 5
        for row in range(rows):
            wz = height * 0.12 + row * height * 0.14
            for col in range(cols):
                wx = -footprint_x * 0.3 + col * footprint_x * 0.2
                subtract(m, tool_box((wx, -footprint_y * 0.5, wz), 66, 30, 96))
    noise(m, 6.0, 0.004, seed=seed)
    return finalize(name, m, [(M["MI_Erebus_RuinDark"], "Ruin")])


def build_gantrytower_a():
    m = new_mesh()
    for lx, ly in ((-130, -130), (130, -130), (-130, 130), (130, 130)):
        box(m, (lx, ly, 0), 26, 26, 1100, mat=0)
    for bz in (260, 560, 860):
        box(m, (0, -130, bz), 280, 20, 20, mat=0)
        box(m, (0, 130, bz), 280, 20, 20, mat=0)
        box(m, (-130, 0, bz), 20, 280, 20, mat=0)
        box(m, (130, 0, bz), 20, 280, 20, mat=0)
        box(m, (0, 0, bz - 130), 24, 350, 20, rot=(38, 0, 0), mat=0)
    box(m, (0, 0, 1100), 320, 320, 30, mat=0)                    # platform
    box(m, (330, 0, 1116), 780, 40, 40, mat=0)                   # arm
    box(m, (700, 0, 1050), 40, 40, 80, mat=0)                    # arm tip block
    return finalize("SM_Erebus_GantryTower_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def run():
    ensure_folder(MESH_DIR)
    ensure_folder(INST_DIR)
    _calibrate_cut()

    for name, (parent, tint, scalars) in INSTANCES.items():
        make_instance(name, parent, tint, scalars)

    build_blastwall_a()
    build_blastwall_b()
    build_bunkerwall_a()
    build_bunkercorner_a()
    build_bunkerroof_a()
    build_industrialwall_a()
    build_industrialcolumn_a()
    build_industrialsupport_a()
    build_ruinedfacade("SM_Erebus_RuinedFacade_A", 900, 780, (240, 540), 131)
    build_ruinedfacade("SM_Erebus_RuinedFacade_B", 700, 950, (260, 600), 137, bite=(280, 0, 760))
    build_brokenfloor_a()
    build_pipe_large_a()
    build_pipe_elbow_a()
    build_pipesupport_a()
    build_barricade_a()
    build_armorbarrier_a()
    build_wreckage_a()
    build_wreckage_b()
    build_rubble("SM_Erebus_RubbleLarge_A", 120, 16.0, 149)
    build_rubble("SM_Erebus_RubbleMedium_A", 62, 9.0, 151)
    build_utilitypole_a()
    build_cablesupport_a()
    build_trenchwall("SM_Erebus_TrenchWall_A", 1150)
    build_trenchcorner_a()
    build_sandbagrow_a()
    build_groundslab_a()
    build_mudbase_a()
    build_puddle_a()
    build_crate_a(False)
    build_crate_a(True)
    build_barrel_a()
    build_monolith_a()
    build_cathedral_spire("SM_Erebus_CathedralSpire_A", 11000, 163)
    build_cathedral_spire("SM_Erebus_CathedralSpire_B", 8200, 167)
    build_ruinblock("SM_Erebus_RuinBlock_A", 820, 620, 1900, 173, windows=True)
    build_ruinblock("SM_Erebus_RuinBlock_B", 520, 520, 2700, 179, windows=False)
    build_gantrytower_a()

    # New Blueprint assembly wrappers (composition lives in AHPresentationPropActor styles).
    for prop in ["BP_Erebus_Bunker_A", "BP_Erebus_DefensivePosition_A", "BP_Erebus_WreckCluster_A",
                 "BP_Erebus_RuinedBlock_A", "BP_Erebus_PropCluster_A"]:
        if not unreal.load_asset("/Game/Ashes/Blueprints/Environment/" + prop):
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", unreal.load_class(None, "/Script/AshesOfHeaven.AHPresentationPropActor"))
            TOOLS.create_asset(prop, "/Game/Ashes/Blueprints/Environment", unreal.Blueprint, factory)

    unreal.EditorAssetLibrary.save_directory("/Game/Ashes/Environment", only_if_is_dirty=True, recursive=True)
    unreal.EditorAssetLibrary.save_directory("/Game/Ashes/Blueprints", only_if_is_dirty=True, recursive=True)

    report_path = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "Saved", "Logs", "ErebusKitReport.txt"))
    with open(report_path, "w") as handle:
        handle.write("\n".join(REPORT))
    unreal.log("[ErebusKit] authored %d kit entries; report at %s" % (len(REPORT), report_path))


run()
