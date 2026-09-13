"""Read-only per-frame creature motion audit; run with UnrealEditor-Cmd -run=pythonscript."""
import json
import os
import unreal

APE = unreal.AnimPoseExtensions
out = {}
for name in ("Pilgrim", "Hound", "Spider", "Teuthisan"):
    definition = unreal.load_asset("/Game/Ashes/Data/Enemies/DA_Enemy_" + name)
    visual = definition.get_editor_property("visuals")
    locomotion = visual.get_editor_property("locomotion")
    scale = visual.get_editor_property("mesh_scale").z
    clips = {}
    for state in ("walk", "run", "attack"):
        clip = locomotion.get_editor_property(state)
        # Soft references may arrive as paths or resolved UObjects depending on UE version.
        if not isinstance(clip, unreal.AnimSequence):
            clip = unreal.load_asset(str(clip))
        length = clip.get_play_length()
        rate = clip.get_editor_property("rate_scale")
        tracks = clip.get_editor_property("data_model_interface").get_bone_track_names()
        options = unreal.AnimPoseEvaluationOptions()
        pose = APE.get_anim_pose_at_time(clip, 0.0, options)
        names = [str(b) for b in APE.get_bone_names(pose)]
        probes = [b for b in names if any(k in b.lower() for k in ("foot", "hand", "head", "jaw")) or b.endswith("_c")]
        points = {b: [] for b in probes}
        n = max(2, round(length * 30))
        for i in range(n + 1):
            pose = APE.get_anim_pose_at_time(clip, length * i / n, options)
            for b in probes:
                p = APE.get_bone_pose(pose, b, unreal.AnimPoseSpaces.WORLD).translation
                points[b].append([p.x * scale, p.y * scale, p.z * scale])
        clips[state] = {"path": clip.get_path_name(), "length": length, "rate": rate,
                        "samples": points, "bones": names, "track_count": len(tracks)}
    out[name] = {"scale": scale, "clips": clips}
path = os.path.join(unreal.Paths.project_saved_dir(), "EnemyMotionAudit.json")
with open(path, "w") as handle:
    json.dump(out, handle)
unreal.log_warning("Enemy motion audit saved: " + path)
