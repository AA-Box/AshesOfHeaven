"""Author locomotion and combat takes for the creature skeletons that shipped without any.

Run with UnrealEditor-Cmd; idempotent, so re-running rebuilds the clips in place.

Two of the four bodies arrive with no animation at all. The humanoid alien ships a single idle
take and the bio-mech spider ships none, which is why both stood in bind pose in every capture -
and the spider's bind pose is not even a stance: its eight legs were left wherever the modeller
last dragged them, one folded up under the body and one stretched flat out behind. A creature
that never moves reads as scenery, and a creature frozen mid-collapse reads as broken.

Everything here is written straight into UAnimSequence bone tracks through the animation data
controller rather than round-tripped through an FBX, because the alien's source FBX is version
6000 and no current DCC will open it. Working on the imported skeleton sidesteps the format
entirely and keeps the clips regenerable from this repo.

The spider gets a real inverse-kinematics pass. Keying its joint angles by hand would need one
guess per joint per leg per frame; solving for a foot position instead means the gait is
described where it is legible - "this foot is planted here, that one is reaching there" - and
the joint angles fall out of it.
"""

import json
import math
import os

import unreal

APE = unreal.AnimPoseExtensions
LOCAL = unreal.AnimPoseSpaces.LOCAL
COMPONENT = unreal.AnimPoseSpaces.WORLD
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()

ENEMY_ROOT = "/Game/Ashes/Enemies"
FRAME_RATE = 30
REPORT_PATH = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "CreatureAnimations.json"))

report = {}


def _log(message):
    unreal.log_warning("[CreatureAnims] " + message)


# --- small maths -----------------------------------------------------------------------
# Pure python so the composition order is visible. Quaternions are Hamilton convention here
# (q1 * q2 applies q2 first), which is the reverse of FQuat's operator*; every conversion to
# and from an unreal type goes through quat_to_ue / quat_from_ue so the flip happens once.
def v_add(a, b):
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def v_sub(a, b):
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def v_scale(a, s):
    return (a[0] * s, a[1] * s, a[2] * s)


def v_dot(a, b):
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def v_cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def v_len(a):
    return math.sqrt(v_dot(a, a))


def v_norm(a):
    n = v_len(a)
    return (0.0, 0.0, 0.0) if n < 1e-9 else v_scale(a, 1.0 / n)


def q_mul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (aw * bx + ax * bw + ay * bz - az * by,
            aw * by - ax * bz + ay * bw + az * bx,
            aw * bz + ax * by - ay * bx + az * bw,
            aw * bw - ax * bx - ay * by - az * bz)


def q_conj(q):
    return (-q[0], -q[1], -q[2], q[3])


def q_norm(q):
    n = math.sqrt(sum(c * c for c in q))
    return (0.0, 0.0, 0.0, 1.0) if n < 1e-9 else tuple(c / n for c in q)


def q_rotate(q, v):
    qv = (q[0], q[1], q[2])
    t = v_scale(v_cross(qv, v), 2.0)
    return v_add(v_add(v, v_scale(t, q[3])), v_cross(qv, t))


def q_axis_angle(axis, radians):
    axis = v_norm(axis)
    s = math.sin(radians * 0.5)
    return (axis[0] * s, axis[1] * s, axis[2] * s, math.cos(radians * 0.5))


def q_slerp(a, b, t):
    dot = sum(x * y for x, y in zip(a, b))
    if dot < 0.0:
        b = tuple(-c for c in b)
        dot = -dot
    if dot > 0.9995:
        return q_norm(tuple(x + (y - x) * t for x, y in zip(a, b)))
    theta = math.acos(max(-1.0, min(1.0, dot)))
    s = math.sin(theta)
    wa = math.sin((1.0 - t) * theta) / s
    wb = math.sin(t * theta) / s
    return q_norm(tuple(x * wa + y * wb for x, y in zip(a, b)))


def quat_from_ue(q):
    return q_norm((q.x, q.y, q.z, q.w))


def quat_to_ue(q):
    q = q_norm(q)
    return unreal.Quat(q[0], q[1], q[2], q[3])


def vec_from_ue(v):
    return (v.x, v.y, v.z)


