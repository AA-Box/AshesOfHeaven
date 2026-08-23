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


TEX_DIR = "/Game/Ashes/Textures/Erebus"

# Baked procedural texture families for the M_ErebusSurface master.
FAMILY_TEXTURES = {
    "concrete": {"AlbedoTex": "T_Erebus_Concrete_D", "NormalTex": "T_Erebus_Concrete_N", "RoughTex": "T_Erebus_Concrete_R"},
    "metal":    {"AlbedoTex": "T_Erebus_Metal_D",    "NormalTex": "T_Erebus_Metal_N",    "RoughTex": "T_Erebus_Metal_R"},
    "mud":      {"AlbedoTex": "T_Erebus_Mud_D",      "NormalTex": "T_Erebus_Mud_N",      "RoughTex": "T_Erebus_Mud_R"},
    "asphalt":  {"AlbedoTex": "T_Erebus_Asphalt_D",  "NormalTex": "T_Erebus_Asphalt_N",  "RoughTex": "T_Erebus_Asphalt_R"},
}
FAMILY_UVTILE = {"concrete": 0.25, "metal": 0.5, "mud": 0.45, "asphalt": 0.32}


def make_instance(name, parent_name, tint, scalars, family=None):
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
    if family:
        for param, tex_name in FAMILY_TEXTURES[family].items():
            tex = unreal.load_asset(TEX_DIR + "/" + tex_name)
            if tex:
                MEL.set_material_instance_texture_parameter_value(inst, param, tex)
            else:
                unreal.log_error("[ErebusKit] missing texture %s for %s" % (tex_name, name))
        if "UVTile" not in scalars:
            MEL.set_material_instance_scalar_parameter_value(inst, "UVTile", FAMILY_UVTILE[family])
    MEL.update_material_instance(inst)
    unreal.EditorAssetLibrary.save_asset(full)
    return inst


