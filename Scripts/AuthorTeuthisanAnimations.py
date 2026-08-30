"""Author the Teuthisan's five gameplay clips from the verified Baked_B takes.

Pure python transform: the Baked_B AnimSequences (route B control-rig bakes, already
verified real motion) are evaluated per frame with AnimPoseExtensions and rewritten
through the animation data controller. No Control Rig re-solve - the poses already exist
as bone tracks, re-solving would just reproduce them slower.

The looping clips are closed here as well. The takes are cinematic - nobody authored them
as cycles - so Idle ended 11cm and the crawl 25cm (plus a 10cm root-Z pop) away from their
first frame, and the single-node player restarts a loop with a cut, not a crossfade: that
gap arrived as a visible snap every cycle. The tail of each looping clip is therefore
blended back into its first frame over the last few frames before it is written.

TANIM_MODE=author (default): delete-and-recreate the five /Game/.../GameAnims clips.
TANIM_MODE=verify: read-back only, measures every claim, writes tanim_verify.json.

Run with UnrealEditor-Cmd -run=pythonscript. Reports land in Saved/TeuthisanAnims.
"""
import json, math, os, time, traceback
import unreal

TAG = "TANIM"
D = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "TeuthisanAnims")) + "/"
os.makedirs(D, exist_ok=True)
SRC = "/Game/Characters/Teuthisan/Baked_B/AS_Teuthisan_"
DEST = "/Game/Characters/Teuthisan/GameAnims"
SK_PATH = "/Game/Characters/Teuthisan/Rig/SK_Teuthisan_rig_v001"
FPS = 30
MODE = os.environ.get("TANIM_MODE", "author")
RUN_TARGET_SPEED = 300.0          # cm/s apparent gait speed for A_Teuthisan_Run
ATTACK_WINDOW_S = 2.0             # inside the demanded 1.5-2.5s band

APE = unreal.AnimPoseExtensions
LOCAL = unreal.AnimPoseSpaces.LOCAL
WORLD = unreal.AnimPoseSpaces.WORLD   # component space for a pose with no actor
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
EAL = unreal.EditorAssetLibrary

log = open(D + ("verify" if MODE == "verify" else "author") + "_log.txt", "w")


def w(s):
    log.write(str(s) + "\n"); log.flush(); unreal.log_warning("%s %s" % (TAG, str(s)[:400]))


def frame_count(asset):
    """Sampled key count. Derived from play length + rate; the data model api if present."""
    try:
        return int(asset.get_data_model().get_number_of_keys())
    except Exception:
        return int(round(asset.get_play_length() * FPS)) + 1


def read_take(path, want_world=()):
    """Every frame's local pose (all bones), plus component positions for named bones."""
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("missing source take " + path)
    n = frame_count(asset)
    opts = unreal.AnimPoseEvaluationOptions()
    pose0 = APE.get_anim_pose_at_frame(asset, 0, opts)
    bones = [str(b) for b in APE.get_bone_names(pose0)]
    poses, world = [], []
    t0 = time.time()
    for fr in range(n):
        pose = APE.get_anim_pose_at_frame(asset, fr, opts)
        local = {}
        for b in bones:
            t = APE.get_bone_pose(pose, b, LOCAL)
            local[b] = (t.translation, t.rotation, t.scale3d)
        poses.append(local)
        if want_world:
            cur = {}
            for b in want_world:
                t = APE.get_bone_pose(pose, b, WORLD).translation
                cur[b] = (t.x, t.y, t.z)
            world.append(cur)
        if fr % 100 == 0:
            w("  %s frame %d/%d (%.1fs)" % (path.rsplit("_", 1)[-1], fr, n, time.time() - t0))
    w("  read %s: %d frames, %d bones, %.1fs" % (path, n, len(bones), time.time() - t0))
    return asset, bones, poses, world


def _slerp(a, b, t):
    """Pure-python quaternion slerp; unreal.Quat exposes no interpolation to python."""
    dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w
    bx, by, bz, bw = (b.x, b.y, b.z, b.w) if dot >= 0.0 else (-b.x, -b.y, -b.z, -b.w)
    dot = abs(dot)
    if dot > 0.9995:
        q = unreal.Quat(a.x + t * (bx - a.x), a.y + t * (by - a.y),
                        a.z + t * (bz - a.z), a.w + t * (bw - a.w))
    else:
        theta = math.acos(min(1.0, dot))
        sin_theta = math.sin(theta)
        wa = math.sin((1.0 - t) * theta) / sin_theta
        wb = math.sin(t * theta) / sin_theta
        q = unreal.Quat(wa * a.x + wb * bx, wa * a.y + wb * by,
                        wa * a.z + wb * bz, wa * a.w + wb * bw)
    n = math.sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w)
    return unreal.Quat(q.x / n, q.y / n, q.z / n, q.w / n)