# --- reference skeleton ----------------------------------------------------------------
class Rig(object):
    """The reference pose of one creature skeleton, plus the parent map derived from it.

    The parent map is recovered numerically instead of hard-coded: every bone's parent has a
    component transform of local^-1 * component, and matching that against the other bones is
    exact for a reference pose. Two of these skeletons name every bone "Bone_017", so a
    hand-written table would be unreviewable and would rot the moment a re-import renumbers.
    """

    def __init__(self, skeleton_path, mesh_path=None):
        self.skeleton = unreal.load_asset(skeleton_path)
        if not self.skeleton:
            raise RuntimeError("no skeleton at " + skeleton_path)
        self.mesh_path = mesh_path
        pose = APE.get_reference_pose(self.skeleton)
        self.order = [str(n) for n in APE.get_bone_names(pose)]
        self.local = {}
        self.comp = {}
        for name in self.order:
            l = APE.get_bone_pose(pose, name, LOCAL)
            c = APE.get_bone_pose(pose, name, COMPONENT)
            self.local[name] = (vec_from_ue(l.translation), quat_from_ue(l.rotation),
                                vec_from_ue(l.scale3d))
            self.comp[name] = (vec_from_ue(c.translation), quat_from_ue(c.rotation),
                               vec_from_ue(c.scale3d))
        self._chains = {}
        self.parent = self._parents_from_mesh() if mesh_path else self._derive_parents()
        self._verify_hierarchy()

    def _parents_from_mesh(self):
        """Read the real hierarchy off the skeletal mesh instead of inferring it.

        _derive_parents recovers the parent map numerically, and on a rig whose bones stack on the
        same head it can pick a sibling: it puts the crawler's tail_4 72 units from where it
        belongs, and the armoured figure's spine_006 258 units off. A transient component exposes
        the reference skeleton's actual parent for every bone, so where a mesh is available the
        map is read rather than guessed. _verify_hierarchy still checks the answer either way.
        """
        mesh = unreal.load_asset(self.mesh_path)
        if not mesh:
            raise RuntimeError("no skeletal mesh at " + self.mesh_path)
        component = unreal.SkeletalMeshComponent()
        component.set_skeletal_mesh_asset(mesh)
        parents = {}
        for name in self.order:
            parent = str(component.get_parent_bone(name))
            parents[name] = None if parent in ("None", "") else parent
        return parents

    def _derive_parents(self):
        """Recover each bone's parent from the reference pose.

        A bone's parent has component transform local^-1 * component, so the parent is whichever
        bone matches that. Nearest-match rather than a threshold: these skeletons stack four
        bones on the same head, and any tolerance loose enough to match through float noise is
        also loose enough to pick a sibling. _verify_hierarchy is what actually rejects a bad
        map, and it checks the answer rather than the guess.
        """
        parents = {}
        for index, name in enumerate(self.order):
            lt, lq, ls = self.local[name]
            ct, cq, cs = self.comp[name]
            # Scale is not decoration here: this alien's root bone carries a 2.54 inch-to-
            # centimetre conversion, so every descendant's local translation means 2.54x what
            # it says. Composing without it puts the tail tip 153 units from where it lives.
            pq = q_mul(cq, q_conj(lq))
            ps = tuple(cs[i] / ls[i] if abs(ls[i]) > 1e-6 else 1.0 for i in range(3))
            pt = v_sub(ct, q_rotate(pq, tuple(ps[i] * lt[i] for i in range(3))))
            if v_len(pt) < 1e-3 and abs(pq[3]) > 1.0 - 1e-6:
                parents[name] = None          # its parent is the identity: this is a root
                continue
            best, best_error = None, 1e30
            # Only bones earlier in the skeleton's own order are candidates. A parent always
            # has a lower bone index in Unreal, and restricting the search that way makes a
            # cycle impossible instead of merely unlikely - two adjacent finger joints an inch
            # apart otherwise pick each other and the walk up the chain never terminates.
            for other in self.order[:index]:
                ot, oq, _os = self.comp[other]
                error = v_len(v_sub(ot, pt))
                for axis in ((1.0, 0.0, 0.0), (0.0, 1.0, 0.0)):
                    error += 10.0 * v_len(v_sub(q_rotate(oq, axis), q_rotate(pq, axis)))
                if error < best_error:
                    best, best_error = other, error
            parents[name] = best
        return parents

    def _verify_hierarchy(self):
        """Re-compose every bone from its parents and compare against the reference pose.

        This is the check that matters. A wrong parent, a flipped quaternion order or a
        transposed rotation all produce a rig that still evaluates - it just puts the legs
        somewhere else - and the only symptom downstream is an inverse-kinematics solver that
        never converges, which is a long way from the cause.
        """
        worst_bone, worst = None, 0.0
        for name in self.order:
            got = self.component_of(name, self.local)[0]
            error = v_len(v_sub(got, self.comp[name][0]))
            if error > worst:
                worst_bone, worst = name, error
        if worst > 0.5:
            raise RuntimeError("parent map is wrong: %s recomposes %.2f units off" % (worst_bone, worst))
        roots = [n for n in self.order if self.parent[n] is None]
        if len(roots) != 1:
            raise RuntimeError("expected one root bone, derived %d: %s" % (len(roots), roots[:5]))

    def chain_to_root(self, name):
        """Bone, then its parents, up to the root. Cached: the inverse-kinematics pass asks for
        the same chains tens of thousands of times, and the bound stops a bad parent map from
        turning a cycle into a hang instead of an error."""
        cached = self._chains.get(name)
        if cached is not None:
            return cached
        out = []
        seen = set()
        while name is not None:
            if name in seen or len(out) > len(self.order):
                raise RuntimeError("bone %s sits in a parent cycle" % name)
            seen.add(name)
            out.append(name)
            name = self.parent.get(name)
        self._chains[name] = out
        self._chains[out[0]] = out
        return out

    def parent_component(self, name, pose_local):
        """Component rotation and accumulated scale of `name`'s parent under a pose."""
        rotation = (0.0, 0.0, 0.0, 1.0)
        scale = (1.0, 1.0, 1.0)
        for bone in reversed(self.chain_to_root(name)[1:]):
            _lt, lq, ls = pose_local.get(bone, self.local[bone])
            rotation = q_mul(rotation, lq)
            scale = (scale[0] * ls[0], scale[1] * ls[1], scale[2] * ls[2])
        return rotation, scale

    def parent_component_rotation(self, name, pose_local):
        """Component rotation of `name`'s parent under a pose, walking down from the root."""
        chain = self.chain_to_root(name)[1:]
        rotation = (0.0, 0.0, 0.0, 1.0)
        for bone in reversed(chain):
            rotation = q_mul(rotation, pose_local.get(bone, self.local[bone])[1])
        return rotation

    def component_of(self, name, pose_local):
        """Component transform (translation, rotation) of one bone under a pose."""
        chain = self.chain_to_root(name)
        translation = (0.0, 0.0, 0.0)
        rotation = (0.0, 0.0, 0.0, 1.0)
        scale = (1.0, 1.0, 1.0)
        for bone in reversed(chain):
            lt, lq, ls = pose_local.get(bone, self.local[bone])
            translation = v_add(translation,
                                q_rotate(rotation, (scale[0] * lt[0], scale[1] * lt[1], scale[2] * lt[2])))
            rotation = q_mul(rotation, lq)
            scale = (scale[0] * ls[0], scale[1] * ls[1], scale[2] * ls[2])
        return translation, rotation


