"""Rebuild only Stalker locomotion and verify its two-hand rifle grip in saved poses.

Run through RunInEditor.py after saving and backing up Stalker assets. Socket authoring is
separate: the report supplies the measured HandGrip_R transform for the skeletal mesh.
"""
import importlib.util
import json
import os
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
path = os.path.join(root, "Scripts", "AuthorCreatureAnimations.py")
spec = importlib.util.spec_from_file_location("stalker_rifle_author", path)
motion = importlib.util.module_from_spec(spec)
spec.loader.exec_module(motion)
motion.author_stalker(locomotion_only=True)
rig = motion.Rig(motion.STALKER_SKELETON)
right_hand, left_hand = "Bip01-R-Hand", "Bip01-L-Hand"
socket_q = motion.q_conj(rig.comp[right_hand][1])
socket_rotation = motion.quat_to_ue(socket_q).rotator()
report = {"socket_rotation": {"pitch": socket_rotation.pitch, "yaw": socket_rotation.yaw,
                               "roll": socket_rotation.roll}, "socket_scale": 1.0 / 2.54}
for state in ("Idle", "Walk", "Run"):
    clip = unreal.load_asset(motion.ENEMY_ROOT + "/Stalker/AS_Stalker_" + state)
    worst = 0.0
    for frame in range(round(clip.get_play_length() * 30) + 1):
        pose = motion.APE.get_anim_pose_at_frame(clip, frame, unreal.AnimPoseEvaluationOptions())
        right = motion.APE.get_bone_pose(pose, right_hand, motion.COMPONENT)
        left = motion.APE.get_bone_pose(pose, left_hand, motion.COMPONENT)
        weapon_q = motion.q_mul(motion.quat_from_ue(right.rotation), socket_q)
        grip = motion.v_add(motion.vec_from_ue(right.translation),
                            motion.q_rotate(weapon_q, (0.0, 28.522245, 3.002997)))
        worst = max(worst, motion.v_len(motion.v_sub(motion.vec_from_ue(left.translation), grip)))
    if worst > 1.0:
        raise RuntimeError("%s support hand drifts %.3f cm off rifle" % (state, worst))
    report[state] = {"max_support_hand_error_cm": worst}
saved = os.path.join(root, "Saved", "StalkerRifleGrip.json")
with open(saved, "w") as handle:
    json.dump(report, handle, indent=2)
unreal.log_warning("RIFLE_GRIP " + json.dumps(report))
