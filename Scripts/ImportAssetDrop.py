"""Import the external art/audio drop in ~/Downloads/new into the Ashes content tree.

Four static-mesh packs (two rock collections, the stylized street props, the military
radio set) and a 50-clip sound library. Animation packs are handled separately by
Scripts/ImportAnimationDrop.py - they need a skeleton, which these do not.

Run with UnrealEditor-Cmd; idempotent, re-running rebuilds in place. Source files are
read from AH_ASSET_DROP (default ~/Downloads/new) and are never modified.

Three things decide most of the code:
  * every pack ships its own texture layout, so each one declares how a mesh name maps
    to a texture set rather than relying on a shared convention;
  * the packs that ship ORM maps pack roughness in green and metalness in blue, while
    the Erebus texture family keeps roughness in red - M_ErebusSurface takes
    RoughChannelMask/MetallicChannelMask so one master serves both;
  * an unreal.Array of USTRUCTs yields copies, so a material slot has to be written
    back into the array and the whole array set again, or the assignment is discarded.
"""

import os

import unreal


SOURCE_ROOT = os.environ.get("AH_ASSET_DROP", os.path.expanduser("~/Downloads/new"))
INSTANCE_DIR = "/Game/Ashes/Materials/Instances"
MASTER = "/Game/Ashes/Materials/M_ErebusSurface"

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary

# ORM channel layout: R ambient occlusion, G roughness, B metalness.
ORM_ROUGH = unreal.LinearColor(0.0, 1.0, 0.0, 0.0)
ORM_METAL = unreal.LinearColor(0.0, 0.0, 1.0, 0.0)
GREY_ROUGH = unreal.LinearColor(1.0, 0.0, 0.0, 0.0)
NO_METAL = unreal.LinearColor(0.0, 0.0, 0.0, 0.0)

REPORT = []


# --- packs -----------------------------------------------------------------------------
# "sets" maps a texture-set name to {albedo, normal, rough} paths relative to the pack
# root, plus the channel masks that set needs. "meshes" maps each source FBX to the set
# that dresses it. A mesh with no set gets the pack's flat instance instead.

def rock_collection_04():
    root = "Environment - Rock Collection 04/rock_collection_04"
    meshes, sets = {}, {}
    for index in range(1, 8):
        tag = "Rock_%02d" % index
        meshes["%s/%s/Meshes/SM_%s.fbx" % (root, tag, tag)] = tag
        sets[tag] = {
            "albedo": "%s/%s/Textures/T_%s_A.tga" % (root, tag, tag),
            "normal": "%s/%s/Textures/T_%s_N.tga" % (root, tag, tag),
            "rough": "%s/%s/Textures/T_%s_ORM.tga" % (root, tag, tag),
            "rough_mask": ORM_ROUGH,
            "metal_mask": NO_METAL,
        }
    return dict(name="RockCollection04", dest="/Game/Ashes/Environment/Rocks/Collection04",
                meshes=meshes, sets=sets, nanite=True)


def rock_pack_vol_01():
    root = "Rock Pack Vol 01"
    mesh_dir = root + "/rock_pack_vol_01_g5n1k9t4l7"
    meshes, sets = {}, {}
    for index in range(1, 5):
        tag = "RP_%02d" % index
        sets[tag] = {
            "albedo": "%s/Texture/T_RP_Vol_01_%02d_BaseColor.jpg" % (root, index),
            "normal": "%s/Texture/T_RP_Vol_01_%02d_Normal.jpg" % (root, index),
            "rough": "%s/Texture/T_RP_Vol_01_%02d_Roughness.jpg" % (root, index),
            "rough_mask": GREY_ROUGH,
            "metal_mask": NO_METAL,
        }
    # Twelve meshes ship with four texture sets and no mapping file. Rocks are
    # interchangeable, so they are dealt round-robin; swap any individual mesh in the
    # editor if a particular set reads better on it.
    for index in range(1, 13):
        meshes["%s/SM_RP_Vol_01_%02d.fbx" % (mesh_dir, index)] = "RP_%02d" % ((index - 1) % 4 + 1)
    return dict(name="RockPackVol01", dest="/Game/Ashes/Environment/Rocks/PackVol01",
                meshes=meshes, sets=sets, nanite=True)