def rest_pose(rig):
    return dict(rig.local)


def rotate_bone(rig, pose, bone, axis_component, radians):
    """Rotate `bone` about a component-space axis, on top of whatever the pose already holds."""
    if bone not in rig.local:
        raise RuntimeError("no bone named " + bone)
    lt, lq, ls = pose.get(bone, rig.local[bone])
    parent_rotation = rig.parent_component_rotation(bone, pose)
    delta = q_axis_angle(axis_component, radians)
    # Bring the component-space rotation into the bone's parent space, then pre-multiply.
    local_delta = q_mul(q_mul(q_conj(parent_rotation), delta), parent_rotation)
    pose[bone] = (lt, q_norm(q_mul(local_delta, lq)), ls)


def offset_bone(rig, pose, bone, component_translation):
    """Shift a bone by a component-space distance.

    The conversion matters. Two of these skeletons carry a unit conversion on the root bone -
    100 for the spider, 2.54 for the alien - so a local translation means a hundred times what
    it says, and "bob the body 12 units" moves it twelve metres.
    """
    lt, lq, ls = pose.get(bone, rig.local[bone])
    parent_rotation, parent_scale = rig.parent_component(bone, pose)
    local = q_rotate(q_conj(parent_rotation), component_translation)
    local = tuple(local[i] / parent_scale[i] if abs(parent_scale[i]) > 1e-9 else local[i]
                  for i in range(3))
    pose[bone] = (v_add(lt, local), lq, ls)


# --- inverse kinematics ----------------------------------------------------------------
def solve_chain(rig, pose, chain, tip_bone, target, iterations=40):
    """Cyclic coordinate descent: swing each joint so the tip closes on the target.

    Chosen over a closed-form two-bone solve because these legs are four segments long and the
    joints have no consistent axis - CCD needs neither, only the ability to evaluate the chain.
    """
    for _ in range(iterations):
        for bone in reversed(chain):
            tip = rig.component_of(tip_bone, pose)[0]
            pivot = rig.component_of(bone, pose)[0]
            to_tip = v_sub(tip, pivot)
            to_target = v_sub(target, pivot)
            if v_len(to_tip) < 1e-4 or v_len(to_target) < 1e-4:
                continue
            a = v_norm(to_tip)
            b = v_norm(to_target)
            axis = v_cross(a, b)
            if v_len(axis) < 1e-6:
                continue
            angle = math.acos(max(-1.0, min(1.0, v_dot(a, b))))
            # Small steps: a full swing per joint per pass makes the chain thrash and settle
            # into a knot instead of an arc.
            rotate_bone(rig, pose, bone, axis, angle * 0.6)
    return v_len(v_sub(rig.component_of(tip_bone, pose)[0], target))


