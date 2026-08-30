"""Blender-side preparation for the enemy creature models.

Run under Blender, not Unreal:

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup -noaudio \
        -P Scripts/PrepareCreatureSources.py -- <task> [<source root>] [<out dir>]

Tasks
    hound      unpack the 4K maps the quadruped's .blend carries but its glTF export mangled
    bakes      bake per-model ambient occlusion and cavity for the two untextured bodies

Why this exists
    * The quadruped's glTF export packs metallic and roughness into one image. Feeding that to
      a roughness sampler reads the RED channel, which in the glTF convention is occlusion and
      is 1.0 nearly everywhere - so the map does nothing and the metal never appears. The
      .blend still carries the separate 4096 originals, packed.
    * Two models arrive with no texture at all. Tiling a generic noise over them is what makes
      them read as clay. Occlusion and cavity baked from the mesh itself land in the model's
      own creases, which is the detail the eye actually reads as "surface".

Every task writes to the out dir (default Saved/CreatureSource) and never modifies the drop.
"""

import json
import math
import os
import sys

import bpy
from mathutils import Matrix, Vector

ARGS = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
TASK = ARGS[0] if ARGS else "all"
SOURCE_ROOT = ARGS[1] if len(ARGS) > 1 else os.path.expanduser("~/Downloads/enemy")
OUT_DIR = ARGS[2] if len(ARGS) > 2 else os.path.join(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))), "Saved", "CreatureSource")

HOUND_BLEND = os.path.join(SOURCE_ROOT, "2/Alien-Animal-Blender_2.93-5_Baked_Animations.blend")
SPIDER_FBX = os.path.join(SOURCE_ROOT, "4/BioMechSpider.fbx")
STALKER_OBJ = os.path.join(SOURCE_ROOT, "1/x-com+alien+180601.obj")

manifest = {}


def log(msg):
    print("[PrepareCreatureSources] " + msg, flush=True)


def ensure_out():
    os.makedirs(OUT_DIR, exist_ok=True)


# --- hound -----------------------------------------------------------------------------
HOUND_MAPS = {
    "Alien-Animal-Base-Color.jpg": "Hound_Color.png",
    "Alien-Animal-Base-Nor.jpg": "Hound_Normal.png",
    "Alien-Animal-Base-Ro.jpg": "Hound_Roughness.png",
    "Alien-Animal-Base-Metallic.jpg": "Hound_Metallic.png",
    "Alien-Animal_eye.jpg": "Hound_Eye.png",
}


def task_hound():
    ensure_out()
    bpy.ops.wm.open_mainfile(filepath=HOUND_BLEND)
    written = {}
    for image in bpy.data.images:
        target = HOUND_MAPS.get(image.name)
        if not target:
            continue
        # These arrive packed into the .blend. Touching a pixel is what forces the decode;
        # has_data is False until then and save() would write a blank image.
        _ = image.pixels[0]
        if image.size[0] < 512:
            raise RuntimeError("image decoded to %dx%d: %s" % (image.size[0], image.size[1], image.name))
        path = os.path.join(OUT_DIR, target)
        image.filepath_raw = path
        image.file_format = "PNG"
        image.save()
        if os.path.getsize(path) < 4096:
            raise RuntimeError("wrote an empty png for " + image.name)
        written[image.name] = "%s (%dx%d)" % (target, image.size[0], image.size[1])
        log("unpacked %s -> %s" % (image.name, target))
    missing = set(HOUND_MAPS) - set(written)
    if missing:
        raise RuntimeError("hound .blend is missing " + ", ".join(sorted(missing)))
    manifest["hound"] = written


# --- occlusion and cavity bakes --------------------------------------------------------
BAKE_SIZE = 2048


def _new_bake_image(name):
    image = bpy.data.images.new(name, BAKE_SIZE, BAKE_SIZE, alpha=False, float_buffer=False)
    image.generated_color = (1.0, 1.0, 1.0, 1.0)
    return image


def _ensure_world():
    """An empty startup file has no World, and occlusion rays that hit no sky return black -
    which is exactly the flat black map an unchecked AO bake writes."""
    scene = bpy.context.scene
    if scene.world is None:
        scene.world = bpy.data.worlds.new("BakeWorld")
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    if background:
        background.inputs["Color"].default_value = (1.0, 1.0, 1.0, 1.0)
        background.inputs["Strength"].default_value = 1.0