def street_props():
    root = "Stylized NYC Street Props/FBX"
    full = os.path.join(SOURCE_ROOT, root)
    meshes = {}
    if os.path.isdir(full):
        for name in sorted(os.listdir(full)):
            if not name.lower().endswith(".fbx"):
                continue
            # SM_Trash_Bin ships broken in this pack: the FBX holds an empty "Circle_028"
            # object ("No polygons were found") and the OBJ is a 98-byte header with no
            # geometry. SM_Bin from the same pack is the intact bin.
            if name.lower() == "sm_trash_bin.fbx":
                continue
            meshes[root + "/" + name] = None  # pack ships no textures
    return dict(name="StreetProps", dest="/Game/Ashes/Props/Street", meshes=meshes, sets={},
                nanite=False, flat_tint=(0.22, 0.22, 0.23), flat_scalars={"Roughness": 0.85,
                                                                          "GrimeAmount": 0.5,
                                                                          "WearAmount": 0.45})


def military_radio():
    root = "Military Radio/MilitaryRadio"
    tex = root + "/T_Textures"
    sets = {
        "HandRadio": {"albedo": tex + "/HandRadio/T_HandRadio_BaseColor.png",
                      "normal": tex + "/HandRadio/T_HandRadio_Normal.png",
                      "rough": tex + "/HandRadio/T_HandRadio_ORM.png"},
        "Headphones": {"albedo": tex + "/HeadPhone/T_Headphones_BaseColor.png",
                       "normal": tex + "/HeadPhone/T_Headphones_Normal.png",
                       "rough": tex + "/HeadPhone/T_Headphone_ORM.png"},
        "HeadphonesHolder": {"albedo": tex + "/HeadPhonesHolder/T_HeadphonesHolder_BaseColor.png",
                             "normal": tex + "/HeadPhonesHolder/T_HeadphonesHolder_Normal.png",
                             "rough": tex + "/HeadPhonesHolder/T_HeadphoneHolder_ORM.png"},
        "WalkieTalkie": {"albedo": tex + "/T_WalkieTalkie/T_WalkieTalkie_BaseColor.png",
                         "normal": tex + "/T_WalkieTalkie/T_WalkieTalkie_Normal.png",
                         "rough": tex + "/T_WalkieTalkie/T_WalkieTalkie_ORM.png"},
        "Radio": {"albedo": tex + "/T_Radio/T_Radio_BaseColor_1.png",
                  "normal": tex + "/T_Radio/T_Radio_Normal_1.png",
                  "rough": tex + "/T_Radio/T_Radio_ORM_1.png"},
        "Ampfilter": {"albedo": tex + "/T_Ampfilter/T_Ampfilter_BaseColor_1.png",
                      "normal": tex + "/T_Ampfilter/T_Ampfilter_Normal_1.png",
                      "rough": tex + "/T_Ampfilter/T_Ampfilter_ORM_1.png"},
        "Telephone": {"albedo": tex + "/Telephone/T_Telephone_BaseColor.png",
                      "normal": tex + "/Telephone/T_Telephone_Normal.png.png",
                      "rough": tex + "/Telephone/T_Telephone_ORM.png"},
    }
    for entry in sets.values():
        entry["rough_mask"] = ORM_ROUGH
        entry["metal_mask"] = ORM_METAL
    models = root + "/SM_Models"
    meshes = {
        models + "/SM_HandRadio.fbx": "HandRadio",
        models + "/SM_Headphones.fbx": "Headphones",
        models + "/SM_HeadphonesHolder.fbx": "HeadphonesHolder",
        models + "/SM_WalkieTalkie_LongAntenna.fbx": "WalkieTalkie",
        models + "/SM_WalkieTalkie_ShortAntenna.fbx": "WalkieTalkie",
        models + "/SM_WalkieTalkie_GPS.fbx": "WalkieTalkie",
        models + "/SM_WalkieTalkie_battery.fbx": "WalkieTalkie",
        models + "/SM_WalkieTalkie_empty.fbx": "WalkieTalkie",
        models + "/SM_WailkieTalkieHolder.fbx": "WalkieTalkie",
        models + "/SM_RadioBattery.fbx": "WalkieTalkie",
        models + "/SM_Radio.fbx": "Radio",
        models + "/SM_Ampfilter.fbx": "Ampfilter",
        models + "/SM_Telephone.fbx": "Telephone",
    }
    return dict(name="MilitaryRadio", dest="/Game/Ashes/Props/Radio", meshes=meshes, sets=sets,
                nanite=False)