# --- clip writing ----------------------------------------------------------------------
def write_clip(skeleton_path, package_path, frames, keyed_bones, rig):
    """Create or overwrite one UAnimSequence from a list of per-frame local-transform poses."""
    package_dir, name = package_path.rsplit("/", 1)
    if unreal.EditorAssetLibrary.does_asset_exist(package_path):
        unreal.EditorAssetLibrary.delete_asset(package_path)
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property("target_skeleton", unreal.load_asset(skeleton_path))
    sequence = TOOLS.create_asset(name, package_dir, unreal.AnimSequence, factory)
    if not sequence:
        raise RuntimeError("could not create " + package_path)

    controller = sequence.get_editor_property("controller")
    controller.open_bracket("author creature clip")
    controller.set_frame_rate(unreal.FrameRate(FRAME_RATE, 1))
    controller.set_number_of_frames(unreal.FrameNumber(len(frames) - 1))
    for bone in sorted(keyed_bones):
        controller.add_bone_track(bone)
        positions, rotations, scales = [], [], []
        for pose in frames:
            lt, lq, ls = pose.get(bone, rig.local[bone])
            positions.append(unreal.Vector(lt[0], lt[1], lt[2]))
            rotations.append(quat_to_ue(lq))
            scales.append(unreal.Vector(ls[0], ls[1], ls[2]))
        controller.set_bone_track_keys(bone, positions, rotations, scales)
    controller.close_bracket()

    # A track that silently failed to take leaves a clip that plays the bind pose and looks
    # exactly like "the animation did not import". Read the clip back and compare a real key
    # against what was written - that catches a dropped track, a wrong key count and a
    # mis-ordered quaternion, none of which raise on the way in.
    probe_frame = len(frames) // 2
    probe_bone = sorted(keyed_bones)[0]
    for bone in sorted(keyed_bones):
        written = frames[probe_frame].get(bone)
        if written and v_len(v_sub(written[1][:3], rig.local[bone][1][:3])) > 1e-3:
            probe_bone = bone
            break
    options = unreal.AnimPoseEvaluationOptions()
    read_pose = APE.get_anim_pose_at_frame(sequence, probe_frame, options)
    read_back = quat_from_ue(APE.get_bone_pose(read_pose, probe_bone, LOCAL).rotation)
    expected = frames[probe_frame].get(probe_bone, rig.local[probe_bone])[1]
    drift = min(v_len(v_sub(read_back[:3], expected[:3])),
                v_len(v_add(read_back[:3], expected[:3])))
    if drift > 0.02:
        raise RuntimeError("%s: %s reads back %s, wrote %s"
                           % (package_path, probe_bone,
                              [round(c, 3) for c in read_back], [round(c, 3) for c in expected]))
    if abs(sequence.get_play_length() - (len(frames) - 1) / float(FRAME_RATE)) > 0.01:
        raise RuntimeError("%s is %.3fs, expected %.3fs"
                           % (package_path, sequence.get_play_length(),
                              (len(frames) - 1) / float(FRAME_RATE)))
    unreal.EditorAssetLibrary.save_asset(package_path, only_if_is_dirty=False)
    _log("wrote %s (%d frames, %d bones, probe %s)"
         % (package_path, len(frames), len(keyed_bones), probe_bone))
    return package_path


# --- stalker ---------------------------------------------------------------------------
# 3ds Max Biped naming, and the body faces +Y in its own space (the toes sit at +Y of the
# ankles). So +X is the left-right axis every fore/aft swing rotates about, and +Z is up.
STALKER_SKELETON = ENEMY_ROOT + "/Stalker/SKM_Stalker_Skeleton"
SIDE_AXIS = (1.0, 0.0, 0.0)
UP_AXIS = (0.0, 0.0, 1.0)
FWD_AXIS = (0.0, 1.0, 0.0)

STALKER = {
    "pelvis": "Bip01-Pelvis",
    "spine": "Bip01-Spine1",
    "chest": "Bip01-Spine3",
    "head": "Bip01-Head",
    "thigh": ("Bip01-L-Thigh", "Bip01-R-Thigh"),
    "calf": ("Bip01-L-Calf", "Bip01-R-Calf"),
    "foot": ("Bip01-L-Foot", "Bip01-R-Foot"),
    "upperarm": ("Bip01-L-UpperArm", "Bip01-R-UpperArm"),
    "forearm": ("Bip01-L-Forearm", "Bip01-R-Forearm"),
    "tail": ("Bone01", "Bone02", "Bone03", "Bone04"),
}


