"""Blender-side preparation for the enemy creature models.

Run under Blender, not Unreal:

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup -noaudio \
        -P Scripts/PrepareCreatureSources.py -- <task> [<source root>] [<out dir>]

Tasks
    ravager    re-export the armoured figure as a SKINNED mesh plus locomotion takes
    hound      unpack the 4K maps the quadruped's .blend carries but its glTF export mangled
    bakes      bake per-model ambient occlusion and cavity for the two untextured bodies

Why this exists
    * The armoured figure ships two FBX variants and the one next to the textures has no skin
      cluster at all - Unreal imports it as sixteen rigid parts named after the mesh pieces
      ("claws_hip_001", "LEATHER_002"), which is a pile of armour plates in bind position, not
      a figure. The .blend beside it holds the same model on a Rigify rig plus a hand-keyed
      walk cycle, so that is the real source.
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

RAVAGER_BLEND = os.path.join(SOURCE_ROOT, "3/BLEND+(Model+++animation)/personaje  modelo.blend")
HOUND_BLEND = os.path.join(SOURCE_ROOT, "2/Alien-Animal-Blender_2.93-5_Baked_Animations.blend")
SPIDER_FBX = os.path.join(SOURCE_ROOT, "4/BioMechSpider.fbx")
STALKER_OBJ = os.path.join(SOURCE_ROOT, "1/x-com+alien+180601.obj")

# The walk take in the .blend. The second take there is a fall-land-walk and has no usable
# standing hold in it, so the idle is authored instead.
RAVAGER_WALK = ("rigAction", 1, 25)

FPS = 30
manifest = {}


def log(msg):
    print("[PrepareCreatureSources] " + msg, flush=True)


def ensure_out():
    os.makedirs(OUT_DIR, exist_ok=True)


# --- action helpers --------------------------------------------------------------------
def action_fcurves(action):
    """Blender 4.4+ moved f-curves behind layers/strips/channelbags."""
    direct = getattr(action, "fcurves", None)
    if direct is not None:
        return list(direct)
    out = []
    for layer in getattr(action, "layers", []):
        for strip in getattr(layer, "strips", []):
            for bag in getattr(strip, "channelbags", []):
                out.extend(bag.fcurves)
    return out


def assign_action(obj, action):
    obj.animation_data_create()
    obj.animation_data.action = action
    slots = getattr(action, "slots", None)
    if slots:
        # Slotted actions bind to nothing by default, so the pose never moves.
        obj.animation_data.action_slot = slots[0]


def bake_pose(rig, start, end, name):
    """Bake the evaluated pose - IK, constraints and all - into a plain action.

    visual_keying is the whole point: the walk take drives foot IK targets, and the deform
    bones only follow through constraints that the FBX exporter would otherwise not resolve.
    """
    bpy.context.view_layer.objects.active = rig
    rig.select_set(True)
    bpy.ops.object.mode_set(mode="POSE")
    bpy.ops.pose.select_all(action="SELECT")
    bpy.ops.nla.bake(frame_start=start, frame_end=end, only_selected=False,
                     visual_keying=True, clear_constraints=True, clear_parents=False,
                     use_current_action=False, bake_types={"POSE"})
    bpy.ops.object.mode_set(mode="OBJECT")
    baked = rig.animation_data.action
    baked.name = name
    return baked


def export_fbx(path, objects, bake_anim, start=1, end=1):
    for ob in bpy.data.objects:
        ob.select_set(ob in objects)
    bpy.context.view_layer.objects.active = objects[0]
    bpy.ops.export_scene.fbx(
        filepath=path,
        use_selection=True,
        apply_scale_options="FBX_SCALE_NONE",
        object_types={"ARMATURE", "MESH"},
        use_mesh_modifiers=True,
        add_leaf_bones=False,
        primary_bone_axis="Y",
        secondary_bone_axis="X",
        use_armature_deform_only=True,
        bake_anim=bake_anim,
        bake_anim_use_all_bones=True,
        bake_anim_use_nla_strips=False,
        bake_anim_use_all_actions=False,
        bake_anim_force_startend_keying=True,
        bake_anim_step=1.0,
        bake_anim_simplify_factor=0.0,
        path_mode="STRIP",
        axis_forward="-Z",
        axis_up="Y",
    )
    log("wrote " + path)


# --- ravager ---------------------------------------------------------------------------
def ravager_scene():
    bpy.ops.wm.open_mainfile(filepath=RAVAGER_BLEND)
    rig = bpy.data.objects.get("rig")
    if rig is None:
        raise RuntimeError("no 'rig' armature in " + RAVAGER_BLEND)

    # Keep the armature and only the meshes it actually deforms. The .blend carries a second,
    # unskinned copy of every part (the modelling originals) plus Rigify's widget meshes; both
    # would export as extra geometry standing beside the character.
    keep = {rig}
    for ob in bpy.data.objects:
        if ob.type != "MESH":
            continue
        if ob.name.startswith("WGT-") or ob.name == "WGTS_rig":
            continue
        if any(m.type == "ARMATURE" and m.object is rig for m in ob.modifiers) and ob.vertex_groups:
            keep.add(ob)
    for ob in list(bpy.data.objects):
        if ob not in keep:
            bpy.data.objects.remove(ob, do_unlink=True)

    # Geometry-nodes modifiers have to go before export. Five of these parts carry one, and the
    # geometry it generates has no vertex groups - the exporter writes it out unskinned, so it
    # stays at the bind position while the body walks away from it. In frame that is a floating
    # mass of quills hanging beside the creature.
    dropped = 0
    for ob in keep:
        for modifier in list(ob.modifiers):
            if modifier.type == "NODES":
                ob.modifiers.remove(modifier)
                dropped += 1
    if dropped:
        log("dropped %d geometry-nodes modifier(s) that would export unskinned" % dropped)

    meshes = sorted((o for o in keep if o.type == "MESH"), key=lambda o: o.name)
    if not meshes:
        raise RuntimeError("no skinned meshes survived the cull")
    log("ravager keeps %d skinned meshes, %d deform bones"
        % (len(meshes), sum(1 for b in rig.data.bones if b.use_deform)))
    return rig, meshes


def bone_world(rig, name):
    pb = rig.pose.bones[name]
    return (rig.matrix_world @ pb.matrix).translation.copy()


def strip_forward_drift(rig, action, start, end):
    """Cancel the walk take's travel so the clip loops in place.

    The take is hand-keyed with real forward motion; Unreal drives translation from the
    movement component, so a clip that also travels slides forward and snaps back every loop.
    Every control in a Rigify rig hangs off `root`, so counter-translating that one bone moves
    the feet and the body together and leaves the gait untouched.

    Two passes on purpose. Writing a key into the action while sampling it changes what the
    next frame evaluates to, so the whole path is measured first and only then written.
    """
    assign_action(rig, action)
    root = rig.pose.bones["root"]
    rest_basis = root.bone.matrix_local.to_3x3()

    torso = []
    root_local = []
    for f in range(start, end + 1):
        bpy.context.scene.frame_set(f)
        bpy.context.view_layer.update()
        torso.append(bone_world(rig, "torso"))
        root_local.append(root.location.copy())

    travel = torso[-1] - torso[0]
    travel.z = 0.0
    log("ravager walk travels %.3f units over %d frames" % (travel.length, end - start))
    if travel.length < 0.2:
        raise RuntimeError("walk take does not travel - wrong action?")

    span = float(end - start)
    to_armature = rig.matrix_world.to_3x3().inverted()
    basis_inverse = rest_basis.inverted()
    for index, f in enumerate(range(start, end + 1)):
        offset = to_armature @ (travel * (index / span))
        root.location = root_local[index] - (basis_inverse @ offset)
        root.keyframe_insert("location", frame=f, group="root")

    # The correction has to actually land, or the clip exports with its travel intact and the
    # creature moonwalks in game - a failure that only shows up once it is in a level.
    bpy.context.scene.frame_set(end)
    bpy.context.view_layer.update()
    residual = (bone_world(rig, "torso") - torso[0])
    residual.z = 0.0
    if residual.length > 0.05:
        raise RuntimeError("drift removal left %.3f units of travel" % residual.length)
    return travel.length / (span / FPS)


def author_ravager_attack(rig, start, end):
    """A two-beat overhead claw swing, keyed on the FK controls.

    Hand-keyed rather than borrowed: the .blend has no attack take, and this body's silhouette
    reads from the arms - a melee enemy that closes and then does nothing visible is the single
    worst thing about the current roster.
    """
    action = bpy.data.actions.new("AH_Ravager_Attack")
    assign_action(rig, action)
    bones = rig.pose.bones
    for pb in bones:
        pb.rotation_mode = "XYZ"

    # (frame, bone, euler radians) - wind up, strike, recover.
    keys = [
        (start, "upper_arm_fk.R", (0.0, 0.0, 0.0)),
        (start, "forearm_fk.R", (0.0, 0.0, 0.0)),
        (start, "chest", (0.0, 0.0, 0.0)),
        (start + 6, "upper_arm_fk.R", (-1.9, 0.0, 0.5)),
        (start + 6, "forearm_fk.R", (-1.1, 0.0, 0.0)),
        (start + 6, "chest", (0.0, 0.0, -0.35)),
        (start + 11, "upper_arm_fk.R", (1.35, 0.0, -0.35)),
        (start + 11, "forearm_fk.R", (0.25, 0.0, 0.0)),
        (start + 11, "chest", (0.12, 0.0, 0.45)),
        (start + 16, "upper_arm_fk.R", (0.35, 0.0, -0.1)),
        (start + 16, "forearm_fk.R", (0.55, 0.0, 0.0)),
        (start + 16, "chest", (0.0, 0.0, 0.12)),
        (end, "upper_arm_fk.R", (0.0, 0.0, 0.0)),
        (end, "forearm_fk.R", (0.0, 0.0, 0.0)),
        (end, "chest", (0.0, 0.0, 0.0)),
    ]
    for frame, bone, euler in keys:
        pb = bones.get(bone)
        if pb is None:
            raise RuntimeError("attack needs missing bone " + bone)
        pb.rotation_euler = euler
        pb.keyframe_insert("rotation_euler", frame=frame, group=bone)
    return action


def author_ravager_idle(rig, start, end):
    """A standing idle, keyed from the rest pose.

    Not taken from the second take in the .blend: that take is a fall, a landing and a walk-off,
    and the stretch of it that holds still holds a crouch - torso at 0.82 against the walk's
    1.17. Cutting an idle out of it gave a heavy that stands permanently braced for impact.
    """
    action = bpy.data.actions.new("AH_Ravager_Idle")
    assign_action(rig, action)
    bones = rig.pose.bones
    for pb in bones:
        pb.rotation_mode = "XYZ"

    span = float(end - start)
    for frame in range(start, end + 1):
        phase = (frame - start) / span
        breath = math.sin(2.0 * math.pi * phase)
        bones["chest"].rotation_euler = (0.035 * breath, 0.0, 0.0)
        bones["chest"].keyframe_insert("rotation_euler", frame=frame, group="chest")
        bones["torso"].rotation_euler = (0.0, 0.0, 0.045 * math.sin(math.pi * phase * 2.0 + 0.7))
        bones["torso"].keyframe_insert("rotation_euler", frame=frame, group="torso")
        bones["torso"].location = Vector((0.0, 0.0, 0.018 * breath))
        bones["torso"].keyframe_insert("location", frame=frame, group="torso")
    return action


def author_ravager_death(rig, start, end):
    """Buckle at the knees, fold forward, drop. Ends flat so the ragdoll has somewhere to go."""
    action = bpy.data.actions.new("AH_Ravager_Death")
    assign_action(rig, action)
    bones = rig.pose.bones
    for pb in bones:
        pb.rotation_mode = "XYZ"

    keys = [
        (start, "torso", (0.0, 0.0, 0.0)),
        (start, "chest", (0.0, 0.0, 0.0)),
        (start + 8, "torso", (0.35, 0.0, 0.0)),
        (start + 8, "chest", (-0.5, 0.0, 0.2)),
        (start + 18, "torso", (1.15, 0.0, 0.15)),
        (start + 18, "chest", (0.35, 0.0, 0.1)),
        (end, "torso", (1.45, 0.0, 0.2)),
        (end, "chest", (0.5, 0.0, 0.0)),
    ]
    for frame, bone, euler in keys:
        pb = bones[bone]
        pb.rotation_euler = euler
        pb.keyframe_insert("rotation_euler", frame=frame, group=bone)
    # Sink the body over the same span so the collapse reaches the ground.
    torso = bones["torso"]
    for frame, drop in ((start, 0.0), (start + 8, -0.25), (start + 18, -0.75), (end, -0.95)):
        bpy.context.scene.frame_set(frame)
        torso.location = Vector((0.0, 0.0, drop))
        torso.keyframe_insert("location", frame=frame, group="torso")
    return action


def task_ravager():
    ensure_out()
    results = {}

    # The mesh, with no animation on it. Imported first in Unreal so the takes have a skeleton.
    rig, meshes = ravager_scene()
    bpy.context.scene.frame_set(1)
    export_fbx(os.path.join(OUT_DIR, "Ravager_Mesh.fbx"), meshes + [rig], bake_anim=False)
    results["mesh"] = "Ravager_Mesh.fbx"

    # Each take gets a clean file opened from the .blend, because baking a Rigify rig has to
    # clear its constraints and a cleared rig cannot evaluate the next take.
    def take(name, build):
        rig, _meshes = ravager_scene()
        start, end, action = build(rig)
        baked = bake_pose(rig, start, end, "AH_" + name)
        bpy.context.scene.frame_start = start
        bpy.context.scene.frame_end = end
        assign_action(rig, baked)
        path = os.path.join(OUT_DIR, "Ravager_%s.fbx" % name)
        export_fbx(path, [rig], bake_anim=True, start=start, end=end)
        results[name.lower()] = os.path.basename(path)

    def build_walk(rig):
        act = bpy.data.actions[RAVAGER_WALK[0]]
        speed = strip_forward_drift(rig, act, RAVAGER_WALK[1], RAVAGER_WALK[2])
        results["walk_speed_units_per_second"] = round(speed, 3)
        return RAVAGER_WALK[1], RAVAGER_WALK[2], act

    def build_idle(rig):
        return 1, 60, author_ravager_idle(rig, 1, 60)

    def build_attack(rig):
        return 1, 24, author_ravager_attack(rig, 1, 24)

    def build_death(rig):
        return 1, 30, author_ravager_death(rig, 1, 30)

    take("Walk", build_walk)
    take("Idle", build_idle)
    take("Attack", build_attack)
    take("Death", build_death)
    manifest["ravager"] = results


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


TASKS = {"ravager": task_ravager, "hound": task_hound, "bakes": task_bakes}

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