def close_loop(bones, poses, blend_frames):
    """Blend the tail of a clip back into its first frame so the loop restart is seamless.

    The last frame becomes frame 0 exactly, and the frames before it ease toward it with a
    smoothstep weight, so the correction is spread across blend_frames instead of landing as
    a different, smaller pop. In place; returns the pre-blend worst seam for the log.
    """
    worst = 0.0
    for b in bones:
        d = poses[-1][b][0] - poses[0][b][0]
        worst = max(worst, math.sqrt(d.x * d.x + d.y * d.y + d.z * d.z))
    n = len(poses)
    span = min(blend_frames, n - 1)
    for i in range(n - span, n):
        k = (i - (n - span)) / float(span)          # 0 at the blend start, 1 on the last frame
        weight = k * k * (3.0 - 2.0 * k)
        for b in bones:
            t, r, s = poses[i][b]
            t0, r0, s0 = poses[0][b]
            poses[i] = poses[i]  # keep dict identity; only the entry changes
            poses[i][b] = (
                unreal.Vector(t.x + (t0.x - t.x) * weight,
                              t.y + (t0.y - t.y) * weight,
                              t.z + (t0.z - t.z) * weight),
                _slerp(r, r0, weight),
                s,
            )
    return worst


def write_clip(name, bones, poses):
    pkg = DEST + "/" + name
    if EAL.does_asset_exist(pkg):
        EAL.delete_asset(pkg)
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property("target_skeleton", unreal.load_asset(SK_PATH))
    asset = TOOLS.create_asset(name, DEST, unreal.AnimSequence, factory)
    if not asset:
        raise RuntimeError("could not create " + pkg)
    controller = asset.get_editor_property("controller")
    controller.open_bracket("author " + name)
    controller.set_frame_rate(unreal.FrameRate(FPS, 1))
    controller.set_number_of_frames(unreal.FrameNumber(len(poses) - 1))
    for b in bones:
        controller.add_bone_track(b)
        controller.set_bone_track_keys(b, [p[b][0] for p in poses],
                                       [p[b][1] for p in poses], [p[b][2] for p in poses])
    controller.close_bracket()

    # Silent-drop gate: read a mid frame back and compare a bone that actually moves.
    mid = len(poses) // 2
    probe = bones[0]
    for b in bones:
        d = poses[mid][b][0] - poses[0][b][0]
        if abs(d.x) + abs(d.y) + abs(d.z) > 0.05:
            probe = b
            break
    got = APE.get_bone_pose(APE.get_anim_pose_at_frame(
        asset, mid, unreal.AnimPoseEvaluationOptions()), probe, LOCAL).translation
    want = poses[mid][probe][0]
    err = ((got.x - want.x) ** 2 + (got.y - want.y) ** 2 + (got.z - want.z) ** 2) ** 0.5
    if err > 0.05:
        raise RuntimeError("%s: %s reads back %.3fcm off what was written" % (pkg, probe, err))
    EAL.save_asset(pkg, only_if_is_dirty=False)
    w("wrote %s (%d frames, %d tracks, probe %s err %.4fcm)" % (pkg, len(poses), len(bones), probe, err))
    return pkg, asset


def xy_dist(a, b):
    return math.hypot(a.x - b.x, a.y - b.y)


