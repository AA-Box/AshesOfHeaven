"""Bake the Teuthisan's cinematic Control Rig takes into AnimSequences.

The character's four takes (LS_Teuthisan_{Idle,Crawl,Stand,Death}) are LevelSequences full of
cameras, lights and cliffs, with the animation living in Control Rig parameter tracks - nothing
gameplay can play. This drives CR_Teuthisan's forward solve frame by frame and writes the
resulting bone poses straight into AnimSequences under /Game/Characters/Teuthisan/Baked_B.

No sequencer, no exporter, no world: the control rig parameter section's float/bool channels are
read directly, pushed onto a UControlRig's hierarchy as control values, solved, and the posed
bone locals are handed to the animation data controller the same way
Scripts/AuthorCreatureAnimations.py write_clip() does. The Sequencer route
(SequencerTools.export_anim_sequence) also works headless but only after spawning the sequence
through a LevelSequencePlayer, and its wait_for_delegate variant returns True while writing an
EMPTY clip - this route has no such trap.

Restore order on a fresh clone: Scripts/MigrateTeuthisan.py -> this -> 
Scripts/AuthorTeuthisanAnimations.py -> Scripts/PrepareTeuthisanGameMesh.py. All of it lands in
the gitignored /Game/Characters/Teuthisan folder; these scripts are the restore path.

Run with UnrealEditor-Cmd -run=pythonscript. Reports land in Saved/TeuthisanBake.
"""
import json, os, time, traceback
import unreal

TAG = "TEUTHBAKE"
D = unreal.Paths.convert_relative_path_to_full(
    os.path.join(unreal.Paths.project_saved_dir(), "TeuthisanBake")) + "/"
os.makedirs(D, exist_ok=True)
DEST = "/Game/Characters/Teuthisan/Baked_B"
SK_PATH = "/Game/Characters/Teuthisan/Rig/SK_Teuthisan_rig_v001"
TAKES = os.environ.get("TEUTH_TAKES", "Idle,Crawl,Stand,Death").split(",")
LIMIT = int(os.environ.get("TEUTH_LIMIT", "0"))
# The animation data controller rejects any rate that is not a factor or multiple of the project
# default (30fps), so the source's 24 is refused outright. 30 costs nothing: channels evaluate at
# the sequence's 24000/s tick resolution, which both 24 and 30 divide exactly, so the baked clip
# lands on the same duration as the source to the tick.
FPS = 30

MSE = unreal.MovieSceneSequenceExtensions
SX = unreal.MovieSceneSectionExtensions
APE = unreal.AnimPoseExtensions
LOCAL = unreal.AnimPoseSpaces.LOCAL
WORLD = unreal.AnimPoseSpaces.WORLD
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()
RATE = unreal.FrameRate(FPS, 1)
# SequencerScriptingRange is denominated in 60000 units per second - its own fixed resolution,
# NOT the sequence's tick resolution (24000 here). Getting that wrong reads the curve at 0.4x the
# intended time and still returns plausible-looking numbers. evaluate_keys walks a grid of
# 60000/rate units anchored at 0 and returns the point nearest the range start, and it yields
# nothing at all for a range narrower than a few units, so every query is one grid step wide.
SCRIPT_TICKS = 60000.0
report = {}
log = open(D + "bake_report.txt", "w")


def w(s):
    log.write(str(s) + "\n"); log.flush(); unreal.log_warning("%s %s" % (TAG, str(s)[:400]))


def sample(chan, tick, width):
    """One channel value at one tick.

    evaluate_keys returns nothing at all for a range narrower than its internal sampling
    interval - a one-tick range silently yields an empty array, which is what makes this the
    trap on this route: every control then reads None and the whole clip bakes as one pose.
    A range one output frame wide returns exactly one sample, taken at the range's start.
    """
    r = unreal.SequencerScriptingRange()
    r.set_editor_property("has_start_value", True)
    r.set_editor_property("has_end_value", True)
    r.set_editor_property("inclusive_start", int(tick))
    r.set_editor_property("exclusive_end", int(tick) + int(width))
    v = chan.evaluate_keys(r, RATE)
    return v[0] if len(v) else None


def keyed_section(seq):
    for b in MSE.get_bindings(seq):
        for t in b.get_tracks():
            if "ControlRigParameter" not in t.get_class().get_name():
                continue
            for s in t.get_sections():
                if len(SX.get_all_channels(s)) > 0:
                    return s, str(b.get_display_name()), t
    return None, None, None