def stalker_gait(rig, frame_count, stride, knee, arm_swing, lean, bob, tail_sway):
    """One full two-step cycle. Phase 0 plants the left foot, 0.5 the right."""
    frames = []
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        pose = rest_pose(rig)
        rotate_bone(rig, pose, STALKER["spine"], SIDE_AXIS, lean)
        rotate_bone(rig, pose, STALKER["chest"], SIDE_AXIS, lean * 0.4)
        # Two footfalls per cycle, so the body rises and falls twice.
        offset_bone(rig, pose, STALKER["pelvis"], (0.0, 0.0, -bob * abs(math.sin(2 * math.pi * t))))
        rotate_bone(rig, pose, STALKER["pelvis"], UP_AXIS, 0.09 * math.sin(2 * math.pi * t))

        for side, phase in ((0, 0.0), (1, 0.5)):
            angle = 2 * math.pi * (t + phase)
            swing = stride * math.sin(angle)
            # The knee only bends on the way through, never on the planted half.
            flex = knee * max(0.0, -math.sin(angle - math.pi * 0.35))
            rotate_bone(rig, pose, STALKER["thigh"][side], SIDE_AXIS, swing)
            rotate_bone(rig, pose, STALKER["calf"][side], SIDE_AXIS, -flex)
            rotate_bone(rig, pose, STALKER["foot"][side], SIDE_AXIS, flex * 0.45 - swing * 0.3)
            # Arms counter the legs: same side, opposite phase.
            rotate_bone(rig, pose, STALKER["upperarm"][side], SIDE_AXIS, -arm_swing * math.sin(angle))
            rotate_bone(rig, pose, STALKER["forearm"][side], SIDE_AXIS,
                        -abs(arm_swing) * 0.5 - arm_swing * 0.3 * math.sin(angle))

        for depth, bone in enumerate(STALKER["tail"]):
            rotate_bone(rig, pose, bone, UP_AXIS,
                        tail_sway * math.sin(2 * math.pi * t - depth * 0.6))
        frames.append(pose)
    return frames


def stalker_attack(rig, frame_count):
    """A right-arm claw swipe with the whole torso behind it."""
    frames = []
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        pose = rest_pose(rig)
        if t < 0.35:                       # wind up
            k = t / 0.35
            reach, twist = -1.5 * k, -0.45 * k
        elif t < 0.55:                     # strike
            k = (t - 0.35) / 0.2
            reach, twist = -1.5 + 3.0 * k, -0.45 + 0.95 * k
        else:                              # recover
            k = (t - 0.55) / 0.45
            reach, twist = 1.5 * (1.0 - k), 0.5 * (1.0 - k)
        rotate_bone(rig, pose, STALKER["upperarm"][1], SIDE_AXIS, reach)
        rotate_bone(rig, pose, STALKER["forearm"][1], SIDE_AXIS, -0.8 + abs(reach) * 0.3)
        rotate_bone(rig, pose, STALKER["chest"], UP_AXIS, twist)
        rotate_bone(rig, pose, STALKER["spine"], UP_AXIS, twist * 0.5)
        rotate_bone(rig, pose, STALKER["head"], UP_AXIS, -twist * 0.4)
        frames.append(pose)
    return frames


def stalker_death(rig, frame_count):
    """Knees go, torso folds, body drops. The ragdoll takes over from wherever this ends."""
    frames = []
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        ease = t * t * (3.0 - 2.0 * t)
        pose = rest_pose(rig)
        rotate_bone(rig, pose, STALKER["spine"], SIDE_AXIS, 1.25 * ease)
        rotate_bone(rig, pose, STALKER["chest"], SIDE_AXIS, 0.55 * ease)
        rotate_bone(rig, pose, STALKER["head"], SIDE_AXIS, 0.5 * ease)
        for side in (0, 1):
            rotate_bone(rig, pose, STALKER["thigh"][side], SIDE_AXIS, 0.9 * ease)
            rotate_bone(rig, pose, STALKER["calf"][side], SIDE_AXIS, -1.6 * ease)
        offset_bone(rig, pose, STALKER["pelvis"], (0.0, 0.0, -62.0 * ease))
        frames.append(pose)
    return frames


def author_stalker():
    rig = Rig(STALKER_SKELETON)
    keyed = set()
    for key, value in STALKER.items():
        keyed.update(value if isinstance(value, tuple) else (value,))
    out = {}
    clips = {
        "AS_Stalker_Idle": stalker_gait(rig, 61, 0.05, 0.07, 0.04, 0.05, 1.5, 0.05),
        "AS_Stalker_Walk": stalker_gait(rig, 41, 0.42, 0.75, 0.30, 0.10, 5.0, 0.10),
        "AS_Stalker_Run": stalker_gait(rig, 25, 0.72, 1.30, 0.62, 0.28, 9.0, 0.18),
        "AS_Stalker_Attack": stalker_attack(rig, 25),
        "AS_Stalker_Death": stalker_death(rig, 37),
    }
    for name, frames in clips.items():
        out[name] = write_clip(STALKER_SKELETON, "%s/Stalker/%s" % (ENEMY_ROOT, name),
                               frames, keyed, rig)
    report["stalker"] = out
    return out