def author():
    report = {}
    EAL.make_directory(DEST)

    # ---- Idle: full take, straight copy ------------------------------------------------
    _, bones, poses, _ = read_take(SRC + "Idle")
    root = bones[0]
    w("root bone = " + root)
    w("idle pre-blend seam %.2fcm" % close_loop(bones, poses, 15))
    pkg, _ = write_clip("A_Teuthisan_Idle", bones, poses)
    report["Idle"] = {"asset_path": pkg, "frames": len(poses)}

    # ---- Walk: crawl take, root X/Y travel removed --------------------------------------
    _, bones, poses, _ = read_take(SRC + "Crawl")
    first = poses[0][root][0]
    last = poses[-1][root][0]
    travel = xy_dist(first, last)
    duration = (len(poses) - 1) / float(FPS)
    natural = travel / duration
    w("crawl root XY travel %.1fcm over %.2fs -> natural speed %.2f cm/s" % (travel, duration, natural))
    for p in poses:
        t, r, s = p[root]
        # kill X/Y travel, keep the Z bob
        p[root] = (unreal.Vector(first.x, first.y, t.z), r, s)
    w("walk pre-blend seam %.2fcm" % close_loop(bones, poses, 24))
    pkg, _ = write_clip("A_Teuthisan_Walk", bones, poses)
    report["Walk"] = {"asset_path": pkg, "frames": len(poses), "natural_speed_cms": natural}

    # ---- Run: the Walk clip rate-scaled to an apparent ~300 cm/s gait -------------------
    run_pkg = DEST + "/A_Teuthisan_Run"
    if EAL.does_asset_exist(run_pkg):
        EAL.delete_asset(run_pkg)
    run = EAL.duplicate_asset(pkg, run_pkg)
    if not run:
        raise RuntimeError("could not duplicate Walk into Run")
    rate = RUN_TARGET_SPEED / natural
    run.set_editor_property("rate_scale", rate)
    EAL.save_asset(run_pkg, only_if_is_dirty=False)
    w("wrote %s (rate_scale %.3f = %.0f/%.2f)" % (run_pkg, rate, RUN_TARGET_SPEED, natural))
    report["Run"] = {"asset_path": run_pkg, "frames": len(poses), "rate_scale": rate}

    # ---- Attack: the most violent ~2s window of the Stand take --------------------------
    _, bones, poses, world = read_take(SRC + "Stand", want_world=None)
    # need component positions of the upper body per frame; re-read those only
    upper = [b for b in bones if any(k in b.lower()
             for k in ("spine", "neck", "head", "clavicle", "upperarm", "lowerarm", "hand"))]
    w("attack scoring over %d upper-body bones" % len(upper))
    asset = unreal.load_asset(SRC + "Stand")
    opts = unreal.AnimPoseEvaluationOptions()
    comp = []
    for fr in range(len(poses)):
        pose = APE.get_anim_pose_at_frame(asset, fr, opts)
        comp.append([APE.get_bone_pose(pose, b, WORLD).translation for b in upper])
    disp = []
    for i in range(1, len(comp)):
        disp.append(sum((comp[i][j] - comp[i - 1][j]).length() for j in range(len(upper))))
    win = int(ATTACK_WINDOW_S * FPS)          # 60 intervals -> 61 poses, 2.0s
    best_s, best_sum = 0, -1.0
    cur = sum(disp[:win])
    best_sum, best_s = cur, 0
    for s in range(1, len(disp) - win + 1):
        cur += disp[s + win - 1] - disp[s - 1]
        if cur > best_sum:
            best_sum, best_s = cur, s
    peak = max(range(len(disp)), key=lambda i: disp[i])
    w("attack window frames %d..%d (sum %.1fcm), peak per-frame displacement at %d (%.1fcm), peak inside=%s"
      % (best_s, best_s + win, best_sum, peak, disp[peak], best_s <= peak <= best_s + win))
    attack_poses = poses[best_s:best_s + win + 1]
    pkg, _ = write_clip("A_Teuthisan_Attack", bones, attack_poses)
    report["Attack"] = {"asset_path": pkg, "frames": len(attack_poses),
                        "source_window": [best_s, best_s + win], "peak_frame": peak,
                        "window_displacement_cm": best_sum}

    # ---- Death: full take, straight copy ------------------------------------------------
    _, bones, poses, _ = read_take(SRC + "Death")
    pkg, _ = write_clip("A_Teuthisan_Death", bones, poses)
    report["Death"] = {"asset_path": pkg, "frames": len(poses)}

    open(D + "tanim_report.json", "w").write(json.dumps(report, indent=2))
    w("AUTHOR DONE")


def verify():
    out = {}
    opts = unreal.AnimPoseEvaluationOptions()
    for name in ("Idle", "Walk", "Run", "Attack", "Death"):
        pkg = DEST + "/A_Teuthisan_" + name
        asset = unreal.load_asset(pkg)
        if not asset:
            out[name] = {"error": "missing " + pkg}
            continue
        n = frame_count(asset)
        probe = sorted(set(list(range(0, n, max(1, n // 12))) + [n - 1]))
        samples, names = [], None
        root_first = root_last = None
        for fr in probe:
            pose = APE.get_anim_pose_at_frame(asset, fr, opts)
            if names is None:
                names = [str(b) for b in APE.get_bone_names(pose)]
            cur = {}
            for b in names:
                t = APE.get_bone_pose(pose, b, WORLD).translation
                cur[b] = (t.x, t.y, t.z)
            samples.append(cur)
            rt = APE.get_bone_pose(pose, names[0], LOCAL).translation
            if fr == 0:
                root_first = rt
            if fr == n - 1:
                root_last = rt
        worst, worst_bone = 0.0, None
        for b in names:
            for i in range(len(samples)):
                for j in range(i + 1, len(samples)):
                    a, c = samples[i][b], samples[j][b]
                    d = sum((a[k] - c[k]) ** 2 for k in range(3)) ** 0.5
                    if d > worst:
                        worst, worst_bone = d, b
        gap = max(sum((samples[0][b][k] - samples[-1][b][k]) ** 2 for k in range(3)) ** 0.5
                  for b in names)
        rec = {
            "asset_path": pkg,
            "frames": n,
            "play_length": asset.get_play_length(),
            "rate_scale": asset.get_editor_property("rate_scale"),
            "max_bone_movement_cm": worst,
            "worst_bone": worst_bone,
            "loop_gap_cm": gap,
            "net_root_xy_cm": math.hypot(root_first.x - root_last.x, root_first.y - root_last.y),
            "root_z_range_cm": (max(s[names[0]][2] for s in samples)
                                - min(s[names[0]][2] for s in samples)),
        }
        out[name] = rec
        w("VERIFY %s %s" % (name, json.dumps(rec)))
    open(D + "tanim_verify.json", "w").write(json.dumps(out, indent=2))
    w("VERIFY DONE")


try:
    verify() if MODE == "verify" else author()
except Exception:
    w("FAILED\n" + traceback.format_exc())
    raise
finally:
    log.close()