def bake(seq_path):
    seq = unreal.load_asset(seq_path)
    if not seq:
        raise RuntimeError("no sequence " + seq_path)
    disp = MSE.get_display_rate(seq)
    tpf = SCRIPT_TICKS / (disp.numerator / float(disp.denominator))   # units per source frame
    step = SCRIPT_TICKS / FPS                                         # units per output frame
    start, end = int(MSE.get_playback_start(seq)), int(MSE.get_playback_end(seq))

    sec, binding, track = keyed_section(seq)
    if sec is None:
        raise RuntimeError("no keyed control rig section in " + seq_path)
    # The section will not hand python its rig, but the sequencer library will; match the proxy
    # back to the track owning the one section that actually carries channels.
    cr, proxies = None, unreal.ControlRigSequencerLibrary.get_control_rigs(seq)
    for px in proxies:
        if px.get_editor_property("track") == track:
            cr = px.get_editor_property("control_rig")
    if cr is None and proxies:
        cr = proxies[0].get_editor_property("control_rig")
    if cr is None:
        raise RuntimeError("no control rig for " + seq_path)
    H = cr.get_hierarchy()
    w("%s: binding=%s rig=%s playback=%d..%d @%s/%s bones=%d controls=%d"
      % (seq_path, binding, cr.get_class().get_name(), start, end,
         disp.numerator, disp.denominator, len(H.get_bones()), len(H.get_controls())))

    chans = dict((str(c.channel_name), c) for c in SX.get_all_channels(sec))
    last = {}

    def chan_val(name, tick):
        c = chans.get(name)
        if c is None:
            return None
        v = sample(c, tick, step)
        if v is None:                       # tick sits outside the channel's keyed range
            v = last.get(name)
            if v is None and c.get_num_keys():
                v = c.get_keys()[0].get_value()
        else:
            last[name] = v
        return v

    controls, unknown = [], []
    for k in H.get_controls():
        n = str(k.name)
        try:
            kind = str(H.get_control_settings(k).control_type).split(".")[-1].split(":")[0]
        except Exception:
            # RigControlType::Transform (7) has no python enum entry in 5.8 and raises on read.
            kind = "TRANSFORM7"
            unknown.append(n)
        controls.append((k, n, kind))
    if unknown:
        w("  control_type unreadable, treated as FTransform: %s" % unknown)

    skipped = {}

    def apply(tick):
        for k, n, kind in controls:
            try:
                if kind == "BOOL":
                    v = chan_val(n, tick)
                    if v is None:
                        skipped[n] = kind; continue
                    H.set_control_value(k, H.make_control_value_from_bool(bool(v)))
                elif kind in ("FLOAT", "SCALE_FLOAT"):
                    v = chan_val(n, tick)
                    if v is None:
                        skipped[n] = kind; continue
                    H.set_control_value(k, H.make_control_value_from_float(float(v)))
                elif kind == "ROTATOR":
                    r = [chan_val(n + ".Rotation." + a, tick) for a in ("Roll", "Pitch", "Yaw")]
                    if any(x is None for x in r):
                        skipped[n] = kind; continue
                    H.set_control_value(k, H.make_control_value_from_rotator(
                        unreal.Rotator(roll=r[0], pitch=r[1], yaw=r[2])))
                else:
                    p = [chan_val(n + ".Location." + a, tick) for a in "XYZ"]
                    r = [chan_val(n + ".Rotation." + a, tick) for a in "XYZ"]
                    s = [chan_val(n + ".Scale." + a, tick) for a in "XYZ"]
                    if any(x is None for x in p + r + s):
                        skipped[n] = kind; continue
                    loc = unreal.Vector(p[0], p[1], p[2])
                    rot = unreal.Rotator(roll=r[0], pitch=r[1], yaw=r[2])
                    scl = unreal.Vector(s[0], s[1], s[2])
                    if kind == "EULER_TRANSFORM":
                        val = H.make_control_value_from_euler_transform(
                            unreal.EulerTransform(location=loc, rotation=rot, scale=scl))
                    else:
                        val = H.make_control_value_from_transform(
                            unreal.Transform(location=loc, rotation=rot, scale=scl))
                    H.set_control_value(k, val)
            except Exception:
                w("  apply %s (%s) FAIL %s" % (n, kind, traceback.format_exc()[-200:]))
        if not cr.execute_event("Forwards Solve"):
            raise RuntimeError("forward solve failed at tick %d" % tick)

    # Correctness gate for the whole route: sampling a channel exactly on one of its own key
    # times has to return that key's value. That catches a sampling grid in the wrong tick
    # space or off by an interval - both of which otherwise bake a plausible but wrong clip.
    probe, best = None, 0
    for n, c in chans.items():
        distinct = len(set(round(k.get_value(), 6) for k in c.get_keys()))
        if distinct > 1 and c.get_num_keys() > best:
            probe, best = (n, c), c.get_num_keys()
    if probe is None:
        raise RuntimeError("no animated channel in " + seq_path)
    pname, pchan = probe
    for k in pchan.get_keys()[:8]:
        ft = k.get_time()
        t = round((ft.frame_number.value + ft.sub_frame) * tpf)
        got = sample(pchan, t, step)
        if got is None or abs(got - k.get_value()) > 1e-3:
            raise RuntimeError("key readback mismatch on %s at %s: %s != %s"
                               % (pname, t, got, k.get_value()))
    w("  key readback exact on %d keys of %s (%d keys); first output frames read %s"
      % (min(8, best), pname, best,
         [round(sample(pchan, start * tpf + i * step, step), 4) for i in range(6)]))

    bone_keys = [(k, str(k.name)) for k in H.get_bones()]
    count = int(round((end - start) * tpf / step)) + 1
    grid = [int(round(start * tpf + i * step)) for i in range(count)]
    if LIMIT:
        grid = grid[:LIMIT]
    w("  sampling %d frames at %dfps over %d channels" % (len(grid), FPS, len(chans)))
    poses = []
    t0 = time.time()
    for i, tick in enumerate(grid):
        apply(tick)
        pose = {}
        for k, n in bone_keys:
            t = H.get_local_transform(k, False)
            pose[n] = (t.translation, t.rotation, t.scale3d)
        poses.append(pose)
        if i % 50 == 0:
            w("  frame %d/%d (%.1fs)" % (i, len(grid), time.time() - t0))
    w("  sampled %d frames in %.1fs; controls never applied: %d %s"
      % (len(poses), time.time() - t0, len(skipped), sorted(skipped)[:10]))
    return poses, len(grid)


