"""Blender-side preparation for the enemy creature models.

Run under Blender, not Unreal:

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup -noaudio \
        -P Scripts/PrepareCreatureSources.py -- <task> [<source root>] [<out dir>]

Tasks
    hound      unpack the 4K maps the quadruped's .blend carries but its glTF export mangled

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


TASKS = {"hound": task_hound}

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