INSTANCES = {
    # name: (parent master, BaseTint, scalar params, texture family or None)
    # Phase 4.7 PBR retune: surface materials ride M_ErebusSurface, which samples
    # the baked procedural texture sets (albedo detail x tint, normal, roughness).
    # Concrete albedos lift again: the gate feedback said the walls crush so dark
    # the secondary geometry is lost. Ground stays dark/wet like the reference.
    "MI_Erebus_Concrete_Dry":    ("M_ErebusSurface", (0.230, 0.222, 0.204), {"Roughness": 0.92, "GrimeAmount": 0.30, "WearAmount": 0.30, "Wetness": 0.0}, "concrete"),
    "MI_Erebus_Concrete_Light":  ("M_ErebusSurface", (0.420, 0.410, 0.385), {"Roughness": 0.90, "GrimeAmount": 0.22, "WearAmount": 0.26, "Wetness": 0.0}, "concrete"),
    "MI_Erebus_Concrete_Panels": ("M_ErebusSurface", (0.260, 0.252, 0.230), {"Roughness": 0.90, "GrimeAmount": 0.34, "WearAmount": 0.45, "Wetness": 0.0}, "concrete"),
    "MI_Erebus_Concrete_Wet":    ("M_ErebusSurface", (0.085, 0.088, 0.092), {"Roughness": 0.55, "GrimeAmount": 0.40, "Wetness": 0.55}, "concrete"),
    "MI_Erebus_Concrete_Burned": ("M_ErebusSurface", (0.060, 0.055, 0.050), {"Roughness": 0.96, "GrimeAmount": 0.65, "DamageMaskStrength": 0.75, "Wetness": 0.0}, "concrete"),
    "MI_Erebus_Steel_Dark":      ("M_ErebusSurface", (0.085, 0.088, 0.094), {"Roughness": 0.70, "Metallic": 0.80, "GrimeAmount": 0.40, "WearAmount": 0.38}, "metal"),
    "MI_Erebus_Steel_Painted":   ("M_ErebusSurface", (0.105, 0.112, 0.088), {"Roughness": 0.80, "Metallic": 0.30, "GrimeAmount": 0.38, "WearAmount": 0.50}, "metal"),
    "MI_Erebus_Steel_Olive":     ("M_ErebusSurface", (0.085, 0.095, 0.068), {"Roughness": 0.82, "Metallic": 0.30, "GrimeAmount": 0.45, "WearAmount": 0.55}, "metal"),
    "MI_Erebus_Metal_Bare":      ("M_ErebusSurface", (0.175, 0.175, 0.180), {"Roughness": 0.55, "Metallic": 0.90, "GrimeAmount": 0.32, "WearAmount": 0.55}, "metal"),
    "MI_Erebus_Steel_Scorched":  ("M_ErebusSurface", (0.034, 0.031, 0.028), {"Roughness": 0.96, "Metallic": 0.35, "GrimeAmount": 0.75, "DamageMaskStrength": 0.85}, "metal"),
    "MI_Erebus_Mud":             ("M_ErebusSurface", (0.011, 0.009, 0.007), {"Roughness": 0.95, "GrimeAmount": 0.50, "Wetness": 0.38}, "mud"),
    # Dark wet pavement for horizontal ground surfaces: the gate feedback said the
    # foreground ground reads bright/flat vs the reference's dark wet battlefield,
    # and bright ground also drags auto-exposure down, crushing the walls.
    "MI_Erebus_Concrete_Ground": ("M_ErebusSurface", (0.018, 0.019, 0.021), {"Roughness": 0.90, "GrimeAmount": 0.55, "WearAmount": 0.50, "Wetness": 0.30}, "concrete"),
    "MI_Erebus_RoadAsphalt":     ("M_ErebusSurface", (0.010, 0.010, 0.012), {"Roughness": 0.94, "GrimeAmount": 0.48, "WearAmount": 0.55, "Wetness": 0.28}, "asphalt"),
    "MI_Erebus_WreckMetal":      ("M_ErebusSurface", (0.110, 0.080, 0.058), {"Roughness": 0.92, "Metallic": 0.55, "GrimeAmount": 0.65, "DamageMaskStrength": 0.85}, "metal"),
    "MI_Erebus_Rubber":          ("M_HumanArmor",    (0.024, 0.024, 0.025), {"Roughness": 0.90, "Metallic": 0.0, "GrimeAmount": 0.60}, None),
    "MI_Erebus_Glass_Damaged":   ("M_Glass",         (0.030, 0.038, 0.042), {"Roughness": 0.35, "GrimeAmount": 0.50}, None),
    "MI_Erebus_Puddle":          ("M_Glass",         (0.010, 0.012, 0.016), {"Roughness": 0.05, "Wetness": 1.0}, None),
    "MI_Erebus_RuinDark":        ("M_ErebusSurface", (0.105, 0.108, 0.115), {"Roughness": 0.95, "GrimeAmount": 0.50, "WearAmount": 0.40}, "concrete"),
    "MI_Erebus_CathedralSilhouette": ("M_VeilObsidian", (0.016, 0.017, 0.020), {"Roughness": 0.85}, None),
    "MI_Erebus_BannerCloth":     ("M_HumanArmor",    (0.030, 0.030, 0.032), {"Roughness": 0.95, "Metallic": 0.0}, None),
    "MI_Erebus_BannerEmblem":    ("M_HumanMetal",    (0.300, 0.310, 0.285), {"Roughness": 0.85, "Metallic": 0.08, "GrimeAmount": 0.35}, None),
    "MI_Erebus_Decal_Scorch":    ("M_Scorch",        (0.030, 0.018, 0.010), {"DecalOpacity": 0.85}, None),
    "MI_Erebus_Decal_Grime":     ("M_Decal_Master",  (0.055, 0.050, 0.042), {"DecalOpacity": 0.65}, None),
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
    return finalize("SM_Erebus_BrokenFloor_A", m, [(M["MI_Erebus_Concrete_Ground"], "Concrete")])


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
    return finalize("SM_Erebus_GroundSlab_A", m, [(M["MI_Erebus_Concrete_Ground"], "Concrete")])


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
    # Asymmetric crane head: jib + short counter-jib + diagonal ties + hook block.
    # A single centered horizontal arm on a vertical tower silhouettes as a giant
    # cross against the sky (visual gate §18) — the diagonals and asymmetry kill it.
    box(m, (390, 0, 1116), 900, 40, 40, mat=0)                   # jib (offset, not centered)
    box(m, (-200, 0, 1116), 320, 48, 48, mat=0)                  # counter-jib
    box(m, (-230, 0, 1140), 90, 90, 70, mat=0)                   # counterweight
    box(m, (60, 0, 1156), 30, 30, 860, rot=(0, -62, 0), mat=0)   # tie: tower top -> jib
    box(m, (0, 0, 1156), 30, 30, 340, rot=(0, 58, 0), mat=0)     # tie: tower top -> counter-jib
    box(m, (0, 0, 1156), 60, 60, 120, mat=0)                     # kingpost
    box(m, (700, 0, 980), 26, 26, 140, mat=0)                    # hook cable
    box(m, (700, 0, 930), 70, 50, 60, mat=0)                     # hook block
    return finalize("SM_Erebus_GantryTower_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


# ---------------------------------------------------------------------------
# Generation 2 (Phase 4.6 visual gate): large architecture with three detail
# scales. Primary mass, secondary structure (pilasters, bands, bays, frames),
# tertiary reads (grilles, conduits, rebar, chips). Damage changes silhouette.
# All damage/chamfer cuts use subtract() with rotated CENTER-origin tools: the
# behaviour is calibrated and predictable, unlike exotic plane-cut frames.
# ---------------------------------------------------------------------------

def build_facade_heavy(name, bays, floors, seed, broken=False):
    import random
    rng = random.Random(seed)
    bay_w = 380.0
    floor_h = 440.0
    width = bays * bay_w
    height = 200.0 + floors * floor_h
    m = new_mesh()
    box(m, (0, 0, 0), width + 40, 110, 200, mat=0)                 # plinth
    box(m, (0, 10, 200), width, 62, height - 200, mat=1, steps=2)  # wall body
    for i in range(bays + 1):                                      # pilasters
        px = -width / 2 + i * bay_w
        box(m, (px, -20, 200), 84, 104, height - 200, mat=0)
    for f in range(1, floors + 1):                                 # spandrel bands
        bz = 200 + f * floor_h - 60
        box(m, (0, -12, bz), width, 88, 60, mat=2)
    for f in range(floors):                                        # window bays
        wz = 200 + f * floor_h + 130
        for b in range(bays):
            wx = -width / 2 + bay_w * (b + 0.5)
            subtract(m, tool_box((wx, -40, wz + 100), 210, 70, 220))
    box(m, (0, 36, 200), width - 40, 16, height - 220, mat=3)      # dark inset plane
    box(m, (0, -8, height), width + 30, 100, 46, mat=0)            # cornice
    box(m, (0, 6, height + 46), width, 60, 90, mat=1)              # parapet
    for i in range(2 + bays // 2):                                 # rooftop units
        ux = -width * 0.32 + i * width * 0.64 / max(1, 1 + bays // 2)
        box(m, (ux, rng.uniform(-30, 30), height + 40), rng.uniform(140, 300),
            rng.uniform(120, 240), rng.uniform(90, 220), rot=(0, 0, rng.uniform(0, 40)), mat=4)
    box(m, (width * 0.28, 0, height + 40), 26, 26, 480, mat=4)     # roof mast
    for b in range(0, bays, 2):                                    # ground-floor grilles
        wx = -width / 2 + bay_w * (b + 0.5)
        box(m, (wx, -52, 240), 200, 16, 140, mat=4)
        for s in range(4):
            box(m, (wx, -58, 246 + s * 30), 204, 20, 8, mat=4)
    cyl(m, (width * 0.5 - 30, -58, 200), 7, height - 400, mat=4, radial_steps=8)  # conduit
    subtract(m, tool_box((-width / 2 - 10, -50, 130), 120, 90, 90, rot=(0, 0, 22)))  # chipped plinth
    if broken:
        # Silhouette damage: raking collapse through the top-right mass.
        subtract(m, tool_box((width * 0.42, 0, height * 0.98), width * 0.55, 260, height * 0.55, rot=(8, 0, 24)))
        subtract(m, tool_box((width * 0.30, -40, height * 0.80), 260, 200, 300, rot=(0, 14, -18)))
        for f in range(2, floors):                                 # exposed floor slabs
            bz = 200 + f * floor_h
            if bz < height * 0.78:
                box(m, (width * 0.30, 0, bz - 26), 300, 150, 26, mat=2)
        for _ in range(7):                                         # bent rebar stubs
            rx = width * 0.18 + rng.uniform(0, width * 0.24)
            rz = height * 0.45 + rng.uniform(0, height * 0.28)
            box(m, (rx, rng.uniform(-30, 30), rz), 6, 6, rng.uniform(60, 160),
                rot=(rng.uniform(-40, 40), rng.uniform(-40, 40), 0), mat=4)
        noise(m, 3.5, 0.010, seed=seed)
    else:
        bevel(m, 2.5)
    return finalize(name, m, [
        (M["MI_Erebus_Concrete_Panels"], "Pilaster"),
        (M["MI_Erebus_Concrete_Dry"], "Wall"),
        (M["MI_Erebus_Concrete_Light"], "Band"),
        (M["MI_Erebus_Steel_Scorched"], "Inset"),
        (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_structure_frame(name, bays_x, levels, seed, collapsed=False):
    import random
    rng = random.Random(seed)
    m = new_mesh()
    sx, sz = 340.0, 330.0
    width = bays_x * sx
    depth = 300.0
    for i in range(bays_x + 1):                                    # columns
        px = -width / 2 + i * sx
        for py in (-depth / 2, depth / 2):
            if collapsed and i >= bays_x - 1 and py > 0:
                box(m, (px, py, 0), 34, 34, sz * 1.3, rot=(16, 7, 0), mat=0)
            else:
                box(m, (px, py, 0), 34, 34, levels * sz, mat=0)
    for lv in range(1, levels + 1):                                # ring beams
        bz = lv * sz - 30
        if collapsed and lv == levels:
            box(m, (-width * 0.25, -depth / 2, bz), width * 0.55, 30, 30, rot=(0, 0, 4), mat=0)
            box(m, (-width * 0.20, depth / 2, bz - 40), width * 0.5, 30, 30, rot=(6, 0, -3), mat=0)
        else:
            for py in (-depth / 2, depth / 2):
                box(m, (0, py, bz), width + 34, 30, 30, mat=0)
        for i in range(bays_x + 1):
            px = -width / 2 + i * sx
            box(m, (px, 0, bz), 30, depth + 34, 30, mat=0)
        for i in range(bays_x):                                    # floor slabs, some gone
            if rng.random() < (0.45 if collapsed else 0.78):
                px = -width / 2 + sx * (i + 0.5)
                box(m, (px, 0, bz + 2), sx - 24, depth - 24, 18, mat=1)
    for i in range(0, bays_x, 2):                                  # rear diagonal braces
        px = -width / 2 + sx * i + 40
        box(m, (px, depth / 2, 20), 26, 26, sz * 1.4, rot=(0, -40, 0), mat=0)
    if collapsed:
        noise(m, 2.5, 0.012, seed=seed)
    return finalize(name, m, [
        (M["MI_Erebus_Steel_Dark"], "Frame"),
        (M["MI_Erebus_Concrete_Burned"], "Slab")])


def build_servicebay(name, destroyed, seed):
    m = new_mesh()
    box(m, (0, 0, 0), 940, 90, 660, mat=0, steps=2)
    subtract(m, tool_box((0, -20, 330), 700, 80, 520))             # bay opening
    box(m, (0, 18, 640), 960, 70, 60, mat=1)                       # header beam
    for sx_ in (-500, 500):                                        # buttresses
        box(m, (sx_, -14, 0), 90, 120, 660, mat=0)
    if destroyed:
        for s in range(9):                                         # torn door slats
            if s in (3, 4):
                continue
            box(m, (14 if s % 2 else 0, 30, 90 + s * 58), 660, 14, 46,
                rot=(0, 0, 3 if s % 3 else -4), mat=2)
        subtract(m, tool_box((120, 20, 300), 320, 200, 420, rot=(10, 0, 14)))
        noise(m, 3.0, 0.012, seed=seed)
    else:
        for s in range(10):                                        # rolled door ribs
            box(m, (0, 30, 80 + s * 56), 680, 14, 48, mat=2)
        bevel(m, 2.5)
    return finalize(name, m, [
        (M["MI_Erebus_Concrete_Panels"], "Concrete"),
        (M["MI_Erebus_Steel_Dark"], "Header"),
        (M["MI_Erebus_Steel_Scorched" if destroyed else "MI_Erebus_Steel_Olive"], "Door")])


def build_overhang_a():
    m = new_mesh()
    box(m, (0, -170, 300), 800, 360, 42, mat=0)                    # canopy slab
    box(m, (0, -344, 296), 810, 26, 60, mat=1)                     # edge trim
    for px in (-330, 0, 330):                                      # knee braces
        box(m, (px, -60, 60), 40, 30, 340, rot=(38, 0, 0), mat=1)
    for px in (-300, -100, 100, 300):                              # soffit ribs
        box(m, (px, -170, 284), 26, 330, 18, mat=1)
    box(m, (0, 0, 0), 820, 60, 320, mat=0)                         # wall plate
    return finalize("SM_Erebus_Overhang_A", m, [
        (M["MI_Erebus_Concrete_Burned"], "Concrete"),
        (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_catwalk_a():
    m = new_mesh()
    box(m, (0, 0, 0), 900, 150, 14, mat=0)                         # deck
    for i in range(11):                                            # grating slats
        box(m, (-410 + i * 82, 0, 12), 20, 150, 6, mat=0)
    for py in (-70, 70):                                           # rails
        for px in (-420, -140, 140, 420):
            box(m, (px, py, 14), 12, 12, 110, mat=1)
        box(m, (0, py, 118), 900, 10, 10, mat=1)
        box(m, (0, py, 66), 900, 8, 8, mat=1)
    return finalize("SM_Erebus_Catwalk_A", m, [
        (M["MI_Erebus_Metal_Bare"], "Deck"),
        (M["MI_Erebus_Steel_Olive"], "Rail")])


def build_catwalksupport_a():
    m = new_mesh()
    box(m, (0, 0, 0), 30, 30, 330, mat=0)
    box(m, (0, -60, 90), 26, 26, 260, rot=(28, 0, 0), mat=0)
    box(m, (0, 60, 90), 26, 26, 260, rot=(-28, 0, 0), mat=0)
    box(m, (0, 0, 322), 46, 190, 18, mat=0)
    return finalize("SM_Erebus_CatwalkSupport_A", m, [(M["MI_Erebus_Steel_Olive"], "Steel")])


def build_industrialdoor_a():
    m = new_mesh()
    box(m, (0, 0, 0), 340, 46, 400, mat=0)                         # frame block
    subtract(m, tool_box((0, -12, 190), 250, 50, 340))
    box(m, (0, 8, 20), 250, 18, 350, mat=1)                        # door leaf
    for s in range(6):
        box(m, (0, -4, 40 + s * 54), 254, 8, 20, mat=1)            # ribs
    box(m, (0, -20, 0), 400, 90, 24, mat=0)                        # step
    return finalize("SM_Erebus_IndustrialDoor_A", m, [
        (M["MI_Erebus_Concrete_Panels"], "Frame"),
        (M["MI_Erebus_Steel_Olive"], "Door")])


def build_ventbank_a():
    m = new_mesh()
    box(m, (0, 0, 0), 470, 70, 280, mat=0)
    for i, vx in enumerate((-150, 0, 150)):
        subtract(m, tool_box((vx, -20, 160), 110, 60, 170))
        for s in range(4):
            box(m, (vx, -26, 82 + s * 40), 114, 24, 10, mat=1)     # louver fins
    return finalize("SM_Erebus_VentBank_A", m, [
        (M["MI_Erebus_Steel_Painted"], "Housing"),
        (M["MI_Erebus_Steel_Dark"], "Louver")])


def build_panelbank_a():
    m = new_mesh()
    box(m, (0, 0, 0), 560, 130, 18, mat=0)                         # base plate
    box(m, (-180, 0, 18), 170, 90, 260, mat=1)                     # cabinets
    box(m, (10, 0, 18), 150, 100, 300, mat=1)
    box(m, (170, 0, 18), 130, 80, 220, mat=2)
    cyl(m, (-180, 0, 278), 6, 140, mat=2, radial_steps=8)          # conduits up
    cyl(m, (10, 0, 318), 6, 100, mat=2, radial_steps=8)
    box(m, (10, -54, 250), 90, 8, 60, mat=2)                       # panel face detail
    box(m, (-180, -48, 160), 110, 8, 80, mat=2)
    return finalize("SM_Erebus_PanelBank_A", m, [
        (M["MI_Erebus_Concrete_Burned"], "Base"),
        (M["MI_Erebus_Steel_Olive"], "Cabinet"),
        (M["MI_Erebus_Steel_Dark"], "Detail")])


def build_columnheavy_a():
    m = new_mesh()
    box(m, (0, 0, 0), 230, 230, 40, mat=1)                         # base plate
    box(m, (0, 0, 40), 150, 150, 2520, mat=0, steps=2)             # shaft
    box(m, (0, 0, 2560), 210, 210, 60, mat=0)                      # capital
    for nx, ny in ((-95, 0), (95, 0), (0, -95), (0, 95)):          # bolt nubs
        box(m, (nx, ny, 46), 26, 26, 22, mat=1)
    subtract(m, tool_box((80, 80, 1300), 60, 60, 2500, rot=(0, 0, 45)))  # chamfered edge
    subtract(m, tool_box((-80, -80, 1300), 60, 60, 2500, rot=(0, 0, 45)))
    return finalize("SM_Erebus_ColumnHeavy_A", m, [
        (M["MI_Erebus_Concrete_Panels"], "Concrete"),
        (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_beamheavy_a():
    m = new_mesh()
    box(m, (0, 0, 0), 1600, 150, 28, mat=0)                        # bottom flange
    box(m, (0, 0, 28), 1600, 26, 180, mat=0)                       # web
    box(m, (0, 0, 208), 1600, 150, 28, mat=0)                      # top flange
    for px in (-700, -350, 0, 350, 700):                           # stiffeners
        box(m, (px, 0, 28), 20, 140, 180, mat=0)
    return finalize("SM_Erebus_BeamHeavy_A", m, [(M["MI_Erebus_Steel_Dark"], "Steel")])


def build_ruinedge(name, length, seed):
    import random
    rng = random.Random(seed)
    m = new_mesh()
    box(m, (0, 0, 0), length, 70, 180, mat=0, steps=3)
    x = -length / 2
    while x < length / 2 - 60:                                     # jagged top bites
        w = rng.uniform(60, 200)
        d = rng.uniform(40, 150)
        subtract(m, tool_box((x + w / 2, rng.uniform(-20, 20), 180), w, 110, d, rot=(rng.uniform(-14, 14), 0, rng.uniform(-10, 10))))
        x += w + rng.uniform(30, 120)
    noise(m, 2.2, 0.02, seed=seed)
    return finalize(name, m, [(M["MI_Erebus_RuinDark"], "Ruin")])


def build_roadslab_cracked_a():
    m = new_mesh()
    box(m, (-128, 0, 0), 250, 500, 30, rot=(0, 1.6, 0), mat=0, steps=2)
    box(m, (130, 6, 0), 246, 500, 30, rot=(0, -2.2, 1.5), mat=0, steps=2)
    subtract(m, tool_box((0, -90, 28), 60, 120, 34, rot=(0, 0, 18)))
    subtract(m, tool_box((10, 140, 28), 44, 90, 32, rot=(0, 0, -25)))
    noise(m, 1.8, 0.03, seed=211, above_z=8)
    return finalize("SM_Erebus_RoadSlab_Cracked_A", m, [(M["MI_Erebus_RoadAsphalt"], "Road")])


def build_curb_a():
    m = new_mesh()
    box(m, (0, 0, 0), 520, 46, 26, mat=0, steps=2)
    subtract(m, tool_box((-140, -10, 24), 70, 40, 24, rot=(0, 0, 12)))
    subtract(m, tool_box((180, 8, 24), 50, 36, 20, rot=(0, 0, -20)))
    noise(m, 1.2, 0.04, seed=223)
    return finalize("SM_Erebus_Curb_A", m, [(M["MI_Erebus_Concrete_Dry"], "Concrete")])


def build_rubbleberm_a():
    import random
    rng = random.Random(227)
    m = new_mesh()
    for i in range(20):
        px = -420 + i * 44 + rng.uniform(-24, 24)
        py = rng.uniform(-110, 110)
        s = rng.uniform(40, 130)
        box(m, (px, py, 0), s, s * rng.uniform(0.6, 1.2), s * rng.uniform(0.5, 0.9),
            rot=(rng.uniform(-30, 30), rng.uniform(-25, 25), rng.uniform(0, 90)),
            mat=0 if rng.random() < 0.7 else 1)
    noise(m, 5.0, 0.02, seed=227)
    cut_keep_below(m, (0, 0, 0), (180, 0, 0))  # flatten underside (plane flipped upside down)
    return finalize("SM_Erebus_RubbleBerm_A", m, [
        (M["MI_Erebus_RuinDark"], "Rubble"),
        (M["MI_Erebus_Concrete_Burned"], "Burned")])


def build_craterpatch_a():
    m = new_mesh()
    box(m, (0, 0, 0), 560, 560, 40, mat=0, steps=4)
    # Stepped depression carved with shrinking centered tools, smoothed by noise.
    subtract(m, tool_box((0, 0, 46), 330, 330, 26))
    subtract(m, tool_box((0, 0, 40), 210, 210, 34))
    subtract(m, tool_box((0, 0, 34), 110, 110, 40))
    for rx, ry in ((-250, 0), (250, 0), (0, -250), (0, 250), (-180, 180), (180, -180)):
        box(m, (rx, ry, 30), 90, 90, 34, rot=(0, 0, 30), mat=1)   # raised rim clods
    noise(m, 4.0, 0.03, seed=229)
    return finalize("SM_Erebus_CraterPatch_A", m, [
        (M["MI_Erebus_Mud"], "Mud"),
        (M["MI_Erebus_Concrete_Burned"], "Rim")])


def build_puddleset_a():
    m = new_mesh()
    cyl(m, (0, 0, 0), 150, 3, mat=0, radial_steps=18)
    cyl(m, (130, 90, 0), 95, 3, mat=0, radial_steps=14)
    cyl(m, (-120, 110, 0), 70, 3, mat=0, radial_steps=12)
    noise(m, 0.6, 0.02, seed=233)
    return finalize("SM_Erebus_PuddleSet_A", m, [(M["MI_Erebus_Puddle"], "Water")])


def build_drainchannel_a():
    m = new_mesh()
    box(m, (0, -70, 0), 620, 40, 26, mat=0)
    box(m, (0, 70, 0), 620, 40, 26, mat=0)
    for i in range(12):
        box(m, (-280 + i * 51, 0, 0), 18, 104, 14, mat=1)          # grate slats
    return finalize("SM_Erebus_DrainChannel_A", m, [
        (M["MI_Erebus_Concrete_Dry"], "Concrete"),
        (M["MI_Erebus_Steel_Dark"], "Grate")])


def build_cathedral_tower(name, height, seed):
    import math
    m = new_mesh()
    w0 = height * 0.16
    tier_z = (0.0, 0.30, 0.55, 0.76)
    tier_h = (0.32, 0.28, 0.24, 0.26)
    for t in range(4):                                             # tapering tiers
        tw = w0 * (1.0 - 0.18 * t)
        box(m, (0, 0, height * tier_z[t]), tw, tw, height * tier_h[t], mat=0, steps=2)
    for yaw in (45, 135, 225, 315):                                # octagonal chamfers
        r = w0 * 0.66
        cx, cy = math.cos(math.radians(yaw)) * r, math.sin(math.radians(yaw)) * r
        subtract(m, tool_box((cx, cy, height * 0.5), w0 * 0.5, w0 * 0.5, height * 1.3, rot=(0, 0, 45)))
    for sx_ in (-1, 1):                                            # deep vertical flutes
        for off in (-0.26, 0.0, 0.26):
            subtract(m, tool_box((sx_ * w0 * 0.5, w0 * off, height * 0.52), w0 * 0.24, w0 * 0.10, height * 1.02))
            subtract(m, tool_box((w0 * off, sx_ * w0 * 0.5, height * 0.52), w0 * 0.10, w0 * 0.24, height * 1.02))
    # asymmetric shoulder spires
    box(m, (w0 * 0.55, w0 * 0.30, 0), w0 * 0.30, w0 * 0.30, height * 0.62, mat=0, steps=1)
    box(m, (-w0 * 0.48, -w0 * 0.42, 0), w0 * 0.24, w0 * 0.24, height * 0.5, mat=0, steps=1)
    cone(m, (w0 * 0.55, w0 * 0.30, height * 0.62), w0 * 0.15, 10, height * 0.12, radial_steps=6)
    # crown needle
    cone(m, (0, 0, height * 0.99), w0 * 0.20, 6, height * 0.14, radial_steps=8)
    for yaw in (0, 90, 180, 270):                                  # base buttress fins
        rad = math.radians(yaw)
        fx, fy = math.cos(rad) * w0 * 0.62, math.sin(rad) * w0 * 0.62
        box(m, (fx, fy, 0), w0 * 0.06, w0 * 0.36, height * 0.42, rot=(0, 0, yaw), mat=0)
    noise(m, height * 0.0009, 0.0016, seed=seed)
    return finalize(name, m, [(M["MI_Erebus_CathedralSilhouette"], "Matter")])


# ---------------------------------------------------------------------------
# Generation 3 (Phase 4.8 visual gate iteration): the reference's dominant
# large-form vocabulary. Fortress slabs, an elevated fortress block on legs,
# a near-frame tower slab, hanging banners, and baked debris fields. All are
# metric-sane for the ~350m diorama the chapter actually is (1uu = 1cm).
# ---------------------------------------------------------------------------

def _seam_bands(m, width, depth, z_start, z_end, spacing, band_h=36, inset=18):
    """Horizontal recessed seam bands around the whole perimeter."""
    z = z_start
    while z < z_end:
        subtract(m, tool_box((0, -depth / 2, z), width + 40, inset * 2, band_h))
        subtract(m, tool_box((0, depth / 2, z), width + 40, inset * 2, band_h))
        subtract(m, tool_box((-width / 2, 0, z), inset * 2, depth + 40, band_h))
        subtract(m, tool_box((width / 2, 0, z), inset * 2, depth + 40, band_h))
        z += spacing


def _corner_chamfers(m, width, depth, height, size):
    for sx_ in (-1, 1):
        for sy_ in (-1, 1):
            subtract(m, tool_box((sx_ * width / 2, sy_ * depth / 2, height / 2),
                                 size, size, height * 1.3, rot=(0, 0, 45)))


def _roof_units(m, rng, width, depth, top_z, count, mat):
    for _ in range(count):
        ux = rng.uniform(-width * 0.32, width * 0.32)
        uy = rng.uniform(-depth * 0.30, depth * 0.30)
        w_ = rng.uniform(120, 320)
        box(m, (ux, uy, top_z), w_, w_ * rng.uniform(0.6, 1.1), rng.uniform(80, 260),
            rot=(0, 0, rng.uniform(0, 90)), mat=mat)


def build_fortress_a():
    """Big military fortress slab: the reference's near-left block building.
    Primary mass + heavy seam bands + recessed panel grid + battered plinth."""
    import random
    rng = random.Random(401)
    W, D, H = 3000.0, 1600.0, 3400.0
    m = new_mesh()
    box(m, (0, 0, 0), W + 220, D + 220, 260, mat=0, steps=2)          # battered plinth
    box(m, (0, 0, 240), W + 120, D + 120, 200, mat=2)                  # plinth cap band
    box(m, (0, 0, 420), W, D, H - 420, mat=1, steps=3)                 # primary mass
    _seam_bands(m, W, D, 900, H - 300, 620)                            # pour seams
    for px in (-1050, -350, 350, 1050):                                # vertical joints
        subtract(m, tool_box((px, -D / 2, (H + 420) / 2), 26, 30, H - 500))
        subtract(m, tool_box((px, D / 2, (H + 420) / 2), 26, 30, H - 500))
    for px in (-700, 0, 700):                                          # recessed slit rows
        for wz in (1500, 2400):
            subtract(m, tool_box((px, -D / 2, wz), 300, 60, 130))
            box(m, (px, -D / 2 + 40, wz - 80), 340, 26, 24, mat=2)     # slit sills
    box(m, (0, 0, H), W + 90, D + 90, 130, mat=2)                      # crown band
    box(m, (0, 0, H + 130), W - 300, D - 300, 240, mat=1, steps=1)     # upper block
    _corner_chamfers(m, W, D, H, 190)
    for bx in (-1200, -400, 400, 1200):                                # buttress fins
        box(m, (bx, -D / 2 - 60, 0), 200, 150, 1150, mat=0)
        box(m, (bx, D / 2 + 60, 0), 200, 150, 1150, mat=0)
    cyl(m, (W * 0.34, -D / 2 - 34, 480), 9, H - 900, mat=4, radial_steps=8)  # conduit pair
    cyl(m, (W * 0.34 + 40, -D / 2 - 34, 480), 9, H - 1300, mat=4, radial_steps=8)
    _roof_units(m, rng, W - 400, D - 400, H + 370, 4, 4)
    box(m, (-W * 0.30, 0, H + 370), 30, 30, 520, mat=4)                # antenna mast
    # Damage: raking bite through the crown's right shoulder + plinth chips.
    subtract(m, tool_box((W * 0.44, 0, H + 260), 900, D * 1.4, 700, rot=(6, 0, 18)))
    subtract(m, tool_box((-W / 2 - 40, -D / 2 - 40, 180), 300, 240, 260, rot=(0, 0, 30)))
    noise(m, 2.4, 0.008, seed=401)
    return finalize("SM_Erebus_Fortress_A", m, [
        (M["MI_Erebus_Concrete_Panels"], "Plinth"),
        (M["MI_Erebus_Concrete_Dry"], "Mass"),
        (M["MI_Erebus_Concrete_Light"], "Band"),
        (M["MI_Erebus_Steel_Scorched"], "Inset"),
        (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_fortress_b():
    """Elevated fortress block on heavy legs: the reference's center-left icon.
    A massive suspended mass with a dark underside reads instantly non-blockout."""
    import random
    rng = random.Random(409)
    W, D = 2600.0, 2000.0
    LEG_H = 1150.0
    BH = 2300.0
    m = new_mesh()
    box(m, (0, 0, 0), W + 500, D + 500, 240, mat=0, steps=2)           # podium
    for lx in (-W * 0.38, W * 0.38):                                   # legs
        for ly in (-D * 0.36, D * 0.36):
            box(m, (lx, ly, 200), 460, 460, LEG_H + 80, mat=0, steps=1)
            box(m, (lx, ly, 200), 560, 560, 180, mat=2)                # leg shoe
    box(m, (0, -D * 0.36, 200), 380, 380, LEG_H + 80, mat=0)           # mid legs
    box(m, (0, D * 0.36, 200), 380, 380, LEG_H + 80, mat=0)
    for ly in (-D * 0.36, D * 0.36):                                   # transfer beams
        box(m, (0, ly, LEG_H + 120), W * 0.86, 260, 300, mat=2)
    box(m, (0, 0, LEG_H + 280), W, D, 300, mat=2)                      # deck slab
    box(m, (0, 0, LEG_H + 560), W, D, BH, mat=1, steps=3)              # suspended mass
    _seam_bands(m, W, D, LEG_H + 1000, LEG_H + 560 + BH - 260, 560)
    for px in (-860, 0, 860):                                          # recessed bays
        subtract(m, tool_box((px, -D / 2, LEG_H + 1500), 560, 70, 900))
        for s in range(4):                                             # bay louvers
            box(m, (px, -D / 2 + 44, LEG_H + 1160 + s * 220), 600, 22, 40, mat=4)
    top = LEG_H + 560 + BH
    box(m, (0, 0, top), W + 80, D + 80, 120, mat=2)                    # crown band
    _corner_chamfers(m, W, D, top, 170)
    _roof_units(m, rng, W - 300, D - 300, top + 110, 5, 4)
    box(m, (W * 0.28, D * 0.2, top + 110), 34, 34, 640, mat=4)         # mast
    # Damage: one shoulder torn open, exposing floor slabs inside.
    subtract(m, tool_box((-W * 0.44, D * 0.30, top - 200), 820, 900, 900, rot=(10, 0, -24)))
    for fz in (LEG_H + 1400, LEG_H + 2000):
        box(m, (-W * 0.36, D * 0.30, fz), 500, 600, 24, mat=2)         # exposed slabs
    for _ in range(5):                                                 # bent rebar
        box(m, (-W * 0.40 + rng.uniform(0, 300), D * 0.28 + rng.uniform(-200, 200),
                top - 500 + rng.uniform(0, 350)), 7, 7, rng.uniform(80, 200),
            rot=(rng.uniform(-50, 50), rng.uniform(-50, 50), 0), mat=4)
    noise(m, 2.6, 0.008, seed=409)
    return finalize("SM_Erebus_Fortress_B", m, [
        (M["MI_Erebus_Concrete_Panels"], "Podium"),
        (M["MI_Erebus_Concrete_Dry"], "Mass"),
        (M["MI_Erebus_Concrete_Light"], "Frame"),
        (M["MI_Erebus_Steel_Scorched"], "Inset"),
        (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_towerslab_a():
    """Tall narrow tower slab for the near-right frame edge (reference right
    towers): vertical ribs, banded setbacks, roof mast; top bitten off."""
    W, D, H1, H2 = 1500.0, 1050.0, 3800.0, 1500.0
    m = new_mesh()
    box(m, (0, 0, 0), W + 160, D + 160, 300, mat=0, steps=1)           # base
    box(m, (0, 0, 280), W, D, H1, mat=1, steps=3)                      # shaft
    for px in (-560, -190, 190, 560):                                  # vertical ribs
        box(m, (px, 0, 280), 110, D + 90, H1 - 60, mat=0)
    for bz in (1300, 2300, 3300):                                      # band recesses
        subtract(m, tool_box((0, -D / 2, bz), W + 60, 26, 50))
        subtract(m, tool_box((0, D / 2, bz), W + 60, 26, 50))
    for px in (-375, 0, 375):                                          # window slots
        for wz in (900, 1800, 2700):
            subtract(m, tool_box((px, -D / 2, wz), 170, 56, 300))
            subtract(m, tool_box((px, D / 2, wz), 170, 56, 300))
    box(m, (0, 0, H1 + 260), W - 340, D - 260, H2, mat=1, steps=2)     # setback top
    box(m, (0, 0, H1 + 240), W - 180, D - 100, 110, mat=2)             # setback band
    subtract(m, tool_box((W * 0.30, 0, H1 + H2 + 260), 800, D * 1.4, 700, rot=(8, 0, 24)))  # top bite
    box(m, (-W * 0.18, 0, H1 + H2 + 90), 40, 40, 700, mat=4)           # mast
    cyl(m, (-W * 0.18, 0, H1 + H2 + 780), 60, 40, mat=4, radial_steps=8)
    box(m, (0, -D / 2 - 40, 460), 240, 90, 380, mat=4)                 # utility cabinet
    cyl(m, (W * 0.42, -D / 2 - 30, 380), 8, H1 - 800, mat=4, radial_steps=8)  # conduit
    noise(m, 2.2, 0.009, seed=419)
    return finalize("SM_Erebus_TowerSlab_A", m, [
        (M["MI_Erebus_Concrete_Panels"], "Rib"),
        (M["MI_Erebus_Concrete_Dry"], "Shaft"),
        (M["MI_Erebus_Concrete_Light"], "Band"),
        (M["MI_Erebus_Steel_Scorched"], "Inset"),
        (M["MI_Erebus_Steel_Dark"], "Steel")])


def build_bannerdrape(name, width, drop, seed):
    """Hanging military banner: rod + sagging cloth + pale emblem plate.
    The reference hangs these off every major facade."""
    import random
    rng = random.Random(seed)
    m = new_mesh()
    cyl(m, (-width / 2 - 30, 0, -40), 16, width + 60, rot=(0, 90, 0), mat=1)  # rod
    # cloth: three slightly kinked segments so the silhouette sags
    box(m, (0, 6, -drop * 0.34), width, 12, drop * 0.36, rot=(2.5, 0, 0), mat=0)
    box(m, (0, 14, -drop * 0.67), width * 0.97, 12, drop * 0.36, rot=(5.0, 0, 0), mat=0)
    box(m, (0, 26, -drop), width * 0.94, 12, drop * 0.36, rot=(8.0, 0, 0), mat=0)
    # torn lower edge
    x = -width / 2
    while x < width / 2 - 40:
        w_ = rng.uniform(50, 150)
        subtract(m, tool_box((x + w_ / 2, 30, -drop), w_, 80, rng.uniform(40, 170),
                             rot=(0, 0, rng.uniform(-12, 12))))
        x += w_ + rng.uniform(20, 90)
    box(m, (0, 0, -drop * 0.42), width * 0.55, 20, width * 0.55, mat=2)  # emblem plate
    box(m, (0, -6, -drop * 0.42), width * 0.10, 24, width * 0.44, mat=3)  # emblem: vertical bar
    box(m, (0, -6, -drop * 0.30), width * 0.30, 24, width * 0.08, mat=3)  # emblem: offset crossbar
    noise(m, 3.0, 0.02, seed=seed)
    return finalize(name, m, [
        (M["MI_Erebus_BannerCloth"], "Cloth"),
        (M["MI_Erebus_Steel_Dark"], "Rod"),
        (M["MI_Erebus_Concrete_Burned"], "Plate"),
        (M["MI_Erebus_BannerEmblem"], "Emblem")])


def build_debrisfield(name, radius, count, seed):
    """Baked scatter of broken chunks: one draw call of ground storytelling."""
    import random
    rng = random.Random(seed)
    m = new_mesh()
    for _ in range(count):
        import math
        ang = rng.uniform(0, 6.283)
        r = radius * math.sqrt(rng.uniform(0.05, 1.0))
        px, py = math.cos(ang) * r, math.sin(ang) * r
        s = rng.uniform(20, 95)
        box(m, (px, py, -s * 0.25), s, s * rng.uniform(0.5, 1.1), s * rng.uniform(0.4, 0.8),
            rot=(rng.uniform(-35, 35), rng.uniform(-30, 30), rng.uniform(0, 180)),
            mat=0 if rng.random() < 0.6 else 1)
    noise(m, 3.0, 0.03, seed=seed)
    cut_keep_below(m, (0, 0, -6), (180, 0, 0))
    return finalize(name, m, [
        (M["MI_Erebus_RuinDark"], "Rubble"),
        (M["MI_Erebus_WreckMetal"], "Metal")])


def build_checkpointgate_a():
    """Erebus corridor terminus gate: a heavy industrial security arch that
    reads as engineered architecture where the legacy transit blockout posts
    silhouetted as floating boxes from the comparison view."""
    m = new_mesh()
    for sy in (-1, 1):
        y = sy * 760.0
        box(m, (0, y, 0), 420, 420, 60, mat=1)                    # base plates
        box(m, (0, y, 60), 300, 300, 1250, mat=0, steps=2)        # columns
        subtract(m, tool_box((0, y + sy * 150, 700), 60, 60, 1400, rot=(0, 0, 45)))  # chamfer
        box(m, (0, y, 1290), 360, 360, 70, mat=1)                 # capitals
        box(m, (-170, y, 200), 60, 200, 900, rot=(0, -6, 0), mat=1)  # face ribs
    box(m, (0, 0, 1360), 340, 1900, 380, mat=0, steps=2)          # header box
    subtract(m, tool_box((0, 0, 1560), 380, 1500, 130))           # recessed band
    box(m, (-160, 0, 1500), 30, 1100, 200, mat=3)                 # sign plate (pale)
    for gy in (-620, -320, 320, 620):
        box(m, (-150, gy, 1420), 24, 130, 60, mat=2)              # vents
    box(m, (0, 0, 1740), 300, 2000, 80, mat=1)                    # cap beam
    for sy in (-1, 1):                                            # brace struts
        box(m, (0, sy * 620, 1140), 80, 80, 460, rot=(sy * 32, 0, 0), mat=2)
        box(m, (-140, sy * 500, 1330), 90, 70, 60, mat=2)         # floodlight boxes
    # battle damage: south column shoulder bitten, rebar stubs
    subtract(m, tool_box((60, -820, 1240), 260, 220, 260, rot=(12, 0, 30)))
    box(m, (30, -790, 1180), 8, 8, 160, rot=(20, 14, 0), mat=2)
    box(m, (-30, -830, 1200), 8, 8, 130, rot=(-16, 22, 0), mat=2)
    noise(m, 1.8, 0.012, seed=443)
    return finalize("SM_Erebus_CheckpointGate_A", m, [
        (M["MI_Erebus_Concrete_Panels"], "Concrete"),
        (M["MI_Erebus_Steel_Dark"], "Steel"),
        (M["MI_Erebus_Steel_Olive"], "Fittings"),
        (M["MI_Erebus_BannerEmblem"], "Sign")])


def build_worklight_a():
    m = new_mesh()
    box(m, (0, 0, 0), 110, 110, 16, mat=0)                        # base plate
    cyl(m, (0, 0, 16), 10, 260, mat=0, radial_steps=10)           # pole
    cone(m, (0, 0, 252), 36, 22, 18, mat=1, radial_steps=10)      # downward shade skirt
    box(m, (0, 0, 270), 46, 34, 26, mat=1)                        # housing
    box(m, (0, 20, 276), 30, 6, 14, mat=2)                        # lens plate
    box(m, (0, 0, 150), 26, 8, 8, mat=0)                          # cable clip
    return finalize("SM_Erebus_WorkLight_A", m, [
        (M["MI_Erebus_Steel_Olive"], "Pole"),
        (M["MI_Erebus_Steel_Dark"], "Housing"),
        (M["MI_Erebus_Metal_Bare"], "Lens")])


# ---------------------------------------------------------------------------
# Phase 4.7 hero pieces: single-purpose destroyed set pieces with enough
# sub-structure to read as specific objects (tank, gunship, gun, truck),
# not recombined generic modules. X+ is forward for all four.
# ---------------------------------------------------------------------------

def build_tankhulk_a():
    m = new_mesh()
    # track assemblies with sprocket/idler wheels poking past the skirt
    for side in (-1, 1):
        y = side * 165
        box(m, (0, y, 0), 600, 95, 100, mat=2)
        cyl(m, (300, y - 15 * side, 45), 46, 30, rot=(90 if side > 0 else -90, 0, 0), mat=3, radial_steps=10)
        cyl(m, (-300, y - 15 * side, 45), 46, 30, rot=(90 if side > 0 else -90, 0, 0), mat=3, radial_steps=10)
    box(m, (0, 0, 95), 640, 285, 125, mat=0)                       # hull
    subtract(m, tool_box((330, 0, 225), 280, 340, 150, rot=(0, 35, 0)))   # sloped glacis
    box(m, (-200, 0, 218), 220, 250, 26, mat=1)                    # engine deck
    for gx in (-260, -200, -140):
        box(m, (gx, 0, 244), 40, 230, 6, mat=3)                    # deck grilles
    cyl(m, (-30, 0, 220), 98, 88, mat=0, radial_steps=16)          # turret
    box(m, (70, 0, 250), 90, 95, 62, mat=0)                        # mantlet
    cyl(m, (110, 0, 272), 15, 430, rot=(0, 105, 0), mat=3)         # drooped barrel (knocked out)
    box(m, (-70, 35, 308), 64, 64, 10, rot=(24, 0, 40), mat=0)     # blown-open hatch
    cyl(m, (-90, -70, 300), 4, 160, rot=(30, 12, 0), mat=3)        # bent antenna
    box(m, (-170, -120, 90), 170, 110, 130, mat=1)                 # charred interior behind the breach
    subtract(m, tool_box((-180, -170, 160), 200, 120, 170, rot=(20, 0, 30)))  # hull breach
    subtract(m, tool_box((270, 175, 40), 170, 130, 150, rot=(0, 0, 25)))      # thrown track / broken skirt
    noise(m, 2.0, 0.02, 331)
    return finalize("SM_Erebus_TankHulk_A", m, [
        (M["MI_Erebus_Steel_Olive"], "Hull"), (M["MI_Erebus_Steel_Scorched"], "Burned"),
        (M["MI_Erebus_Rubber"], "Tracks"), (M["MI_Erebus_WreckMetal"], "Wreck")])


def build_gunshipwreck_a():
    m = new_mesh()
    capsule(m, (60, 0, 150), 92, 420, rot=(0, 82, 0), mat=0)       # fuselage, nose dug in
    cone(m, (430, 0, 130), 88, 30, 190, rot=(0, 98, 0), mat=1)     # crushed nose cone
    box(m, (330, 0, 190), 110, 90, 60, rot=(0, 12, 0), mat=3)      # canopy frame w/ glass slot
    box(m, (80, 0, 205), 60, 540, 26, rot=(8, 4, 0), mat=0)        # stub wings
    subtract(m, tool_box((70, -290, 210), 150, 160, 90, rot=(0, 0, 30)))  # sheared left wingtip
    cyl(m, (110, 245, 160), 46, 160, rot=(0, 90, 0), mat=2)        # right nacelle
    cyl(m, (40, -215, 170), 46, 140, rot=(0, 96, 0), mat=2)        # left nacelle, torn loose
    cyl(m, (-120, 0, 210), 33, 340, rot=(0, -108, 0), mat=0)       # tail boom kicked up
    box(m, (-430, 0, 300), 26, 90, 150, rot=(14, 0, 0), mat=1)     # tail fin
    cyl(m, (70, 0, 262), 26, 44, mat=3)                            # rotor hub
    for blade_yaw in (15, 135, 255):
        box(m, (70, 0, 292), 430, 36, 9, rot=(0, -16, blade_yaw), mat=1)  # drooped blades
    subtract(m, tool_box((380, 0, 60), 320, 280, 150, rot=(0, 22, 0)))    # belly crush at the nose
    subtract(m, tool_box((-40, 130, 140), 160, 100, 120, rot=(15, 0, -20)))  # flank rupture
    box(m, (-30, 100, 120), 130, 80, 90, mat=1)                    # charred interior in the rupture
    noise(m, 2.5, 0.015, 337)
    return finalize("SM_Erebus_GunshipWreck_A", m, [
        (M["MI_Erebus_Steel_Dark"], "Airframe"), (M["MI_Erebus_Steel_Scorched"], "Burned"),
        (M["MI_Erebus_WreckMetal"], "Wreck"), (M["MI_Erebus_Glass_Damaged"], "Canopy")])


def build_artillerygun_a():
    m = new_mesh()
    for side in (-1, 1):                                            # wheels
        cyl(m, (0, -155 if side < 0 else 121, 75), 75, 34, rot=(90, 0, 0), mat=2, radial_steps=14)
    cyl(m, (0, -155, 75), 18, 310, rot=(90, 0, 0), mat=1)           # axle
    box(m, (0, 0, 80), 160, 120, 70, mat=0)                         # carriage
    box(m, (-250, 60, 55), 430, 36, 42, rot=(0, 0, 14), mat=0)      # split trail L
    box(m, (-250, -60, 55), 430, 36, 42, rot=(0, 0, -14), mat=0)    # split trail R
    box(m, (30, 0, 132), 140, 72, 52, mat=0)                        # cradle
    cyl(m, (60, 0, 152), 13, 520, rot=(0, 72, 0), mat=1)            # barrel, elevated 18deg
    box(m, (554, 0, 300), 58, 38, 38, rot=(0, 72, 0), mat=1)        # muzzle brake
    box(m, (18, 0, 142), 72, 48, 48, mat=1)                         # breech
    box(m, (95, 0, 62), 14, 265, 145, rot=(0, -10, 0), mat=0)       # gun shield
    subtract(m, tool_box((95, -95, 175), 40, 80, 60))               # shield sight notch
    for i, (sx, sy) in enumerate([(-170, 95), (-200, 65), (-150, 130)]):
        cyl(m, (sx, sy, 12), 8, 42, rot=(90, 15 * i, 0), mat=1)     # spent casings
    noise(m, 1.2, 0.03, 341)
    return finalize("SM_Erebus_ArtilleryGun_A", m, [
        (M["MI_Erebus_Steel_Olive"], "Carriage"), (M["MI_Erebus_Metal_Bare"], "Gun"),
        (M["MI_Erebus_Rubber"], "Wheels")])


def build_truckwreck_a():
    m = new_mesh()
    for side in (-1, 1):
        box(m, (0, side * 70, 55), 600, 24, 20, mat=1)              # chassis rails
    cyl(m, (200, 92, 55), 55, 38, rot=(90, 0, 0), mat=2, radial_steps=12)     # front right wheel
    cyl(m, (200, -125, 55), 22, 30, rot=(90, 0, 0), mat=3, radial_steps=10)   # front left: bare drum
    for rx in (-120, -230):
        cyl(m, (rx, 92, 55), 55, 38, rot=(90, 0, 0), mat=2, radial_steps=12)
        cyl(m, (rx, -130, 55), 55, 38, rot=(90, 0, 0), mat=2, radial_steps=12)
    box(m, (215, 0, 90), 190, 220, 165, mat=0)                      # cab
    subtract(m, tool_box((215, 45, 265), 250, 270, 130, rot=(12, 0, 8)))     # crushed roof
    subtract(m, tool_box((315, 0, 190), 40, 150, 70))               # windshield hole
    box(m, (335, 0, 90), 130, 185, 75, mat=0)                       # hood
    subtract(m, tool_box((345, 30, 175), 90, 90, 50, rot=(0, 8, 15)))        # blown hood opening
    box(m, (340, 15, 95), 70, 60, 55, mat=3)                        # exposed engine block
    box(m, (-115, 0, 100), 380, 220, 24, mat=1)                     # flatbed
    for px in (-280, -160, -40, 60):                                # stake posts, one row torn
        box(m, (px, 105, 124), 16, 12, 120, mat=1)
        if px != -160:
            box(m, (px, -105, 124), 16, 12, 120, mat=1)
    for rib_x in (-240, -120, 0):                                   # burned canopy ribs
        box(m, (rib_x, 95, 244), 12, 10, 90, rot=(18, 0, 0), mat=1)
        box(m, (rib_x, -95, 244), 12, 10, 90, rot=(-18, 0, 0), mat=1)
        box(m, (rib_x, 0, 320), 12, 200, 10, mat=1)
    cyl(m, (-140, 30, 124), 32, 85, rot=(0, 78, 30), mat=3)         # tipped fuel drum
    noise(m, 1.8, 0.03, 347)
    return finalize("SM_Erebus_TruckWreck_A", m, [
        (M["MI_Erebus_Steel_Scorched"], "Burned"), (M["MI_Erebus_WreckMetal"], "Wreck"),
        (M["MI_Erebus_Rubber"], "Wheels"), (M["MI_Erebus_Metal_Bare"], "Bare")])


def run():
    ensure_folder(MESH_DIR)
    ensure_folder(INST_DIR)
    _calibrate_cut()

    for name, (parent, tint, scalars, family) in INSTANCES.items():
        make_instance(name, parent, tint, scalars, family)

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

    # Generation 2: the large-architecture language for the visual gate.
    build_facade_heavy("SM_Erebus_Facade_Heavy_A", 4, 5, 241)
    build_facade_heavy("SM_Erebus_Facade_Heavy_B", 3, 4, 251)
    build_facade_heavy("SM_Erebus_Facade_Broken_A", 4, 5, 257, broken=True)
    build_structure_frame("SM_Erebus_StructureFrame_A", 4, 3, 263)
    build_structure_frame("SM_Erebus_StructureFrame_B", 3, 2, 269, collapsed=True)
    build_servicebay("SM_Erebus_ServiceBay_A", False, 271)
    build_servicebay("SM_Erebus_ServiceBay_Destroyed_A", True, 277)
    build_overhang_a()
    build_catwalk_a()
    build_catwalksupport_a()
    build_industrialdoor_a()
    build_ventbank_a()
    build_panelbank_a()
    build_columnheavy_a()
    build_beamheavy_a()
    build_ruinedge("SM_Erebus_RuinEdge_A", 900, 281)
    build_ruinedge("SM_Erebus_RuinEdge_B", 1400, 283)
    build_roadslab_cracked_a()
    build_curb_a()
    build_rubbleberm_a()
    build_craterpatch_a()
    build_puddleset_a()
    build_drainchannel_a()
    build_cathedral_tower("SM_Erebus_CathedralTower_A", 15000, 293)
    build_cathedral_tower("SM_Erebus_CathedralTower_B", 11500, 307)
    build_cathedral_tower("SM_Erebus_CathedralTower_C", 8800, 311)
    build_worklight_a()

    # Generation 3: reference-vocabulary large forms (Phase 4.8 gate iteration).
    build_fortress_a()
    build_fortress_b()
    build_towerslab_a()
    build_bannerdrape("SM_Erebus_BannerDrape_A", 360, 1300, 421)
    build_bannerdrape("SM_Erebus_BannerDrape_B", 520, 1900, 431)
    build_debrisfield("SM_Erebus_DebrisField_A", 420, 34, 433)
    build_debrisfield("SM_Erebus_DebrisField_B", 700, 52, 439)
    build_checkpointgate_a()

    # Phase 4.7 hero pieces.
    build_tankhulk_a()
    build_gunshipwreck_a()
    build_artillerygun_a()
    build_truckwreck_a()

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