# --- crawler ----------------------------------------------------------------------------
# The old bio-mech spider is gone. This is the facehugger-shaped crawler out of the alien-eggs
# diorama, rigged by Scripts/RigFacehugger.py: eight legs of three joints in mirrored pairs, plus
# a four-joint tail. It keeps the archetype id "Spider" because the encounters, the tests and the
# streaming manifests all name it, and none of them care what the body looks like.
#
# Measured off the imported skeleton: the legs reach toward -Y and the tail lies along +Y, so the
# body faces -Y exactly as the old one did and the gait maths below carries over unchanged. The
# whole creature is 120cm long and 88cm tall, roughly a twentieth of the old spider's reach, which
# is why every distance here is in single or double digits where the old ones were hundreds.
SPIDER_SKELETON = ENEMY_ROOT + "/Spider/SKM_Spider_Skeleton"
SPIDER_BODY = "body"
SPIDER_TAIL = ("tail_1", "tail_2", "tail_3", "tail_4")


def _crawler_legs():
    """The leg table, derived from the naming contract instead of transcribed.

    Every one of the eight legs reaches the ground on this body - unlike the old spider, which had
    four walking legs and four short appendages that could not reach the floor. Gait phase is the
    alternating tetrapod: L1 R2 L3 R4 push while R1 L2 R3 L4 swing.

    The chain solves on the first two joints and plants the third: the last joint sits at the
    limb tip, and rotating a bone cannot move its own head, so including it would spend a solver
    iteration achieving nothing.
    """
    legs = []
    for index in (1, 2, 3, 4):
        for side in ("L", "R"):
            name = "leg_%s_%d" % (side, index)
            legs.append({
                "name": name,
                "chain": [name + "_a", name + "_b"],
                "tip": name + "_c",
                "phase": 0.0 if (side == "L") == (index % 2 == 1) else 0.5,
                # Legs 1 and 2 reach furthest forward (tips near y=-41); 3 and 4 brace at the
                # sides (y=-29). The attack rears on the side legs and strikes with the front.
                "front": index <= 2,
                "ground": True,
            })
    return legs


SPIDER_LEGS = _crawler_legs()
# Stride is bounded by the shortest chain, not the longest. The side legs measure about 17 units
# from their root to their tip and rest close to extended, so a 13-unit stride asked them for
# reach they do not have and left a foot 6 units off its target for the whole clip. 9 is what all
# eight can actually deliver.
SPIDER_STEP_HEIGHT = 4.0
SPIDER_STEP_LENGTH = 9.0


def spider_stance_targets(rig):
    """Where every tip belongs when the creature is simply standing.

    Read off the rest pose rather than authored as eight literals: this rig was generated from the
    mesh, so its rest pose IS a stance - the diorama had the creature crouched on its legs - and
    hardcoded targets would rot the moment RigFacehugger.py places a joint differently.

    No ground plane is imposed on top of it. Flattening all eight tips onto one Z left the short
    side legs 11 units short of a target they physically cannot reach, because their tips rest 10
    units higher than the front pair's; the solver then spent the whole clip straining. The rest
    tips are reachable by construction - they are where the limb already is - so the gait only has
    to add travel and lift on top.
    """
    targets = {}
    for leg in SPIDER_LEGS:
        targets[leg["name"]] = rig.component_of(leg["tip"], rig.local)[0]
    return targets


def spider_tail(rig, pose, sway, curl=0.0):
    """Sway the tail as one whip, each joint lagging the one before it."""
    for depth, bone in enumerate(SPIDER_TAIL):
        rotate_bone(rig, pose, bone, UP_AXIS, sway * math.sin(2 * math.pi * (0.0 - depth * 0.12)))
        if curl:
            rotate_bone(rig, pose, bone, SIDE_AXIS, curl * (0.6 + 0.4 * depth))


def _measure_reach(rig):
    """How far each chain can actually stretch, measured once off the rest pose.

    Cached on the leg entries because it is a property of the rig, not of a frame.
    """
    for leg in SPIDER_LEGS:
        joints = [rig.component_of(bone, rig.local)[0] for bone in leg["chain"]]
        joints.append(rig.component_of(leg["tip"], rig.local)[0])
        span = sum(v_len(v_sub(joints[i + 1], joints[i])) for i in range(len(joints) - 1))
        # 0.98: a chain solved dead straight is both unreachable in practice and ugly - it locks
        # the joint and the leg reads as a stick.
        leg["reach"] = span * 0.98


