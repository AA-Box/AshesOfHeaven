"""Import the external enemy creature models and build their game-side presentation.

The shipped enemies were two tints of the same UE mannequin. This brings in four authored
creature meshes (a humanoid alien, a quadruped biter, a bio-mech spider, and a heavy armoured
figure), each with its own skeleton, and gives each one a lit PBR material set instead of the
flat tint masters the mannequin used.

Run with UnrealEditor-Cmd; idempotent, so re-running rebuilds in place. Source files are read
from AH_ENEMY_SOURCE (default ~/Downloads/enemy) and are never modified.

Three facts drive most of the code here:
  * a material must have used_with_skeletal_mesh before it can paint a character body, or the
    body silently falls back to the engine default grey;
  * these meshes carry their own skeletons, so nothing retargets from the mannequin - the
    animation that ships inside each FBX is all the motion an archetype gets;
  * none of the four FBX files carry usable embedded textures. Two reference maps by a Windows
    path that does not exist here and two ship no image references at all, so every texture is
    imported explicitly from a file listed below rather than left to the FBX importer.
"""

import json
import os

import unreal


SOURCE_ROOT = os.environ.get("AH_ENEMY_SOURCE", os.path.expanduser("~/Downloads/enemy"))
# Maps baked by Scripts/BakeCreatureTextures.py. A texture path prefixed "baked:" resolves here
# instead of against the external model drop.
BAKED_ROOT = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "CreatureTextureSource"))
# Files written by Scripts/PrepareCreatureSources.py under Blender: the armoured figure's
# skinned re-export and its takes, and the quadruped's 4K maps unpacked from its .blend. A
# path prefixed "prepared:" resolves here.
PREPARED_ROOT = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "CreatureSource"))
ENEMY_ROOT = "/Game/Ashes/Enemies"
MATERIAL_DIR = "/Game/Ashes/Materials"
MASTER_PATH = MATERIAL_DIR + "/M_EnemyCreature"
MANIFEST_PATH = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "EnemyModelManifest.json"))

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary


# --- model table -----------------------------------------------------------------------
# target_height_cm is what the mesh is scaled to at spawn. The source files are authored at
# wildly different scales (the alien is roughly life-size, the hound is twenty metres long), so
# a fixed mesh scale would be a guess; the author script divides this by the imported bounds.
#
# texture_sets maps a set name to the source images that dress it. slot_sets then matches the
# FBX's own material names against those sets, which is the only way to tell the armoured
# figure's rock plates from its leather straps - the model ships five materials in one body.
MODELS = {
    # The old Pilgrim body. Humanoid alien on a 3ds Max Biped skeleton, and the only archetype
    # that still carries a rifle. Untextured at source (FBX 6000, no image references), so it
    # shades from baked procedural maps.
    "Stalker": {
        "fbx": "1/x-com+alien+180601@idel+1-221.FBX",
        "import_animations": True,
        "target_height_cm": 190.0,
        # Tint is a multiplier over a real albedo, so it sits near white; the darkness lives in
        # the baked map. Metallic 0: this is a shell, and a metallic organic body is exactly what
        # made it read as painted plastic.
        "tint": (0.20, 0.11, 0.055),
        "roughness": 0.82,
        "metallic": 0.0,
        "specular": 0.18,
        "normal_strength": 1.15,
        "detail_normal_strength": 0.90,
        "emissive": (0.95, 0.28, 0.10),
        "emissive_strength": 2.0,
        "texture_sets": {
            "body": {
                "color": "baked:T_Creature_Chitin_D.png",
                "normal": "baked:T_Creature_Chitin_N.png",
                "roughness": "baked:T_Creature_Chitin_R.png",
                "ao": "baked:T_Creature_Chitin_AO.png",
            },
        },
        "slot_sets": [],
        "default_set": "body",
    },
    # The biter. Quadruped, six authored takes, no weapon - it closes and bites.
    # Its maps come from the .blend rather than the glTF folder beside it. The glTF export packs
    # metallic and roughness into one image in the glTF convention (occlusion in red, roughness
    # in green, metal in blue); the roughness sampler reads red, which is 1.0 nearly everywhere,
    # so the map did nothing and the body had one flat roughness. The .blend still holds the
    # separate 4096 originals.
    "Hound": {
        "fbx": "2/Alien-Animal_1_5_Baked.fbx",
        "import_animations": True,
        "target_height_cm": 115.0,
        # This model's albedo means 0.316 where the baked creature maps mean 0.10, so it needs
        # roughly a third of their tint to land on the same surface value.
        "tint": (0.14, 0.095, 0.075),
        "roughness": 0.88,
        "metallic": 0.75,
        "specular": 0.20,
        "normal_strength": 1.10,
        "detail_normal_strength": 0.85,
        "emissive": (1.0, 0.12, 0.035),
        "emissive_strength": 1.8,
        "slot_tuning": {
            1: {"tint": (0.10, 0.055, 0.035), "roughness": 0.24,
                "metallic": 0.0, "specular": 0.18, "detail_normal_strength": 0.0},
        },
        "texture_sets": {
            "body": {
                "color": "prepared:Hound_Color.png",
                "normal": "prepared:Hound_Normal.png",
                "roughness": "prepared:Hound_Roughness.png",
                "metallic": "prepared:Hound_Metallic.png",
            },
            "eye": {"emissive": "prepared:Hound_Eye.png"},
        },
        "slot_sets": [("eye", "eye"), ("saliva", "body")],
        "default_set": "body",
    },
    # Bio-mech crawler. Skinned, ships no takes and no textures; both are authored - the takes by
    # Scripts/AuthorCreatureAnimations.py, the maps by folding occlusion and cavity baked from
    # this mesh into the procedural carapace so the detail lands in the model's own creases.
    "Spider": {
        # The bio-mech crawler this archetype started from is gone. It was a 6,955-vert body with
        # no textures of its own, and every map it wore was numpy noise; next to the quadruped's
        # authored 4K set it read as a grey blob no tint could rescue. This is the crawler out of
        # the alien-eggs diorama instead, rigged by Scripts/RigFacehugger.py - the drop ships it
        # as static scene dressing with no armature at all, so the skeleton is built from its own
        # geometry before it ever reaches Unreal.
        "fbx": "prepared:Facehugger_Mesh.fbx",
        "import_animations": False,
        # Flat and long rather than tall: at 95 the body is about 130cm from tail tip to front
        # legs, which is a crab the size of a large dog. The old 140 was a height for a body that
        # stood up; this one lies along the ground and 140 would have made it a car.
        "target_height_cm": 95.0,
        # A multiplier, not an albedo. The map is a real painted trim sheet averaging 0.26, so
        # the darkness is already in it - the mistake the old spider spent months carrying was
        # multiplying a dark map by a dark tint and bottoming the body out near black. 0.72
        # rather than 0.90: every combatant carries a 15cd warm fill light, and on the lineup
        # bench the brighter value read as a pale cut-out next to the hound.
        "tint": (0.72, 0.70, 0.68),
        # The packed roughness channel is film-authored and averages 0.44 - wet-skin gloss.
        # Under the per-combatant fill light that specular wash is what erased the painted
        # detail, so the scalar doubles the map into a matte range (the graph multiplies
        # tex.R by this and the renderer clamps at 1).
        "roughness": 2.0,
        "specular": 0.12,
        "normal_strength": 1.0,
        "detail_normal_strength": 0.55,
        # Chitin, not machinery. The source packs metal as a flat 1.0 because glTF defaults it
        # that way, which would make a fleshy body a mirror - see prepare_facehugger().
        "metallic": 0.0,
        # No emissive mask ships with this body, and EmissiveMask defaults to white, so any
        # strength above zero lights the entire creature rather than an eye.
        "emissive": (0.55, 0.85, 0.60),
        "emissive_strength": 0.0,
        "texture_sets": {
            "body": {
                "color": "baked:T_Facehugger_D.png",
                "normal": "baked:T_Facehugger_N.png",
                "roughness": "baked:T_Facehugger_R.png",
            },
        },
        "slot_sets": [],
        "default_set": "body",
    },
}

