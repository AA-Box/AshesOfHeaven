"""Import the external animation packs in ~/Downloads/new onto the project mannequin.

Five packs, ~1826 clips: a 1759-file pistol/rifle locomotion set, a falling/rolling set,
a "look through window" mocap set, 30 dead-body poses, and the VanillaLoop sample set
(cover, DBNO, ladder, stairs, push, lift, mining, roll, flashlight and locker clips, plus
three skeletal interactables), which arrives as a .unitypackage and is unpacked here. All four were authored on the
UE5 mannequin hierarchy (root / pelvis / spine_05 / ik_foot_root / clavicle_l).

They import onto the UEFN mannequin the locomotion pack ships, NOT onto the project's own
/Game/Characters/Mannequins/Meshes/SK_Mannequin: the locomotion clips carry an extra
"attach" bone track, and importing them against a skeleton without that bone aborts the
whole run with "Unable to retrieve bone index for track: attach". The UEFN hierarchy is a
superset, so every pack lands on it; retargeting to another body is an IK Retargeter step
on top, not part of the import.

Run with UnrealEditor-Cmd; idempotent, re-running rebuilds in place. Source files are read
from AH_ASSET_DROP (default ~/Downloads/new) and are never modified. The dead-body pack ships
as a zip and is extracted to Saved/DeadBodySource on first run.

The imported animations are gitignored - this script is how they come back.
"""

import os
import posixpath
import shutil
import tarfile
import zipfile

import unreal


SOURCE_ROOT = os.environ.get("AH_ASSET_DROP", os.path.expanduser("~/Downloads/new"))


def _contained(root, candidate):
    """True only if `candidate` really lands inside `root`.

    realpath first: a symlink already on disk inside the destination would otherwise let a
    later member escape through it.
    """
    root = os.path.realpath(root)
    target = os.path.realpath(candidate)
    return target == root or target.startswith(root + os.sep)


def saved_path(*parts):
    """Join a path under the project's Saved directory.

    Every write this script makes outside the content browser goes through here, so this is the
    one place the destination is checked. `unreal` only exists inside the editor; stub it - which
    is exactly what the tests do to import this module - and unreal.Paths returns a mock that
    os.path.join happily accepts through __fspath__. The result is a literal
    "MagicMock/mock.Paths.convert_relative_path_to_full()" directory in the working directory,
    which is where a 193MB animation drop unpacked, three times, before anyone noticed.
    """
    root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
    if not isinstance(root, str) or not os.path.isabs(root):
        raise RuntimeError(
            "the project Saved directory resolved to %r, which is not an absolute path. "
            "This script writes only from inside a running editor." % (root,))
    return os.path.join(root, *parts)


def _safe_join(root, relative):
    """Join an archive-supplied relative path to root, or None if it tries to escape.

    Archive member names are attacker-controlled data. An entry called `../../evil` or
    `/etc/evil` writes outside the destination on a plain extractall - CodeQL calls this
    "Arbitrary file write during tarfile extraction" and it is just as true of zips and of
    the paths these packages carry inside their own `pathname` files.
    """
    relative = relative.replace("\\", "/").strip()
    if not relative or relative.startswith("/") or posixpath.isabs(relative):
        return None
    if os.path.splitdrive(relative)[0]:
        return None
    if any(part in ("..", "") for part in relative.split("/")[:-1] if part != "."):
        return None
    if ".." in relative.split("/"):
        return None
    candidate = os.path.join(root, *[p for p in relative.split("/") if p not in (".",)])
    return candidate if _contained(root, candidate) else None


def extract_zip(archive, root):
    """Extract regular files only, each to a validated path."""
    extracted = skipped = 0
    with zipfile.ZipFile(archive) as handle:
        for info in handle.infolist():
            if info.is_dir():
                continue
            destination = _safe_join(root, info.filename)
            if destination is None:
                skipped += 1
                continue
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            with handle.open(info) as source, open(destination, "wb") as sink:
                shutil.copyfileobj(source, sink)
            extracted += 1
    if skipped:
        REPORT.append("REJECTED %d unsafe member(s) in %s" % (skipped, os.path.basename(archive)))
    return extracted