def _clamp_to_reach(rig, leg, target, pose):
    """Pull an out-of-range target back onto the chain's reachable sphere.

    Without this the solver simply fails on the shortest legs and reports the shortfall: these
    side legs rest close to extended, so a stride that suits the front pair asks them for distance
    that does not exist, and every frame of the clip carries the error. Clamping turns "a foot 4
    units off its target" into "a foot planted slightly short", which is what a real short leg
    does anyway.
    """
    root = rig.component_of(leg["chain"][0], pose)[0]
    delta = v_sub(target, root)
    distance = v_len(delta)
    if distance <= leg["reach"] or distance < 1e-6:
        return target
    return v_add(root, v_scale(delta, leg["reach"] / distance))


def spider_pose(rig, foot_targets, body_offset=(0.0, 0.0, 0.0), body_rotation=None):
    pose = rest_pose(rig)
    if body_rotation:
        rotate_bone(rig, pose, SPIDER_BODY, body_rotation[0], body_rotation[1])
    if body_offset != (0.0, 0.0, 0.0):
        offset_bone(rig, pose, SPIDER_BODY, body_offset)
    worst = 0.0
    for leg in SPIDER_LEGS:
        target = _clamp_to_reach(rig, leg, foot_targets[leg["name"]], pose)
        error = solve_chain(rig, pose, leg["chain"], leg["tip"], target)
        worst = max(worst, error)
    return pose, worst


def spider_gait(rig, frame_count, step_length, step_height, bob, name):
    """Alternating-tetrapod scuttle: two groups of four, half a cycle apart."""
    stance = spider_stance_targets(rig)
    frames = []
    worst = 0.0
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        targets = {}
        for leg in SPIDER_LEGS:
            phase = (t + leg["phase"]) % 1.0
            base = stance[leg["name"]]
            # This body faces -Y in its own space (the legs reach toward negative Y), so a planted
            # foot travels towards +Y while the creature moves forward.
            if phase < 0.5:
                travel = step_length * (-0.5 + phase * 2.0)
                lift = 0.0
            else:
                k = (phase - 0.5) * 2.0
                travel = step_length * (0.5 - k)
                lift = step_height * math.sin(math.pi * k)
            targets[leg["name"]] = (base[0], base[1] + travel, base[2] + lift)
        pose, error = spider_pose(
            rig, targets,
            body_offset=(0.0, 0.0, bob * math.sin(4 * math.pi * t)))
        # The tail counterweights the scuttle at the stride frequency, not twice it.
        spider_tail(rig, pose, 0.20 * math.sin(2 * math.pi * t))
        worst = max(worst, error)
        frames.append(pose)
    _log("crawler %s solved to within %.2f units" % (name, worst))
    if worst > 3.0:
        raise RuntimeError("crawler %s left a foot %.2f units off target" % (name, worst))
    return frames


def spider_idle(rig, frame_count):
    """Breathing on the legs: the body rises and falls and the tail keeps time."""
    stance = spider_stance_targets(rig)
    frames = []
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        breath = math.sin(2 * math.pi * t)
        pose, _error = spider_pose(rig, stance, body_offset=(0.0, 0.0, 1.6 * breath))
        spider_tail(rig, pose, 0.16 * breath)
        frames.append(pose)
    return frames


def spider_attack(rig, frame_count):
    """Rear back on the side legs, then stab the body forward over the front ones."""
    stance = spider_stance_targets(rig)
    frames = []
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        if t < 0.4:
            k = t / 0.4
            rear, lunge = k, 0.0
        elif t < 0.6:
            k = (t - 0.4) / 0.2
            rear, lunge = 1.0 - k, k
        else:
            k = (t - 0.6) / 0.4
            rear, lunge = 0.0, 1.0 - k
        targets = {}
        for leg in SPIDER_LEGS:
            base = stance[leg["name"]]
            if leg["front"]:
                targets[leg["name"]] = (base[0], base[1] - 5.5 * lunge,
                                        base[2] + 17.0 * rear + 3.0 * lunge)
            else:
                targets[leg["name"]] = base
        pose, _error = spider_pose(
            rig, targets,
            body_offset=(0.0, -6.0 * lunge, 9.0 * rear),
            body_rotation=(SIDE_AXIS, -0.4 * rear + 0.22 * lunge))
        # The tail whips over the top on the strike - it is the only part of this silhouette
        # that reads at range, and a still tail makes the whole lunge look like a slide.
        spider_tail(rig, pose, 0.10, curl=-0.35 * rear + 0.5 * lunge)
        frames.append(pose)
    return frames