def write_clip(name, poses):
    pkg = DEST + "/AS_Teuthisan_" + name
    if unreal.EditorAssetLibrary.does_asset_exist(pkg):
        unreal.EditorAssetLibrary.delete_asset(pkg)
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property("target_skeleton", unreal.load_asset(SK_PATH))
    asset = TOOLS.create_asset("AS_Teuthisan_" + name, DEST, unreal.AnimSequence, factory)
    if not asset:
        raise RuntimeError("could not create " + pkg)
    ref = APE.get_reference_pose(unreal.load_asset(SK_PATH))
    sk_bones = [str(b) for b in APE.get_bone_names(ref)]
    controller = asset.get_editor_property("controller")
    controller.open_bracket("bake teuthisan " + name)
    controller.set_frame_rate(RATE)
    controller.set_number_of_frames(unreal.FrameNumber(len(poses) - 1))
    written = 0
    for bone in sk_bones:
        if bone in poses[0]:
            pos = [p[bone][0] for p in poses]
            rot = [p[bone][1] for p in poses]
            scl = [p[bone][2] for p in poses]
        else:                                # in the skeleton but not in the rig hierarchy
            t = APE.get_bone_pose(ref, bone, LOCAL)
            pos, rot, scl = ([t.translation] * len(poses), [t.rotation] * len(poses),
                             [t.scale3d] * len(poses))
        controller.add_bone_track(bone)
        controller.set_bone_track_keys(bone, pos, rot, scl)
        written += 1
    controller.close_bracket()
    unreal.EditorAssetLibrary.save_asset(pkg, only_if_is_dirty=False)
    return pkg, asset, written, len(sk_bones)


def verify(asset, frame_count):
    """Read the saved asset back and measure how far bones actually travel between frames."""
    opts = unreal.AnimPoseEvaluationOptions()
    probe = sorted(set([0, frame_count // 8, frame_count // 4, (3 * frame_count) // 8,
                        frame_count // 2, (5 * frame_count) // 8, (3 * frame_count) // 4,
                        (7 * frame_count) // 8, frame_count - 1]))
    samples, names = [], None
    for fr in probe:
        pose = APE.get_anim_pose_at_frame(asset, fr, opts)
        if names is None:
            names = [str(b) for b in APE.get_bone_names(pose)]
        cur = {}
        for n in names:
            t = APE.get_bone_pose(pose, n, WORLD).translation
            cur[n] = (t.x, t.y, t.z)
        samples.append(cur)
    worst, worst_bone, moving = 0.0, None, 0
    for n in names:
        far = 0.0
        for i in range(len(samples)):
            for j in range(i + 1, len(samples)):
                a, b = samples[i][n], samples[j][n]
                far = max(far, sum((a[c] - b[c]) ** 2 for c in range(3)) ** 0.5)
        if far > 0.1:
            moving += 1
        if far > worst:
            worst, worst_bone = far, n
    return worst, worst_bone, moving, probe


unreal.EditorAssetLibrary.make_directory(DEST)
for take in TAKES:
    try:
        poses, n = bake("/Game/Characters/Teuthisan/Animations/LS_Teuthisan_" + take)
        pkg, asset, tracks, total = write_clip(take, poses)
        worst, worst_bone, moving, probe = verify(asset, n)
        rec = dict(name=take, asset_path=pkg, animates=worst > 0.5, frames=n,
                   bone_tracks=tracks, skeleton_bones=total,
                   play_length=asset.get_play_length(), max_bone_movement=worst,
                   worst_bone=worst_bone, bones_that_move=moving, probe_frames=probe,
                   skeleton=asset.get_editor_property("skeleton").get_path_name())
        w("RESULT " + json.dumps(rec))
        report[take] = rec
    except Exception:
        w("FAILED %s\n%s" % (take, traceback.format_exc()))
        report[take] = dict(name=take, error=traceback.format_exc()[-800:])

open(D + "bake_report.json", "w").write(json.dumps(report, indent=2))
w("DONE")
log.close()