def ladder_set():
    """The VanillaLoop sample's ladder modules, unpacked by Scripts/ImportAnimationDrop.py."""
    root = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        "UnityDropSource", "VanillaLoopStudio", "FreeSampleAnimationSet", "Art", "Meshes",
        "LadderSet")
    meshes = {}
    if os.path.isdir(root):
        for name in sorted(os.listdir(root)):
            if name.lower().endswith(".fbx"):
                meshes[os.path.join(root, name)] = None
    return dict(name="LadderSet", dest="/Game/Ashes/Props/Ladder", meshes=meshes, sets={},
                nanite=False, flat_tint=(0.20, 0.20, 0.21),
                flat_scalars={"Roughness": 0.70, "Metallic": 0.85, "GrimeAmount": 0.55})


PACKS = [rock_collection_04, rock_pack_vol_01, street_props, military_radio, ladder_set]

SOUND_PACK = dict(source="50 Free Game Sounds Pack/Free Sounds Pack",
                  dest="/Game/Ashes/Audio/Library")


# --- import helpers --------------------------------------------------------------------
def _run_task(filename, destination_path, destination_name, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination_path
    task.destination_name = destination_name
    task.automated = True
    task.replace_existing = True
    task.save = False
    if options is not None:
        task.set_editor_property("options", options)
    TOOLS.import_asset_tasks([task])
    return unreal.load_asset(destination_path + "/" + destination_name)


def import_texture(source, dest_path, kind):
    """kind: 'albedo' (sRGB colour), 'normal', or 'mask' (linear packed data)."""
    stem = os.path.basename(source)
    while "." in stem:  # one file in the radio pack is named "....Normal.png.png"
        stem = stem.rsplit(".", 1)[0]
    if stem.startswith("T_"):
        stem = stem[2:]
    name = "T_" + stem
    full = dest_path + "/" + name
    if EAL.does_asset_exist(full):
        return unreal.load_asset(full)
    texture = _run_task(source, dest_path, name)
    if not texture:
        REPORT.append("MISSING texture %s" % source)
        return None
    if kind == "normal":
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    elif kind == "mask":
        texture.set_editor_property("srgb", False)
        texture.set_editor_property("compression_settings",
                                    unreal.TextureCompressionSettings.TC_MASKS)
    else:
        texture.set_editor_property("srgb", True)
    EAL.save_asset(full)
    return texture


def static_mesh_options(nanite):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", True)
    options.set_editor_property("import_as_skeletal", False)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", False)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    data = options.get_editor_property("static_mesh_import_data")
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    # Lumen lights this project, so a baked lightmap UV set is dead weight.
    data.set_editor_property("generate_lightmap_u_vs", False)
    data.set_editor_property("remove_degenerates", True)
    data.set_editor_property("normal_import_method",
                             unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    try:
        data.set_editor_property("build_nanite", bool(nanite))
    except Exception:
        pass  # set on the asset below instead
    return options


def set_nanite(mesh, path):
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    for prop in ("fallback_percent_triangles", "keep_triangle_percent"):
        try:
            settings.set_editor_property(prop, 1.0)
        except Exception:
            pass
    mesh.set_editor_property("nanite_settings", settings)
    EAL.save_asset(path)


def make_instance(name, textures, tint=None, scalars=None):
    full = INSTANCE_DIR + "/" + name
    instance = unreal.load_asset(full)
    if not instance:
        instance = TOOLS.create_asset(name, INSTANCE_DIR, unreal.MaterialInstanceConstant,
                                      unreal.MaterialInstanceConstantFactoryNew())
    if not instance:
        REPORT.append("FAILED instance %s" % name)
        return None
    MEL.set_material_instance_parent(instance, unreal.load_asset(MASTER))
    # These packs carry their own authored UVs; the master's Erebus default tiles at 0.25.
    MEL.set_material_instance_scalar_parameter_value(instance, "UVTile", 1.0)
    for key, value in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(instance, key, value)
    if tint:
        MEL.set_material_instance_vector_parameter_value(
            instance, "BaseTint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    for param, texture in (textures or {}).items():
        if param != "_masks" and texture:
            MEL.set_material_instance_texture_parameter_value(instance, param, texture)
    for param, mask in (textures or {}).get("_masks", {}).items():
        MEL.set_material_instance_vector_parameter_value(instance, param, mask)
    MEL.update_material_instance(instance)
    EAL.save_asset(full)
    return instance


def assign_material(mesh, path, material):
    slots = mesh.get_editor_property("static_materials")
    rebuilt = []
    for index in range(len(slots)):
        slot = slots[index]
        slot.set_editor_property("material_interface", material)
        rebuilt.append(slot)
    if not rebuilt:
        slot = unreal.StaticMaterial()
        slot.set_editor_property("material_interface", material)
        slot.set_editor_property("material_slot_name", "Material")
        rebuilt.append(slot)
    mesh.set_editor_property("static_materials", rebuilt)
    EAL.save_asset(path)


# --- drivers ---------------------------------------------------------------------------
def import_pack(pack):
    dest = pack["dest"]
    texture_dir = dest + "/Textures"
    instances = {}
    for tag, entry in pack["sets"].items():
        maps = {}
        for param, key, kind in (("AlbedoTex", "albedo", "albedo"),
                                 ("NormalTex", "normal", "normal"),
                                 ("RoughTex", "rough", "mask")):
            source = os.path.join(SOURCE_ROOT, entry[key])
            if not os.path.isfile(source):
                REPORT.append("MISSING source texture %s" % entry[key])
                continue
            maps[param] = import_texture(source, texture_dir, kind)
        maps["_masks"] = {"RoughChannelMask": entry["rough_mask"],
                          "MetallicChannelMask": entry["metal_mask"]}
        instances[tag] = make_instance("MI_%s_%s" % (pack["name"], tag), maps,
                                       scalars={"GrimeAmount": 0.30, "WearAmount": 0.30})

    flat = None
    if any(tag is None for tag in pack["meshes"].values()):
        flat = make_instance("MI_%s_Base" % pack["name"], None,
                             tint=pack.get("flat_tint", (0.25, 0.25, 0.26)),
                             scalars=pack.get("flat_scalars", {"Roughness": 0.85}))

    count = 0
    for relative, tag in sorted(pack["meshes"].items()):
        source = relative if os.path.isabs(relative) else os.path.join(SOURCE_ROOT, relative)
        if not os.path.isfile(source):
            REPORT.append("MISSING source mesh %s" % relative)
            continue
        name = os.path.splitext(os.path.basename(relative))[0]
        if not name.startswith("SM_"):
            name = "SM_" + name
        mesh = _run_task(source, dest, name, static_mesh_options(pack["nanite"]))
        if not mesh:
            REPORT.append("FAILED mesh %s" % relative)
            continue
        material = instances.get(tag) if tag else flat
        if material:
            assign_material(mesh, dest + "/" + name, material)
        if pack["nanite"]:
            set_nanite(mesh, dest + "/" + name)
        count += 1
    REPORT.append("%s: %d meshes, %d material instances" % (pack["name"], count, len(instances)))


def import_sounds():
    source_dir = os.path.join(SOURCE_ROOT, SOUND_PACK["source"])
    if not os.path.isdir(source_dir):
        REPORT.append("MISSING sound pack %s" % SOUND_PACK["source"])
        return
    count = 0
    for filename in sorted(os.listdir(source_dir)):
        # .mp3 as well: the sprint footstep loop ships as one, and UE5 imports it natively.
        if not filename.lower().endswith((".wav", ".mp3")):
            continue
        stem = os.path.splitext(filename)[0]
        name = "SW_" + "".join(part.capitalize() for part in stem.replace("-", " ").split())
        wave = _run_task(os.path.join(source_dir, filename), SOUND_PACK["dest"], name)
        if not wave:
            REPORT.append("FAILED sound %s" % filename)
            continue
        # Library clips are one-shots played on demand; keep them out of the always-loaded set.
        wave.set_editor_property("loading_behavior", unreal.SoundWaveLoadingBehavior.LOAD_ON_DEMAND)
        EAL.save_asset(SOUND_PACK["dest"] + "/" + name)
        count += 1
    REPORT.append("Sounds: %d clips" % count)


def main():
    if not os.path.isdir(SOURCE_ROOT):
        unreal.log_error("[AssetDrop] source not found: %s" % SOURCE_ROOT)
        return
    if not EAL.does_asset_exist(MASTER):
        unreal.log_error("[AssetDrop] %s is missing; run AuthorErebusPBRMasters.py first" % MASTER)
        return
    # Sound-only reruns are the common case once the meshes are in: re-importing every mesh
    # pack to pick up one new clip costs minutes and re-touches assets git then reports dirty.
    if not os.environ.get("AH_ASSET_DROP_SOUNDS_ONLY"):
        for pack in PACKS:
            import_pack(pack())
    import_sounds()
    # unreal.log is not reliably visible in commandlet stdout; the report goes to a file.
    report_path = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()),
        "AssetDropReport.txt")
    with open(report_path, "w") as handle:
        handle.write("\n".join(REPORT) + "\n")
    for line in REPORT:
        unreal.log("[AssetDrop] " + line)
    failures = [line for line in REPORT if line.startswith(("MISSING", "FAILED"))]
    if failures:
        unreal.log_error("[AssetDrop] %d problems" % len(failures))


main()