def spider_death(rig, frame_count):
    """Legs curl inward and the body settles - the way a real arthropod dies."""
    stance = spider_stance_targets(rig)
    frames = []
    for index in range(frame_count):
        t = index / float(frame_count - 1)
        ease = t * t * (3.0 - 2.0 * t)
        targets = {}
        for leg in SPIDER_LEGS:
            base = stance[leg["name"]]
            pull = 0.55 * ease
            targets[leg["name"]] = (base[0] * (1.0 - pull), base[1] * (1.0 - pull),
                                    base[2] + 11.0 * ease)
        pose, _error = spider_pose(rig, targets, body_offset=(0.0, 0.0, -7.0 * ease))
        spider_tail(rig, pose, 0.05 * (1.0 - ease), curl=0.55 * ease)
        frames.append(pose)
    return frames


def author_spider():
    rig = Rig(SPIDER_SKELETON, ENEMY_ROOT + "/Spider/SKM_Spider")
    _measure_reach(rig)
    keyed = {SPIDER_BODY}
    keyed.update(SPIDER_TAIL)
    for leg in SPIDER_LEGS:
        keyed.update(leg["chain"])
        keyed.add(leg["tip"])
    out = {}
    clips = {
        "AS_Spider_Idle": spider_idle(rig, 61),
        "AS_Spider_Walk": spider_gait(rig, 41, SPIDER_STEP_LENGTH, SPIDER_STEP_HEIGHT, 1.2, "walk"),
        "AS_Spider_Run": spider_gait(rig, 21, SPIDER_STEP_LENGTH * 1.5,
                                     SPIDER_STEP_HEIGHT * 1.4, 2.6, "run"),
        "AS_Spider_Attack": spider_attack(rig, 25),
        "AS_Spider_Death": spider_death(rig, 37),
    }
    for name, frames in clips.items():
        out[name] = write_clip(SPIDER_SKELETON, "%s/Spider/%s" % (ENEMY_ROOT, name),
                               frames, keyed, rig)

    # The stance is also the pose the body should be measured in: the rest pose is the crouch the
    # model was posed in for a diorama, not a stance it holds on its feet. The definition script
    # reads these back to place the capsule and the mesh offset.
    stance_pose, error = spider_pose(rig, spider_stance_targets(rig))
    lowest = min(rig.component_of(leg["tip"], stance_pose)[0][2] for leg in SPIDER_LEGS)
    highest = max(rig.component_of(bone, stance_pose)[0][2] for bone in rig.order)
    widest = max(abs(rig.component_of(bone, stance_pose)[0][0]) for bone in rig.order)
    report["spider_stance"] = {"foot_plane": round(lowest, 1), "top": round(highest, 1),
                               "half_width": round(widest, 1), "solver_error": round(error, 2)}
    report["spider"] = out
    return out


# --- hound -----------------------------------------------------------------------------
def author_hound_walk():
    """The quadruped ships a run and no walk. A run played slower is a walk for a four-legged
    body - the gait does not change between them the way a biped's does."""
    target_path = author_rate_scaled(
        ENEMY_ROOT + "/Hound/SKM_HoundAlien-Animal_1_5_01_Run-Cycle",
        ENEMY_ROOT + "/Hound/AS_Hound_Walk", 0.45, "run cycle")
    report["hound"] = {"AS_Hound_Walk": target_path}
    return target_path


def author_rate_scaled(source_path, target_path, rate, note):
    """A second take built from an existing one by play rate alone.

    Honest for a gait that does not change shape with speed - a quadruped's run slowed down is
    its walk. Not honest for a biped, which is why the alien has three separately authored
    cycles.
    """
    source = unreal.load_asset(source_path)
    if not source:
        raise RuntimeError("missing take: " + source_path)
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        unreal.EditorAssetLibrary.delete_asset(target_path)
    clip = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if not clip:
        raise RuntimeError("could not duplicate " + source_path)
    clip.set_editor_property("rate_scale", rate)
    unreal.EditorAssetLibrary.save_asset(target_path, only_if_is_dirty=False)
    _log("wrote %s (%s at %.2f rate)" % (target_path, note, rate))
    return target_path


TASKS = {
    "stalker": lambda: author_stalker(),
    "spider": lambda: author_spider(),
    "hound": lambda: author_hound_walk(),
}


def main():
    selected = [name.strip() for name in os.environ.get("AH_ANIM_TASKS", "").split(",")
                if name.strip()]
    unknown = [name for name in selected if name not in TASKS]
    if unknown:
        raise RuntimeError("AH_ANIM_TASKS names no such creature: " + ", ".join(unknown))
    for name in (selected or list(TASKS)):
        TASKS[name]()
    with open(REPORT_PATH, "w") as handle:
        json.dump(report, handle, indent=1, default=str)
    _log("report " + REPORT_PATH)


main()