def extract_tar_gz(archive, root):
    """Extract regular files only. Symlinks, hardlinks and devices are never written."""
    extracted = skipped = 0
    with tarfile.open(archive, "r:gz") as handle:
        for member in handle.getmembers():
            if not member.isfile():
                if not member.isdir():
                    skipped += 1
                continue
            destination = _safe_join(root, member.name)
            if destination is None:
                skipped += 1
                continue
            source = handle.extractfile(member)
            if source is None:
                skipped += 1
                continue
            os.makedirs(os.path.dirname(destination), exist_ok=True)
            with source, open(destination, "wb") as sink:
                shutil.copyfileobj(source, sink)
            extracted += 1
    if skipped:
        REPORT.append("REJECTED %d unsafe member(s) in %s" % (skipped, os.path.basename(archive)))
    return extracted
ANIM_ROOT = "/Game/Ashes/Animations"
CHARACTER_DIR = "/Game/Ashes/Characters/UEFNMannequin"
SOURCE_MESH = ("Pistol and Rifle Locomotion Animations 1700/GaspFix/Characters/"
               "UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.FBX")

TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary
REPORT = []

# Importing 1800 clips one task at a time spends most of its time in per-task setup.
BATCH = 40
# A full re-import of the set costs ~25 minutes, so an existing clip is left alone unless
# AH_ANIM_FORCE is set.
FORCE = bool(os.environ.get("AH_ANIM_FORCE"))


def extracted_dead_bodies():
    """The dead-body pack only ships a zip; unpack it next to the other sources."""
    archive = os.path.join(
        SOURCE_ROOT, "Dead Bodies Sitting & Lying Poses – Mocap Pack",
        "dead_bodies_sitting_lying_poses_mocap_pack_fbx.zip")
    if not os.path.isfile(archive):
        REPORT.append("MISSING dead body archive")
        return None
    target = saved_path("DeadBodySource")
    if not os.path.isdir(target) or not os.listdir(target):
        os.makedirs(target, exist_ok=True)
        extract_zip(archive, target)
    return target