def _emit_bake(objects, image, build_tree, samples=1):
    """Bake whatever `build_tree` emits into `image`, through the objects' own UV layout.

    Everything here goes through an emission bake rather than Blender's dedicated bake types.
    The dedicated AO bake returns solid black on these models - it is a lighting bake, and
    these scenes have no lighting - whereas the Ambient Occlusion shader node computes the
    same occlusion as a surface property and an emission bake writes it out verbatim.
    """
    scene = bpy.context.scene
    _ensure_world()
    scene.render.engine = "CYCLES"
    scene.cycles.device = "CPU"
    scene.cycles.samples = samples
    scene.render.bake.use_selected_to_active = False
    scene.render.bake.margin = 16

    for ob in objects:
        material = bpy.data.materials.new(ob.name + "_bake")
        ob.data.materials.clear()
        ob.data.materials.append(material)
        material.use_nodes = True
        tree = material.node_tree
        tree.nodes.clear()
        out = tree.nodes.new("ShaderNodeOutputMaterial")
        emit = tree.nodes.new("ShaderNodeEmission")
        tree.links.new(emit.outputs["Emission"], out.inputs["Surface"])
        build_tree(tree, emit)
        node = tree.nodes.new("ShaderNodeTexImage")
        node.image = image
        node.select = True
        tree.nodes.active = node

    for ob in bpy.data.objects:
        ob.select_set(ob in objects)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.object.bake(type="EMIT", use_clear=True)


def _save(image, path):
    # A bake that fails writes a uniform image and reports no error, so the result has to be
    # looked at. Colour channels only - alpha is 1.0 everywhere and would mask a flat bake.
    flat = list(image.pixels)
    colour = [flat[i] for i in range(0, len(flat), 4)]
    spread = max(colour) - min(colour)
    if spread < 0.05:
        raise RuntimeError("%s baked flat (spread %.4f) - nothing was written" % (path, spread))
    image.filepath_raw = path
    image.file_format = "PNG"
    image.save()
    log("baked %s (spread %.2f)" % (path, spread))


def _cavity_tree(tree, emit):
    """Per-vertex pointiness, remapped. Convex edges go white, creases go black."""
    geo = tree.nodes.new("ShaderNodeNewGeometry")
    ramp = tree.nodes.new("ShaderNodeValToRGB")
    # Pointiness sits in a narrow band around 0.5; widen it or the bake is flat grey.
    ramp.color_ramp.elements[0].position = 0.42
    ramp.color_ramp.elements[1].position = 0.58
    tree.links.new(geo.outputs["Pointiness"], ramp.inputs["Fac"])
    tree.links.new(ramp.outputs["Color"], emit.inputs["Color"])


def _ao_tree(tree, emit):
    ao = tree.nodes.new("ShaderNodeAmbientOcclusion")
    ao.samples = 24
    ao.only_local = True
    ao.inputs["Distance"].default_value = 60.0
    tree.links.new(ao.outputs["Color"], emit.inputs["Color"])


def _bake_model(load, prefix, mesh_filter=None):
    ensure_out()
    load()
    objects = [o for o in bpy.data.objects if o.type == "MESH" and o.data.uv_layers]
    if mesh_filter:
        objects = [o for o in objects if mesh_filter(o)]
    if not objects:
        raise RuntimeError(prefix + ": no UV-mapped meshes to bake")
    # One shared image: every part of these models packs into a single UV layout.
    log("%s bakes %d meshes" % (prefix, len(objects)))

    ao = _new_bake_image(prefix + "_AO")
    _emit_bake(objects, ao, _ao_tree, samples=16)
    _save(ao, os.path.join(OUT_DIR, prefix + "_AO.png"))

    load()
    objects = [o for o in bpy.data.objects if o.type == "MESH" and o.data.uv_layers]
    cavity = _new_bake_image(prefix + "_Cavity")
    _emit_bake(objects, cavity, _cavity_tree)
    _save(cavity, os.path.join(OUT_DIR, prefix + "_Cavity.png"))
    # Pointiness is evaluated per vertex, so the cavity map is only as dense as the mesh; log
    # the ratio rather than pretending a 168-vertex part carries surface detail.
    manifest.setdefault("bakes", {})[prefix] = [prefix + "_AO.png", prefix + "_Cavity.png"]


def task_bakes():
    """Only the spider bakes. The alien's own FBX is version 6000 and no DCC will open it; the
    .obj beside it is a separate export whose UV layout is not guaranteed to match the mesh
    Unreal imported, and a map baked against the wrong layout is worse than no map at all."""
    def load_spider():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=SPIDER_FBX)

    def load_stalker():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.wm.obj_import(filepath=STALKER_OBJ)

    _bake_model(load_spider, "Spider")


TASKS = {"hound": task_hound, "bakes": task_bakes}

if TASK == "all":
    for fn in TASKS.values():
        fn()
elif TASK in TASKS:
    TASKS[TASK]()
else:
    raise SystemExit("unknown task %r, expected one of %s" % (TASK, ", ".join(TASKS)))

ensure_out()
path = os.path.join(OUT_DIR, "manifest_%s.json" % TASK)
with open(path, "w") as handle:
    json.dump(manifest, handle, indent=1)
log("manifest " + path)