# Source material names that mark the part of a body meant to glow. Everything else stays dark;
# painting every slot emissive turns a creature into a lamp.
GLOW_TOKENS = ("eye", "glow", "emiss", "light", "lamp")


def _log(message):
    unreal.log_warning("[EnemyModels] " + message)


def _fail(message):
    unreal.log_error("[EnemyModels] " + message)


def _load(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def _resolve_source(relative):
    """Resolve a model-table path against the right root for its prefix."""
    if relative.startswith("baked:"):
        return os.path.join(BAKED_ROOT, relative[len("baked:"):])
    if relative.startswith("prepared:"):
        return os.path.join(PREPARED_ROOT, relative[len("prepared:"):])
    return os.path.join(SOURCE_ROOT, relative)


def _assets_under(folder):
    return list(unreal.EditorAssetLibrary.list_assets(folder, recursive=True, include_folder=False))


SHARED_DIR = ENEMY_ROOT + "/Shared"


def import_shared_textures():
    """The one map every creature samples, imported before the master that defaults to it."""
    unreal.EditorAssetLibrary.make_directory(SHARED_DIR)
    name = "T_Creature_DetailN"
    source = os.path.join(BAKED_ROOT, name + ".png")
    if not os.path.isfile(source):
        raise RuntimeError("run Scripts/BakeCreatureTextures.py first: " + source)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", SHARED_DIR)
    task.set_editor_property("destination_name", name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    TOOLS.import_asset_tasks([task])
    full = "%s/%s" % (SHARED_DIR, name)
    texture = _load(full)
    if not texture:
        raise RuntimeError("shared detail normal import failed: " + full)
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
    texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
    unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
    _log("imported shared " + full)
    return full


# --- master material -------------------------------------------------------------------
def author_creature_master():
    """One lit PBR master for every creature body.

    The existing character masters (M_HumanMetal, M_VeilObsidian) are flat tints with no texture
    inputs, which is exactly why the mannequin enemies read as untextured. This one takes the
    maps a model shipped with and falls back to the tint where a map is missing, so the two
    models that arrived with no textures at all still shade correctly instead of going white.
    """
    material = _load(MASTER_PATH)
    if not material:
        material = TOOLS.create_asset("M_EnemyCreature", MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    if not material:
        raise RuntimeError("failed to create " + MASTER_PATH)

    # Re-running has to start from an empty graph or every node is duplicated.
    MEL.delete_all_material_expressions(material)

    def expr(class_name, x, y):
        return MEL.create_material_expression(material, getattr(unreal, class_name), x, y)

    def connect(src, out, dst, inp):
        # Raise, do not log. A dropped connection leaves the graph silently half-wired - the
        # material still compiles and still ships, just with the wrong thing plugged into it.
        if not MEL.connect_material_expressions(src, out, dst, inp):
            raise RuntimeError("connect failed %s -> %s.%s" % (src.get_name(), dst.get_name(), inp))

    def scalar(name, default, x, y):
        node = expr("MaterialExpressionScalarParameter", x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", default)
        return node

    def vector(name, value, x, y):
        node = expr("MaterialExpressionVectorParameter", x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("default_value", unreal.LinearColor(*value))
        return node

    # A sampler node whose default texture has the wrong compression does not warn - the whole
    # material fails to compile, the cook ships no shader map, and every body that uses it renders
    # with the engine default material. That is a grey enemy in the packaged game and a single
    # warning buried in the cook log, so the pairing is asserted here instead.
    REQUIRED_COMPRESSION = {
        unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL: unreal.TextureCompressionSettings.TC_NORMALMAP,
        unreal.MaterialSamplerType.SAMPLERTYPE_MASKS: unreal.TextureCompressionSettings.TC_MASKS,
    }

    def tex_param(name, sampler, x, y, default_path):
        node = expr("MaterialExpressionTextureSampleParameter2D", x, y)
        node.set_editor_property("parameter_name", name)
        node.set_editor_property("sampler_type", sampler)
        default = _load(default_path)
        if not default:
            raise RuntimeError("%s default texture does not resolve: %s" % (name, default_path))
        required = REQUIRED_COMPRESSION.get(sampler)
        actual = default.get_editor_property("compression_settings")
        if required is not None and actual != required:
            raise RuntimeError("%s default %s is %s, sampler needs %s" % (name, default_path, actual, required))
        node.set_editor_property("texture", default)
        return node

    # Project textures, not engine ones: these are the only images guaranteed to carry the
    # compression each sampler type demands. /Engine/EngineMaterials/DefaultNormal does not
    # resolve in 5.8, and the node then silently falls back to the sRGB DefaultTexture.
    white = "/Engine/EngineResources/WhiteSquareTexture.WhiteSquareTexture"
    flat_normal = "/Game/Ashes/Textures/Erebus/T_Erebus_Concrete_N.T_Erebus_Concrete_N"
    flat_mask = "/Game/Ashes/Textures/Erebus/T_Erebus_Concrete_R.T_Erebus_Concrete_R"
    detail_normal = "/Game/Ashes/Enemies/Shared/T_Creature_DetailN.T_Creature_DetailN"

    base_tint = vector("BaseTint", (0.5, 0.5, 0.5, 1.0), -1400, -560)
    albedo = tex_param("BaseColorTex", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1120, -420, white)
    # UseBaseColorTex is 0 for a model that shipped no albedo: the lerp then leaves the tint
    # alone rather than multiplying it by whatever the fallback white square happens to be.
    use_albedo = scalar("UseBaseColorTex", 1.0, -1400, -300)
    tinted = expr("MaterialExpressionMultiply", -820, -460)
    connect(albedo, "RGB", tinted, "A")
    connect(base_tint, "", tinted, "B")
    base_lerp = expr("MaterialExpressionLinearInterpolate", -600, -500)
    connect(base_tint, "", base_lerp, "A")
    connect(tinted, "", base_lerp, "B")
    connect(use_albedo, "", base_lerp, "Alpha")

    normal = tex_param("NormalTex", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1120, 320, flat_normal)
    normal_strength = scalar("NormalStrength", 1.0, -1400, 470)
    flat = expr("MaterialExpressionConstant3Vector", -1120, 560)
    flat.set_editor_property("constant", unreal.LinearColor(0.0, 0.0, 1.0, 1.0))
    normal_lerp = expr("MaterialExpressionLinearInterpolate", -700, 380)
    connect(flat, "", normal_lerp, "A")
    connect(normal, "RGB", normal_lerp, "B")
    connect(normal_strength, "", normal_lerp, "Alpha")

    rough_tex = tex_param("RoughnessTex", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1120, 60, flat_mask)
    rough_scalar = scalar("Roughness", 0.55, -1400, -20)
    use_rough = scalar("UseRoughnessTex", 0.0, -1400, 130)
    rough_mul = expr("MaterialExpressionMultiply", -820, 60)
    connect(rough_tex, "R", rough_mul, "A")
    connect(rough_scalar, "", rough_mul, "B")
    rough_lerp = expr("MaterialExpressionLinearInterpolate", -600, 20)
    connect(rough_scalar, "", rough_lerp, "A")
    connect(rough_mul, "", rough_lerp, "B")
    connect(use_rough, "", rough_lerp, "Alpha")

    # Metallic gets a map as well as a scalar. The quadruped ships a real metal mask - plating
    # over hide - and collapsing that to one value is what turned the whole animal into either
    # rubber or a mirror depending on which way the scalar was pushed.
    metal_tex = tex_param("MetallicTex", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1120, 200, flat_mask)
    metal_scalar = scalar("Metallic", 0.1, -1400, 160)
    use_metal = scalar("UseMetallicTex", 0.0, -1400, 250)
    metal_mul = expr("MaterialExpressionMultiply", -820, 200)
    connect(metal_tex, "R", metal_mul, "A")
    connect(metal_scalar, "", metal_mul, "B")
    metallic = expr("MaterialExpressionLinearInterpolate", -600, 160)
    connect(metal_scalar, "", metallic, "A")
    connect(metal_mul, "", metallic, "B")
    connect(use_metal, "", metallic, "Alpha")
    # 0.5 is the dielectric default and it is too hot for a body standing under a dedicated fill
    # light: the specular lobe, not the albedo, is what made these read as wet plastic.
    specular = scalar("Specular", 0.32, -600, 230)

    # --- ambient occlusion ------------------------------------------------------------
    ao_tex = tex_param("AOTex", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -1120, 1100, flat_mask)
    use_ao = scalar("UseAOTex", 0.0, -1400, 1240)
    one = expr("MaterialExpressionConstant", -1400, 1180)
    one.set_editor_property("r", 1.0)
    ao_lerp = expr("MaterialExpressionLinearInterpolate", -600, 1140)
    connect(one, "", ao_lerp, "A")
    connect(ao_tex, "R", ao_lerp, "B")
    connect(use_ao, "", ao_lerp, "Alpha")

    # --- detail normal ----------------------------------------------------------------
    # The base normal is one texel per body-sized UV island; at contact range that is a smooth
    # shape with a picture on it. This is the surface itself - pores and scale - tiled far
    # tighter, added to the base normal's tangent XY and renormalised.
    detail_uv_tile = scalar("DetailUVTile", 12.0, -1900, 640)
    detail_coord = expr("MaterialExpressionTextureCoordinate", -1900, 560)
    detail_uv = expr("MaterialExpressionMultiply", -1700, 600)
    connect(detail_coord, "", detail_uv, "A")
    connect(detail_uv_tile, "", detail_uv, "B")
    detail_tex = tex_param("DetailNormalTex", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -1400, 700, detail_normal)
    connect(detail_uv, "", detail_tex, "UVs")
    detail_strength = scalar("DetailNormalStrength", 0.65, -1400, 860)

    base_rg = expr("MaterialExpressionComponentMask", -420, 340)
    base_rg.set_editor_property("r", True)
    base_rg.set_editor_property("g", True)
    connect(normal_lerp, "", base_rg, "")
    base_b = expr("MaterialExpressionComponentMask", -420, 470)
    base_b.set_editor_property("r", False)
    base_b.set_editor_property("g", False)
    base_b.set_editor_property("b", True)
    connect(normal_lerp, "", base_b, "")
    detail_rg = expr("MaterialExpressionComponentMask", -1100, 700)
    detail_rg.set_editor_property("r", True)
    detail_rg.set_editor_property("g", True)
    connect(detail_tex, "RGB", detail_rg, "")
    detail_scaled = expr("MaterialExpressionMultiply", -900, 720)
    connect(detail_rg, "", detail_scaled, "A")
    connect(detail_strength, "", detail_scaled, "B")
    summed_rg = expr("MaterialExpressionAdd", -250, 380)
    connect(base_rg, "", summed_rg, "A")
    connect(detail_scaled, "", summed_rg, "B")
    combined = expr("MaterialExpressionAppendVector", -120, 400)
    connect(summed_rg, "", combined, "A")
    connect(base_b, "", combined, "B")
    final_normal = expr("MaterialExpressionNormalize", 20, 400)
    connect(combined, "", final_normal, "VectorInput")

    # Eyes and vents. Creatures read as dead silhouettes in Erebus without a hot spot, and the
    # emissive mask is the only thing on the body bright enough to survive the fog.
    emissive_tex = tex_param("EmissiveMask", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -1120, 760, white)
    emissive_color = vector("EmissiveColor", (1.0, 0.3, 0.12, 1.0), -1400, 900)
    emissive_strength = scalar("EmissiveStrength", 0.0, -1400, 1030)
    emissive_mul = expr("MaterialExpressionMultiply", -820, 820)
    connect(emissive_tex, "RGB", emissive_mul, "A")
    connect(emissive_color, "", emissive_mul, "B")
    emissive_final = expr("MaterialExpressionMultiply", -600, 880)
    connect(emissive_mul, "", emissive_final, "A")
    connect(emissive_strength, "", emissive_final, "B")

    MEL.connect_material_property(base_lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(final_normal, "", unreal.MaterialProperty.MP_NORMAL)
    MEL.connect_material_property(rough_lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.connect_material_property(metallic, "", unreal.MaterialProperty.MP_METALLIC)
    MEL.connect_material_property(specular, "", unreal.MaterialProperty.MP_SPECULAR)
    MEL.connect_material_property(emissive_final, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(ao_lerp, "", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    # Without this the body renders as the engine default grey and only logs a warning.
    material.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(MASTER_PATH, only_if_is_dirty=False)
    _log("authored master " + MASTER_PATH)
    return material


# --- import ----------------------------------------------------------------------------
def _build_options(spec):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", True)
    # Materials are imported only for their names: the FBX's own slot labels ("rock", "LEATHER",
    # "Red-Eye-Alien-Animal") are what tell one part of a body from another, and they are lost if
    # the importer is told to skip materials. Textures are imported explicitly further down,
    # because no FBX here carries a usable embedded or resolvable image path.
    options.set_editor_property("import_materials", True)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", bool(spec.get("import_animations")))
    options.set_editor_property("create_physics_asset", True)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)

    mesh_data = options.get_editor_property("skeletal_mesh_import_data")
    mesh_data.set_editor_property("convert_scene", True)
    mesh_data.set_editor_property("force_front_x_axis", False)
    mesh_data.set_editor_property("convert_scene_unit", True)
    mesh_data.set_editor_property("import_morph_targets", False)
    mesh_data.set_editor_property("preserve_smoothing_groups", True)
    mesh_data.set_editor_property("compute_weighted_normals", True)
    mesh_data.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    # False on purpose, including for the rigid model. With it on, that model's eighteen unskinned
    # parts each imported as a separate skeletal mesh; off, the whole node hierarchy becomes one
    # rigid skeletal mesh with a bone per part, which is the body the game needs.
    mesh_data.set_editor_property("import_meshes_in_bone_hierarchy", False)
    if spec.get("import_animations"):
        anim_data = options.get_editor_property("anim_sequence_import_data")
        anim_data.set_editor_property("import_bone_tracks", True)
        anim_data.set_editor_property("remove_redundant_keys", True)
        anim_data.set_editor_property("convert_scene", True)
    return options


def import_model(name, spec):
    source = _resolve_source(spec["fbx"])
    if not os.path.isfile(source):
        _fail("source missing for %s: %s" % (name, source))
        return None

    destination = "%s/%s" % (ENEMY_ROOT, name)
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", source)
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", "SKM_" + name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("replace_existing_settings", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", _build_options(spec))
    TOOLS.import_asset_tasks([task])

    imported = list(task.get_editor_property("imported_object_paths") or [])
    meshes = []
    for path in imported:
        asset = unreal.load_asset(path.split(".")[0])
        if isinstance(asset, unreal.SkeletalMesh):
            meshes.append(asset)
    if not meshes:
        _fail("%s produced no skeletal mesh (imported: %s)" % (name, imported))
        return None
    if len(meshes) > 1:
        # An unskinned FBX imports one skeletal mesh per top-level node. For the armoured figure
        # that is the full body plus three small detached props; the body carries every material
        # slot, so it is the one that becomes the enemy. Name the dropped parts rather than
        # letting a silent pick look like a clean import.
        meshes.sort(key=lambda mesh: mesh.get_bounds().box_extent.z, reverse=True)
        _log("%s split into %d skeletal meshes; using %s, leaving %s in the folder unused" % (
            name, len(meshes), meshes[0].get_name(),
            ", ".join(other.get_name() for other in meshes[1:])))

    body = meshes[0]
    for extra in meshes[1:]:
        # Unused fragments still cook and still show up in the content browser next to the real
        # body, which is exactly how the wrong mesh ends up referenced later.
        extra_skeleton = extra.get_editor_property("skeleton")
        unreal.EditorAssetLibrary.delete_asset(unreal.SystemLibrary.get_path_name(extra).split(".")[0])
        if extra_skeleton:
            unreal.EditorAssetLibrary.delete_asset(
                unreal.SystemLibrary.get_path_name(extra_skeleton).split(".")[0])

    expected = "SKM_" + name
    if body.get_name() != expected:
        source_path = "%s/%s" % (destination, body.get_name())
        if unreal.EditorAssetLibrary.rename_asset(source_path, "%s/%s" % (destination, expected)):
            body = _load("%s/%s" % (destination, expected)) or body
    skeleton = body.get_editor_property("skeleton")
    if skeleton and skeleton.get_name() != expected + "_Skeleton":
        unreal.EditorAssetLibrary.rename_asset(
            unreal.SystemLibrary.get_path_name(skeleton).split(".")[0],
            "%s/%s_Skeleton" % (destination, expected))

    if spec.get("import_animations"):
        import_animations(name, spec, body, destination)
    if spec.get("anim_files"):
        import_animation_files(name, spec, body, destination)
    _log("%s imported %d object(s) from %s" % (name, len(imported), spec["fbx"]))
    return body


def import_animation_files(name, spec, mesh, destination):
    """One take per file, for a model whose animation was re-exported alongside its mesh.

    Separate files rather than one multi-take FBX because Unreal's importer only reliably picks
    up the first take from a Blender export, and a silently dropped take here is a creature that
    never bites.
    """
    skeleton = mesh.get_editor_property("skeleton")
    if not skeleton:
        _fail("%s has no skeleton to import takes against" % name)
        return []

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("skeleton", skeleton)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    anim_data = options.get_editor_property("anim_sequence_import_data")
    anim_data.set_editor_property("import_bone_tracks", True)
    anim_data.set_editor_property("remove_redundant_keys", True)
    anim_data.set_editor_property("convert_scene", True)

    written = []
    for take, relative in sorted(spec["anim_files"].items()):
        source = _resolve_source(relative)
        if not os.path.isfile(source):
            raise RuntimeError("%s take %s missing: %s (run Scripts/PrepareCreatureSources.py)"
                               % (name, take, source))
        asset_name = "AS_%s_%s" % (name, take)
        full = "%s/%s" % (destination, asset_name)
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", source)
        task.set_editor_property("destination_path", destination)
        task.set_editor_property("destination_name", asset_name)
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", False)
        task.set_editor_property("options", options)
        TOOLS.import_asset_tasks([task])
        clip = _load(full)
        if not isinstance(clip, unreal.AnimSequence):
            raise RuntimeError("%s take %s did not import as an AnimSequence" % (name, take))
        # A take that arrives with a single key is a take that did not arrive: the exporter
        # wrote the file, the importer accepted it, and the body stands still.
        if clip.get_play_length() < 0.1:
            raise RuntimeError("%s take %s is %.3fs long" % (name, take, clip.get_play_length()))
        unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
        written.append(full)
        _log("%s take %s -> %s (%.2fs)" % (name, take, asset_name, clip.get_play_length()))
    return written


def import_animations(name, spec, mesh, destination):
    """Second pass for the takes, against the skeleton the mesh import just created.

    The combined mesh+animation import is not reliable across these files - the FBX 6000 alien
    yielded its idle take on one run and nothing on the next - so the takes are pulled explicitly
    with the skeleton already decided. Nothing happens if the mesh pass already produced them.
    """
    existing = [path for path in _assets_under(destination) if isinstance(_load(path), unreal.AnimSequence)]
    if existing:
        return existing
    skeleton = mesh.get_editor_property("skeleton")
    if not skeleton:
        _fail("%s has no skeleton to import animations against" % name)
        return []

    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("skeleton", skeleton)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    anim_data = options.get_editor_property("anim_sequence_import_data")
    anim_data.set_editor_property("import_bone_tracks", True)
    anim_data.set_editor_property("remove_redundant_keys", True)
    anim_data.set_editor_property("convert_scene", True)

    task = unreal.AssetImportTask()
    task.set_editor_property("filename", _resolve_source(spec["fbx"]))
    task.set_editor_property("destination_path", destination)
    task.set_editor_property("destination_name", "A_" + name)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])

    found = [path for path in _assets_under(destination) if isinstance(_load(path), unreal.AnimSequence)]
    if not found:
        _fail("%s animation pass produced nothing" % name)
    return found


def import_textures(name, spec):
    """Import each listed source image once, named by the set and map it belongs to.

    Compression is corrected per map type here. Imported images default to sRGB DXT1 regardless
    of what they hold, so a roughness map arrives gamma-encoded and a normal map loses its
    precision - both show up as flat, plastic shading rather than as an obvious error.
    """
    folder = "%s/%s" % (ENEMY_ROOT, name)
    sets = {}
    for set_name, maps in (spec.get("texture_sets") or {}).items():
        resolved = {}
        for kind, relative in maps.items():
            source = _resolve_source(relative)
            if not os.path.isfile(source):
                _fail("%s texture missing: %s" % (name, source))
                continue
            asset_name = "T_%s_%s_%s" % (name, set_name.capitalize(), kind.capitalize())
            full = "%s/%s" % (folder, asset_name)
            task = unreal.AssetImportTask()
            task.set_editor_property("filename", source)
            task.set_editor_property("destination_path", folder)
            task.set_editor_property("destination_name", asset_name)
            task.set_editor_property("automated", True)
            task.set_editor_property("replace_existing", True)
            task.set_editor_property("save", False)
            TOOLS.import_asset_tasks([task])

            texture = _load(full)
            if not texture:
                _fail("%s texture import failed: %s" % (name, asset_name))
                continue
            if kind == "normal":
                texture.set_editor_property("srgb", False)
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
                texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_NORMAL_MAP)
            elif kind in ("roughness", "metallic", "mask", "ao"):
                texture.set_editor_property("srgb", False)
                texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
                texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER_SPECULAR)
            else:
                texture.set_editor_property("srgb", True)
                texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_CHARACTER)
            unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
            resolved[kind] = full
        sets[set_name] = resolved
    return sets


def _set_for_slot(spec, slot_name):
    lowered = (slot_name or "").lower()
    for token, set_name in spec.get("slot_sets") or []:
        if token in lowered:
            return set_name
    return spec.get("default_set")


def build_material_instances(name, spec, mesh, texture_sets):
    """Re-parent every body slot onto M_EnemyCreature with the maps that dress that slot.

    Slot names come from the source materials ("Red-Eye-Alien-Animal", "LEATHER", "rock"), which
    is the only signal available for telling one part of a body from another, and the only way
    to decide which slot is the part that should glow.
    """
    master = _load(MASTER_PATH)
    if not master:
        _fail("master missing; cannot instance for " + name)
        return []

    instance_dir = "%s/%s" % (ENEMY_ROOT, name)
    materials = mesh.get_editor_property("materials")
    built = []
    stale = set()

    # Same contract as the master's sampler nodes: an instance that overrides a texture parameter
    # with the wrong compression breaks the instance's shader map, not just the sample.
    param_compression = {
        "NormalTex": unreal.TextureCompressionSettings.TC_NORMALMAP,
        "DetailNormalTex": unreal.TextureCompressionSettings.TC_NORMALMAP,
        "RoughnessTex": unreal.TextureCompressionSettings.TC_MASKS,
        "MetallicTex": unreal.TextureCompressionSettings.TC_MASKS,
        "AOTex": unreal.TextureCompressionSettings.TC_MASKS,
    }

    def set_texture(instance, param, texture):
        required = param_compression.get(param)
        if required is not None and texture.get_editor_property("compression_settings") != required:
            raise RuntimeError("%s %s expects %s, got %s for %s" % (
                name, param, required, texture.get_editor_property("compression_settings"), texture.get_name()))
        MEL.set_material_instance_texture_parameter_value(instance, param, texture)

    for index, slot in enumerate(materials):
        # The slot name is the FBX's own material name and survives even when the imported
        # material asset does not; the material's own name is only a fallback.
        slot_label = str(slot.get_editor_property("material_slot_name") or "")
        source_material = slot.get_editor_property("material_interface")
        source_name = slot_label or (source_material.get_name() if source_material else "")
        lowered = source_name.lower()
        is_glow_slot = any(token in lowered for token in GLOW_TOKENS)
        set_name = _set_for_slot(spec, source_name)
        maps = texture_sets.get(set_name) or {}
        # Stable slot-specific overrides let one multi-material body keep distinct metal, hide,
        # glow, and stone response without multiplying every surface by the same value.
        tuning = dict(spec)
        tuning.update((spec.get("slot_tuning") or {}).get(index, {}))

        instance_name = "MI_%s_%02d" % (name, index)
        full = "%s/%s" % (instance_dir, instance_name)
        instance = _load(full)
        if not instance:
            instance = TOOLS.create_asset(
                instance_name, instance_dir, unreal.MaterialInstanceConstant,
                unreal.MaterialInstanceConstantFactoryNew())
        if not instance:
            _fail("failed to create " + full)
            continue
        MEL.set_material_instance_parent(instance, master)

        emissive = _load(maps["emissive"]) if maps.get("emissive") else None
        albedo = emissive if (is_glow_slot and emissive) else (_load(maps["color"]) if maps.get("color") else None)
        MEL.set_material_instance_vector_parameter_value(
            instance, "BaseTint", unreal.LinearColor(*(list(tuning["tint"]) + [1.0])))
        MEL.set_material_instance_scalar_parameter_value(instance, "UseBaseColorTex", 1.0 if albedo else 0.0)
        if albedo:
            set_texture(instance, "BaseColorTex", albedo)

        normal = None if is_glow_slot else (_load(maps["normal"]) if maps.get("normal") else None)
        MEL.set_material_instance_scalar_parameter_value(
            instance, "NormalStrength", tuning.get("normal_strength", 1.0) if normal else 0.0)
        MEL.set_material_instance_scalar_parameter_value(
            instance, "DetailNormalStrength", tuning.get("detail_normal_strength", 0.65))
        if normal:
            set_texture(instance, "NormalTex", normal)

        ao = None if is_glow_slot else (_load(maps["ao"]) if maps.get("ao") else None)
        MEL.set_material_instance_scalar_parameter_value(instance, "UseAOTex", 1.0 if ao else 0.0)
        if ao:
            set_texture(instance, "AOTex", ao)

        rough = None if is_glow_slot else (_load(maps["roughness"]) if maps.get("roughness") else None)
        MEL.set_material_instance_scalar_parameter_value(
            instance, "Roughness", tuning["roughness"])
        MEL.set_material_instance_scalar_parameter_value(instance, "UseRoughnessTex", 1.0 if rough else 0.0)
        if rough:
            set_texture(instance, "RoughnessTex", rough)
        metal = None if is_glow_slot else (_load(maps["metallic"]) if maps.get("metallic") else None)
        MEL.set_material_instance_scalar_parameter_value(
            instance, "Metallic", tuning["metallic"])
        MEL.set_material_instance_scalar_parameter_value(instance, "UseMetallicTex", 1.0 if metal else 0.0)
        if metal:
            set_texture(instance, "MetallicTex", metal)
        MEL.set_material_instance_scalar_parameter_value(
            instance, "Specular", tuning.get("specular", 0.28))

        MEL.set_material_instance_vector_parameter_value(
            instance, "EmissiveColor", unreal.LinearColor(*(list(tuning["emissive"]) + [1.0])))
        MEL.set_material_instance_scalar_parameter_value(
            instance, "EmissiveStrength", tuning["emissive_strength"] if is_glow_slot else 0.0)
        if is_glow_slot and emissive:
            set_texture(instance, "EmissiveMask", emissive)

        unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
        # MaterialInterface, not Material: the FBX importer's translations arrive as
        # MaterialInstanceConstant parented to the engine's FBXLegacyPhongSurfaceMaterial, so an
        # isinstance check against Material never matched and they were left behind to cook.
        if source_material and isinstance(source_material, unreal.MaterialInterface) and source_material != instance:
            # The importer's translation of the FBX material has served its purpose - it carried
            # the slot name here. Nothing references it once the slot points at the instance.
            # The prefix test is not paranoia: a slot the importer could not fill points at
            # /Engine/EngineMaterials/WorldGridMaterial, and deleting that from the engine
            # install stops every later editor process from booting at all.
            source_path = unreal.SystemLibrary.get_path_name(source_material).split(".")[0]
            if source_path.startswith(instance_dir + "/") and not source_path.startswith(instance_dir + "/MI_"):
                stale.add(source_path)
        slot.set_editor_property("material_interface", instance)
        slot.set_editor_property("material_slot_name", unreal.Name("%s_%02d" % (name, index)))
        # Iterating an unreal.Array of USTRUCTs yields COPIES. Without this assignment every
        # set_editor_property above is discarded and the body keeps the importer's untextured
        # material - which renders as plain grey and logs nothing at all.
        materials[index] = slot
        built.append(full)
        _log("%s slot %d source=%s set=%s glow=%s maps=%s" % (
            name, index, source_name or "<none>", set_name, is_glow_slot, sorted(maps.keys())))

    mesh.set_editor_property("materials", materials)

    # Read it back. This assignment has silently failed before; a grey body in a dark scene is
    # not something a log line about "authored 5 instances" would ever catch.
    applied = mesh.get_editor_property("materials")
    for index, slot in enumerate(applied):
        assigned = slot.get_editor_property("material_interface")
        expected = "MI_%s_%02d" % (name, index)
        if not assigned or assigned.get_name() != expected:
            _fail("%s slot %d did not take the instance: expected %s, got %s" % (
                name, index, expected, assigned.get_name() if assigned else "<none>"))
    return built, sorted(stale)


def ensure_physics_asset(name, mesh):
    """A body with no physics asset cannot ragdoll and has no bone to resolve a headshot to.

    The FBX importer's create_physics_asset flag produced nothing for any of these meshes, so
    the asset is built explicitly and bound to the mesh.
    """
    folder = "%s/%s" % (ENEMY_ROOT, name)
    full = "%s/PA_%s" % (folder, name)
    existing = _load(full)
    if existing:
        mesh.set_editor_property("physics_asset", existing)
        return full
    try:
        subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
        physics = subsystem.create_physics_asset(mesh)
    except Exception as exc:
        _fail("%s physics asset creation failed: %s" % (name, exc))
        return None
    if not physics:
        _fail("%s physics asset creation returned nothing" % name)
        return None
    created_path = unreal.SystemLibrary.get_path_name(physics).split(".")[0]
    if created_path != full:
        unreal.EditorAssetLibrary.rename_asset(created_path, full)
        physics = _load(full) or physics
    subsystem.assign_physics_asset(mesh, physics)
    unreal.EditorAssetLibrary.save_asset(full, only_if_is_dirty=False)
    return full


def tune_mesh(name, mesh):
    """Higher-precision tangents. No generated LOD chain - see below.

    High-precision tangents are the difference between clean and banded shading on a normal-
    mapped creature.

    ponytail: LOD0 only. `regenerate_lod` with auto screen sizes drops to LOD2 at roughly half a
    screen height, which for a 190cm body is about seven metres - well inside this game's
    engagement ranges (the Pilgrim fights at 1200uu), so every enemy the player actually looked at
    was a faceted low-poly shell. These are ~20k-vert meshes and at most eight are active, which
    is the same order as the mannequin they replace. Add LODs back only with authored screen
    sizes, not the automatic ones.
    """
    subsystem = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
    try:
        build = subsystem.get_lod_build_settings(mesh, 0)
        build.set_editor_property("use_high_precision_tangent_basis", True)
        build.set_editor_property("recompute_normals", False)
        build.set_editor_property("recompute_tangents", False)
        build.set_editor_property("remove_degenerates", True)
        subsystem.set_lod_build_settings(mesh, 0, build)
    except Exception as exc:
        unreal.log_warning("[EnemyModels] %s LOD0 build settings unavailable: %s" % (name, exc))

    try:
        # Drop any chain a previous run generated; the import itself only ever produces LOD0.
        if subsystem.get_lod_count(mesh) > 1:
            subsystem.remove_lods(mesh, list(range(1, subsystem.get_lod_count(mesh))))
    except Exception as exc:
        unreal.log_warning("[EnemyModels] %s LOD strip skipped: %s" % (name, exc))


def describe(name, mesh, folder, physics_asset):
    """Bounds and animations, written out for the definition author script.

    Nothing here knows how tall a "40 unit" spider is meant to be, so the mesh scale, capsule and
    body offset each archetype spawns with are derived from the imported bounds, not guessed.
    """
    bounds = mesh.get_bounds()
    extent = bounds.box_extent
    origin = bounds.origin
    animations = [path for path in _assets_under(folder) if isinstance(_load(path), unreal.AnimSequence)]
    return {
        "mesh": unreal.SystemLibrary.get_path_name(mesh).split(".")[0],
        "skeleton": unreal.SystemLibrary.get_path_name(mesh.get_editor_property("skeleton")).split(".")[0],
        "physics_asset": physics_asset,
        "animations": sorted(animations),
        "extent": [extent.x, extent.y, extent.z],
        "origin": [origin.x, origin.y, origin.z],
        "height_cm": extent.z * 2.0,
    }


def main():
    if not os.path.isdir(SOURCE_ROOT):
        raise RuntimeError("enemy source folder not found: " + SOURCE_ROOT)
    # Pre-flight before the first delete. Each model's folder is wiped and rebuilt, so a source
    # drop that is missing one FBX would otherwise destroy that archetype's content, rewrite the
    # manifest without it, and exit 0 - with the failure only surfacing in the next script, after
    # the assets are already gone.
    missing = []
    for spec in MODELS.values():
        for relative in [spec["fbx"]] + list((spec.get("anim_files") or {}).values()):
            if not os.path.isfile(_resolve_source(relative)):
                missing.append(relative)
    if not os.path.isdir(BAKED_ROOT):
        raise RuntimeError("run Scripts/BakeCreatureTextures.py first: " + BAKED_ROOT)
    if missing:
        raise RuntimeError("enemy source incomplete, nothing deleted: " + ", ".join(missing))
    unreal.EditorAssetLibrary.make_directory(ENEMY_ROOT)
    import_shared_textures()
    author_creature_master()

    manifest = {}
    for name, spec in MODELS.items():
        folder = "%s/%s" % (ENEMY_ROOT, name)
        if folder == SHARED_DIR:
            continue
        # A re-import with different options can leave the previous run's stray meshes behind,
        # and a stale fragment is worse than no model at all: it spawns and looks correct.
        if unreal.EditorAssetLibrary.does_directory_exist(folder):
            unreal.EditorAssetLibrary.delete_directory(folder)
        unreal.EditorAssetLibrary.make_directory(folder)

        mesh = import_model(name, spec)
        if not mesh:
            continue
        texture_sets = import_textures(name, spec)
        _, stale_materials = build_material_instances(name, spec, mesh, texture_sets)
        physics_asset = ensure_physics_asset(name, mesh)
        tune_mesh(name, mesh)
        unreal.EditorAssetLibrary.save_directory(folder, only_if_is_dirty=False, recursive=True)
        # After the save, so the mesh package on disk no longer names the translated materials.
        for path in stale_materials:
            unreal.EditorAssetLibrary.delete_asset(path)

        entry = describe(name, mesh, folder, physics_asset)
        entry["target_height_cm"] = spec["target_height_cm"]
        entry["texture_sets"] = texture_sets
        manifest[name] = entry
        _log("%s mesh=%s height=%.1fcm anims=%d physics=%s" % (
            name, entry["mesh"], entry["height_cm"], len(entry["animations"]), physics_asset))

    with open(MANIFEST_PATH, "w") as handle:
        json.dump(manifest, handle, indent=2, sort_keys=True)
    _log("wrote manifest " + MANIFEST_PATH)
    if len(manifest) != len(MODELS):
        # Raise rather than log: a partial run that exits 0 leaves the manifest and the content
        # folders disagreeing, and the next script fails somewhere far from the cause.
        raise RuntimeError("expected %d models, imported %d" % (len(MODELS), len(manifest)))


if __name__ == "__main__":
    main()