def import_skeleton():
    """Import the pack's own mannequin; its skeleton is what every clip binds to."""
    source = os.path.join(SOURCE_ROOT, SOURCE_MESH)
    if not os.path.isfile(source):
        REPORT.append("MISSING " + SOURCE_MESH)
        return None
    name = "SKM_UEFN_Mannequin"
    path = CHARACTER_DIR + "/" + name
    if not EAL.does_asset_exist(path):
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", True)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("import_animations", False)
        options.set_editor_property("create_physics_asset", True)
        options.set_editor_property("mesh_type_to_import",
                                    unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        data = options.get_editor_property("skeletal_mesh_import_data")
        data.set_editor_property("convert_scene", True)
        data.set_editor_property("convert_scene_unit", True)
        data.set_editor_property("normal_import_method",
                                 unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = CHARACTER_DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.set_editor_property("options", options)
        TOOLS.import_asset_tasks([task])
    mesh = unreal.load_asset(path)
    if not mesh:
        REPORT.append("FAILED " + name)
        return None
    return mesh.get_editor_property("skeleton")


def extracted_unity_pack():
    """The VanillaLoop sample ships as a .unitypackage: a gzipped tar of hash folders, each
    holding the raw asset next to a `pathname` file naming it. Unpack it into a normal tree."""
    archive = os.path.join(SOURCE_ROOT, "freesampleanimationset.unitypackage")
    if not os.path.isfile(archive):
        REPORT.append("MISSING unitypackage")
        return None
    target = saved_path("UnityDropSource")
    if os.path.isdir(target) and os.listdir(target):
        return target
    staging = target + "_raw"
    shutil.rmtree(staging, ignore_errors=True)
    os.makedirs(staging, exist_ok=True)
    extract_tar_gz(archive, staging)
    for entry in os.listdir(staging):
        name_file = os.path.join(staging, entry, "pathname")
        asset_file = os.path.join(staging, entry, "asset")
        if not (os.path.isfile(name_file) and os.path.isfile(asset_file)):
            continue
        with open(name_file) as handle:
            relative = handle.read().splitlines()[0].strip()
        if not relative.startswith("Assets/"):
            continue
        # The path comes out of the archive too, so it gets the same treatment as a member name.
        destination = _safe_join(target, relative[len("Assets/"):])
        if destination is None:
            REPORT.append("REJECTED unsafe pathname %r" % relative)
            continue
        os.makedirs(os.path.dirname(destination), exist_ok=True)
        shutil.copy2(asset_file, destination)
    shutil.rmtree(staging, ignore_errors=True)
    return target


def pack_unity_animations(root):
    """Mannequin clips only; the locker/flashlight meshes are handled separately."""
    base = os.path.join(root, "VanillaLoopStudio", "FreeSampleAnimationSet", "Art", "Animations")
    entries = []
    for directory, _, names in os.walk(base):
        for name in sorted(names):
            if not name.lower().endswith(".fbx"):
                continue
            group = os.path.relpath(directory, base).split(os.sep)[0]
            entries.append((os.path.join(directory, name),
                            "%s/VanillaLoop/%s" % (ANIM_ROOT, group)))
    return entries


# Skeletal props that carry their own skeleton and their own open/close clips.
UNITY_PROPS = (
    ("Meshes/StorageUnitsSet/LockerSingleKnob/100cm/Mesh/SKM_LockerSingleKnob_100cm.fbx",
     "Meshes/StorageUnitsSet/LockerSingleKnob/100cm/Animations/RightHand"),
    ("Meshes/StorageUnitsSet/SchoolLockerFullSize/Mesh/SKM_SchoolLockerFullSize_R.fbx",
     "Meshes/StorageUnitsSet/SchoolLockerFullSize/Animations/RightHand"),
    ("Meshes/SurvivalSet/SK_Flashlight.fbx", None),
)
PROP_DIR = "/Game/Ashes/Props/Interactables"


def import_unity_props(root):
    base = os.path.join(root, "VanillaLoopStudio", "FreeSampleAnimationSet", "Art")
    imported = 0
    for mesh_relative, anim_relative in UNITY_PROPS:
        source = os.path.join(base, mesh_relative)
        if not os.path.isfile(source):
            REPORT.append("MISSING " + mesh_relative)
            continue
        name = os.path.splitext(os.path.basename(source))[0]
        options = unreal.FbxImportUI()
        options.set_editor_property("import_mesh", True)
        options.set_editor_property("import_as_skeletal", True)
        options.set_editor_property("import_materials", False)
        options.set_editor_property("import_textures", False)
        options.set_editor_property("import_animations", False)
        options.set_editor_property("create_physics_asset", True)
        options.set_editor_property("mesh_type_to_import",
                                    unreal.FBXImportType.FBXIT_SKELETAL_MESH)
        task = unreal.AssetImportTask()
        task.filename = source
        task.destination_path = PROP_DIR
        task.destination_name = name
        task.automated = True
        task.replace_existing = True
        task.save = True
        task.set_editor_property("options", options)
        TOOLS.import_asset_tasks([task])
        mesh = unreal.load_asset(PROP_DIR + "/" + name)
        if not mesh:
            REPORT.append("FAILED " + name)
            continue
        imported += 1
        if not anim_relative:
            continue
        anim_dir = os.path.join(base, anim_relative)
        clips = [(os.path.join(anim_dir, clip), PROP_DIR + "/" + name + "_Anim")
                 for clip in sorted(os.listdir(anim_dir)) if clip.lower().endswith(".fbx")]
        imported += import_entries(clips, mesh.get_editor_property("skeleton"))
    REPORT.append("Unity props: %d assets" % imported)


def pack_locomotion(kind):
    """kind: 'Rifle' or 'Pistol'. Keeps the pack's own Idle/Walk/Run/... grouping."""
    root = os.path.join(SOURCE_ROOT, "Pistol and Rifle Locomotion Animations 1700",
                        "GaspFix", "_Fixed" + kind)
    entries = []
    if not os.path.isdir(root):
        REPORT.append("MISSING locomotion pack " + kind)
        return entries
    for group in sorted(os.listdir(root)):
        group_dir = os.path.join(root, group)
        if not os.path.isdir(group_dir):
            continue
        for name in sorted(os.listdir(group_dir)):
            if name.lower().endswith(".fbx"):
                entries.append((os.path.join(group_dir, name),
                                "%s/Locomotion/%s/%s" % (ANIM_ROOT, kind, group)))
    return entries


def pack_flat(relative, dest, extension=".fbx"):
    root = os.path.join(SOURCE_ROOT, relative) if not os.path.isabs(relative) else relative
    if not os.path.isdir(root):
        REPORT.append("MISSING pack " + relative)
        return []
    return [(os.path.join(root, name), dest)
            for name in sorted(os.listdir(root)) if name.lower().endswith(extension)]


def animation_options(skeleton):
    options = unreal.FbxImportUI()
    options.set_editor_property("import_mesh", False)
    options.set_editor_property("import_as_skeletal", True)
    options.set_editor_property("import_materials", False)
    options.set_editor_property("import_textures", False)
    options.set_editor_property("import_animations", True)
    options.set_editor_property("skeleton", skeleton)
    options.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_ANIMATION)
    data = options.get_editor_property("anim_sequence_import_data")
    data.set_editor_property("import_bone_tracks", True)
    data.set_editor_property("remove_redundant_keys", True)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("convert_scene_unit", True)
    return options


def asset_name(path):
    stem = os.path.splitext(os.path.basename(path))[0]
    return stem if stem.startswith("A_") else "A_" + stem


def import_entries(entries, skeleton):
    imported = 0
    if not FORCE:
        present = [entry for entry in entries
                   if EAL.does_asset_exist(entry[1] + "/" + asset_name(entry[0]))]
        imported += len(present)
        entries = [entry for entry in entries if entry not in present]
    for start in range(0, len(entries), BATCH):
        tasks = []
        for source, destination in entries[start:start + BATCH]:
            task = unreal.AssetImportTask()
            task.filename = source
            task.destination_path = destination
            task.destination_name = asset_name(source)
            task.automated = True
            task.replace_existing = True
            task.save = True
            task.set_editor_property("options", animation_options(skeleton))
            tasks.append(task)
        try:
            TOOLS.import_asset_tasks(tasks)
        except Exception as error:
            # One unusable clip raises for the whole batch, so retry the batch one file at
            # a time and let only the genuinely bad clips fall out.
            REPORT.append("BATCH retry (%s)" % str(error).splitlines()[0][:120])
            for task in tasks:
                try:
                    TOOLS.import_asset_tasks([task])
                except Exception as single:
                    # A dropped bone track ("Unable to retrieve bone index for track: thigh")
                    # raises but still writes a usable clip, so only a missing asset counts
                    # as a failure - the existence check below decides.
                    if not EAL.does_asset_exist(task.destination_path + "/" + task.destination_name):
                        REPORT.append("FAILED %s: %s" % (os.path.basename(task.filename),
                                                         str(single).splitlines()[0][:120]))
        for source, destination in entries[start:start + BATCH]:
            if EAL.does_asset_exist(destination + "/" + asset_name(source)):
                imported += 1
            else:
                REPORT.append("FAILED " + os.path.basename(source))
    return imported


def main():
    skeleton = import_skeleton()
    if not skeleton:
        unreal.log_error("[AnimDrop] could not import the UEFN mannequin skeleton")
        return

    groups = [
        ("Rifle locomotion", pack_locomotion("Rifle")),
        ("Pistol locomotion", pack_locomotion("Pistol")),
        ("Traversal", pack_flat("Dynamic Falling & Rolling Animation Pack - Free Asset/sourse",
                                ANIM_ROOT + "/Traversal")),
        ("Look window", pack_flat("lookwindow01_fbx/Animations", ANIM_ROOT + "/Mocap/LookWindow")),
    ]
    dead = extracted_dead_bodies()
    if dead:
        groups.append(("Dead body poses", pack_flat(dead, ANIM_ROOT + "/DeadBodies")))
    unity = extracted_unity_pack()
    if unity:
        groups.append(("VanillaLoop sample set", pack_unity_animations(unity)))

    for name, entries in groups:
        if not entries:
            continue
        count = import_entries(entries, skeleton)
        REPORT.append("%s: %d/%d clips" % (name, count, len(entries)))

    if unity:
        import_unity_props(unity)

    report_path = saved_path("AnimationDropReport.txt")
    with open(report_path, "w") as handle:
        handle.write("\n".join(REPORT) + "\n")
    failures = [line for line in REPORT if line.startswith(("MISSING", "FAILED"))]
    if failures:
        unreal.log_error("[AnimDrop] %d problems, see %s" % (len(failures), report_path))


main()
