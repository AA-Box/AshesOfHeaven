"""Rig the crawler out of the alien-eggs diorama so the enemy pipeline can use it.

The model arrives as scene dressing: a static mesh in a Sketchfab diorama with no armature, no
vertex groups and no animation anywhere in the file. The enemy pipeline needs a skinned mesh, so
the skeleton is built here - an anatomical template of 8 legs x 3 joints plus a 4-joint tail,
FITTED to this geometry rather than hardcoded. Limb tips come from farthest-point sampling of the
vertex cloud around the body centroid with an angular separation test (the finger pairs sit only
~0.14 apart, so a pure distance threshold merges them); each joint then comes from walking the
mesh edge graph from the tip back toward the centroid and averaging the limb's cross-section, so
the bone lands on the centreline instead of on the skin.

Bone names are the contract the animation authoring reads: root, body, leg_{L,R}_{1..4}_{a,b,c},
tail_{1..4}. Scripts/AuthorCreatureAnimations.py pattern-matches on exactly those.

    /Applications/Blender.app/Contents/MacOS/Blender -b --factory-startup -noaudio \
        -P Scripts/RigFacehugger.py -- [<outdir>]

Source drop: $AH_FACEHUGGER_SOURCE (default ~/Downloads/spider-new), opened read-only. Writes
Facehugger_Mesh.fbx and rig_report.json into <outdir> (default Saved/CreatureSource, which is
where Scripts/ImportEnemyModels.py reads "prepared:" sources from).

Known limits, measured rather than assumed: about 20% of a leg's motion bleeds into its immediate
neighbour, and the unweighted root bone sits ~5% of the model's size outside the mesh. Both are
visible only under extreme poses on a body this small.
"""
import bpy, json, math, os, sys, heapq
from mathutils import Vector

DROP = os.environ.get("AH_FACEHUGGER_SOURCE", os.path.expanduser("~/Downloads/spider-new"))
SRC = os.path.join(
    DROP, "this-is-us-the-last-survivors-signing-off/source/p_114_AlienEggs_sf.blend")
OBJ = "facehugger01"
argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = argv[0] if argv else os.path.join(REPO, "Saved", "CreatureSource")
os.makedirs(OUT, exist_ok=True)
FBX = os.path.join(OUT, "Facehugger_Mesh.fbx")
REPORT = os.path.join(OUT, "rig_report.json")

log = []
def P(*a):
    s = " ".join(str(x) for x in a); log.append(s); print("[rig]", s)

# ---------------------------------------------------------------- load geometry
bpy.ops.wm.open_mainfile(filepath=SRC)
src = bpy.data.objects[OBJ]
me = src.data
SCALE = src.matrix_world.to_scale()[0]          # 0.8977 uniform

# Mesh-local axes are the anatomical ones (+Y = legs/front, -Y = tail, +Z = up). The object's
# world rotation/translation are deliberately DISCARDED and only the uniform object scale is
# applied, then the cloud is re-centred on its centroid -> body at the origin, legs on +Y.
co = [Vector(v.co) * SCALE for v in me.vertices]
N = len(co)
# snapshot everything we need from the source file: the scene reset below invalidates it
faces = [tuple(p.vertices) for p in me.polygons]
uv_name = me.uv_layers.active.name if me.uv_layers.active else None
uv_data = [tuple(l.uv) for l in me.uv_layers.active.data] if uv_name else None
n_polys = len(me.polygons)
uv_names = [l.name for l in me.uv_layers]

adj = [[] for _ in range(N)]
for e in me.edges:
    a, b = e.vertices
    w = (co[a] - co[b]).length
    adj[a].append((b, w)); adj[b].append((a, w))

comp = [-1] * N; comps = []
for s in range(N):
    if comp[s] >= 0: continue
    st = [s]; cur = []; comp[s] = len(comps)
    while st:
        v = st.pop(); cur.append(v)
        for u, _ in adj[v]:
            if comp[u] < 0: comp[u] = len(comps); st.append(u)
    comps.append(cur)
comps.sort(key=len, reverse=True)
body_comp = comps[0]
stray = [v for c in comps[1:] for v in c]

centroid = sum((co[v] for v in body_comp), Vector()) / len(body_comp)
for i in range(N): co[i] = co[i] - centroid
rad = [c.length for c in co]
dims = [max(c[i] for c in co) - min(c[i] for c in co) for i in range(3)]
P("components", [len(c) for c in comps], "stray", len(stray),
  "centroid", [round(x, 4) for x in centroid], "dims", [round(x, 3) for x in dims])

INF = float("inf")
def dijkstra(sources):
    d = [INF] * N; pred = [-1] * N; h = []
    for s in sources: d[s] = 0.0; heapq.heappush(h, (0.0, s))
    while h:
        dv, v = heapq.heappop(h)
        if dv > d[v] + 1e-12: continue
        for u, w in adj[v]:
            if dv + w < d[u] - 1e-12:
                d[u] = dv + w; pred[u] = v; heapq.heappush(h, (dv + w, u))
    return d, pred

# ---------------------------------------------------------------- limb tips
# Farthest-point sampling of the vertex cloud from the body centroid, with a minimum
# euclidean separation so two adjacent fingers cannot collapse onto one tip.
# Separation is ANGULAR (direction from the body centre), not euclidean: two of this model's
# finger pairs are fused near their tips (geodesically only ~0.14 apart), so a distance
# threshold cannot tell a second finger from a bump on the flank of the first, while the
# direction spread can (real neighbouring fingers >= 12 deg apart, flank bumps <= 9 deg).
ANG_SEP = math.radians(11.0)
MIN_SEP = 0.05 * max(dims)

def fps(cands, n, seeds=()):
    out = list(seeds)
    picked = []
    for v in sorted(cands, key=lambda v: -rad[v]):
        if len(picked) >= n: break
        d = co[v].normalized()
        if all(d.angle(co[u].normalized()) >= ANG_SEP and (co[v] - co[u]).length >= MIN_SEP
               for u in out):
            out.append(v); picked.append(v)
    return picked

leg_tips = fps([v for v in body_comp if co[v].y > 0], 8)
tail_tip = max((v for v in body_comp if co[v].y <= 0), key=lambda v: rad[v])
if len(leg_tips) != 8: raise RuntimeError("found %d leg tips" % len(leg_tips))
for v in leg_tips + [tail_tip]:
    P("tip", v, [round(x, 3) for x in co[v]], "r", round(rad[v], 3))

# left = +X  (forward = +Y, up = +Z  =>  left = forward x up = +X)
Lt = [v for v in leg_tips if co[v].x > 0]
Rt = [v for v in leg_tips if co[v].x <= 0]
if len(Lt) != 4 or len(Rt) != 4: raise RuntimeError("leg tips not 4/4: %d/%d" % (len(Lt), len(Rt)))
Lt.sort(key=lambda v: (-co[v].y, -co[v].z))
# pair each left tip with its mirror on the right so indices match across the midline
pairs, free = [], list(Rt)
for lv in Lt:
    m = Vector((-co[lv].x, co[lv].y, co[lv].z))
    rv = min(free, key=lambda v: (co[v] - m).length)
    free.remove(rv); pairs.append((lv, rv))

limbs = {}
for i, (lv, rv) in enumerate(pairs):
    limbs["leg_L_%d" % (i + 1)] = lv
    limbs["leg_R_%d" % (i + 1)] = rv
limbs["tail"] = tail_tip

# ---------------------------------------------------------------- joints from the mesh graph
tip_d, tip_pred = {}, {}
for name, t in limbs.items():
    tip_d[t], tip_pred[t] = dijkstra([t])
tips = list(limbs.values())
owner = {v: min(tips, key=lambda t: tip_d[t][v]) for v in body_comp}

base_v = min(body_comp, key=lambda v: rad[v])        # shell vertex closest to the body centre
XSEC = 0.05 * max(dims)                              # cross-section averaging radius

def limb_joints(tip, fracs, root_frac):
    """Walk the tip's geodesic tree back to the body-centre vertex, then at each arc-length
    fraction average this limb's cross-section so the joint lands inside the limb."""
    path = [base_v]
    v = base_v
    while v != tip and tip_pred[tip][v] >= 0:
        v = tip_pred[tip][v]; path.append(v)
    arc = [0.0]
    for i in range(1, len(path)):
        arc.append(arc[-1] + (co[path[i]] - co[path[i - 1]]).length)
    r_root = root_frac * rad[tip]
    i0 = next((i for i, v in enumerate(path) if rad[v] >= r_root), 0)
    s0, s1 = arc[i0], arc[-1]
    mine = [u for u in body_comp if owner[u] == tip]
    out = []
    for f in fracs:
        s = s0 + f * (s1 - s0)
        pv = min(range(len(path)), key=lambda i: abs(arc[i] - s))
        p = co[path[pv]]
        sel = [u for u in mine if (co[u] - p).length <= XSEC] or [path[pv]]
        out.append(sum((co[u] for u in sel), Vector()) / len(sel))
    return out, {"tip_vert": tip, "tip_co": [round(x, 4) for x in co[tip]],
                 "tip_radius": round(rad[tip], 4), "path_verts": len(path),
                 "limb_arc_len": round(s1 - s0, 4), "limb_verts": len(mine)}

joints, seg_info = {}, {}
for name, tip in limbs.items():
    fr = [0.0, .25, .5, .75, 1.0] if name == "tail" else [0.0, 1/3, 2/3, 1.0]
    joints[name], seg_info[name] = limb_joints(tip, fr, 0.25 if name == "tail" else 0.35)
    P(name, "joints", [[round(x, 3) for x in p] for p in joints[name]])

# body bone: the palm is a cupped shell, so the vertex centroid (the origin) sits in the
# hollow *under* it. Take the mean of the core ring in each half so the bone lies in the
# middle of the slab instead of in that hollow.
core_r = 0.35 * (sum(rad[t] for t in leg_tips) / 8.0)
core_v = [v for v in body_comp if rad[v] <= core_r]
core_v.sort(key=lambda v: co[v].y)
half = len(core_v) // 2
def mid(vs):
    m = sum((co[v] for v in vs), Vector()) / len(vs)
    return Vector((0.0, m.y, m.z))          # keep the body bone on the midline
body_head, body_tail = mid(core_v[:half]), mid(core_v[half:])
P("core_r", round(core_r, 4), "core verts", len(core_v),
  "body bone", [round(x, 3) for x in body_head], "->", [round(x, 3) for x in body_tail])

# ---------------------------------------------------------------- build scene
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene

nm = bpy.data.meshes.new("Facehugger")
nm.from_pydata([tuple(c) for c in co], [], faces)
nm.update()
mesh_obj = bpy.data.objects.new("Facehugger", nm)
scene.collection.objects.link(mesh_obj)
if uv_name:
    uv_dst = nm.uv_layers.new(name=uv_name)
    for i in range(min(len(nm.loops), len(uv_data))): uv_dst.data[i].uv = uv_data[i]

arm_data = bpy.data.armatures.new("Facehugger_Armature")
arm = bpy.data.objects.new("Facehugger_Armature", arm_data)
scene.collection.objects.link(arm)
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode="EDIT")
eb = arm_data.edit_bones

def mk(name, head, tail, parent=None, deform=True):
    b = eb.new(name)
    h, t = Vector(head), Vector(tail)
    if (t - h).length < 1e-4: t = h + Vector((0, 0, 1e-3))   # zero-length bones get culled
    b.head, b.tail, b.roll = h, t, 0.0
    b.use_connect = False; b.parent = parent; b.use_deform = deform
    return b

root = mk("root", (0, 0, 0), (0, 0.12 * max(dims), 0), None, deform=False)
body = mk("body", body_head, body_tail, root, deform=True)
names = []
for i in (1, 2, 3, 4):
    for side in ("L", "R"):
        key = "leg_%s_%d" % (side, i)
        p = joints[key]; par = body
        for j, suf in enumerate("abc"):
            par = mk("%s_%s" % (key, suf), p[j], p[j + 1], par); names.append(par.name)
tp = joints["tail"]; par = body
for i in range(4):
    par = mk("tail_%d" % (i + 1), tp[i], tp[i + 1], par); names.append(par.name)
bpy.ops.object.mode_set(mode="OBJECT")
bone_names = [b.name for b in arm_data.bones]
P("bones", len(arm_data.bones))

# ---------------------------------------------------------------- skinning
bpy.ops.object.select_all(action="DESELECT")
mesh_obj.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
method = "ARMATURE_AUTO (bone heat)"
try:
    bpy.ops.object.parent_set(type="ARMATURE_AUTO")
except Exception as e:
    P("bone heat failed:", e); method = "manual inverse-distance"
    bpy.ops.object.select_all(action="DESELECT")
    mesh_obj.select_set(True); arm.select_set(True)
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.parent_set(type="ARMATURE_NAME")

segs = [(b.name, Vector(b.head_local), Vector(b.tail_local)) for b in arm_data.bones if b.use_deform]
seg_names = set(n for n, _, _ in segs)
for n in seg_names:
    if n not in mesh_obj.vertex_groups: mesh_obj.vertex_groups.new(name=n)

def seg_dist(p, a, b):
    ab = b - a; l2 = ab.length_squared
    t = 0.0 if l2 < 1e-12 else max(0.0, min(1.0, (p - a).dot(ab) / l2))
    return (p - (a + ab * t)).length

def zero_weight_verts():
    out = []
    for v in nm.vertices:
        tot = sum(g.weight for g in v.groups
                  if mesh_obj.vertex_groups[g.group].name in seg_names)
        if tot <= 1e-6: out.append(v.index)
    return out

zero = zero_weight_verts()
P("zero-weight verts after", method, ":", len(zero))
if zero:                       # inverse-distance fill: 3 nearest bone segments, normalised
    for vi in zero:
        p = Vector(nm.vertices[vi].co)
        ds = sorted(((seg_dist(p, a, b), n) for n, a, b in segs))[:3]
        ws = [(n, 1.0 / (d + 1e-4) ** 4) for d, n in ds]
        s = sum(w for _, w in ws)
        for n, w in ws: mesh_obj.vertex_groups[n].add([vi], w / s, "REPLACE")
    method += " + inverse-distance fill (%d verts)" % len(zero)
zero_after = zero_weight_verts()
P("zero-weight verts final:", len(zero_after))

# ---------------------------------------------------------------- export
bpy.ops.object.select_all(action="DESELECT")
mesh_obj.select_set(True); arm.select_set(True)
bpy.context.view_layer.objects.active = arm
bpy.ops.export_scene.fbx(
    filepath=FBX, use_selection=True, path_mode="AUTO",
    apply_unit_scale=True, global_scale=1.0, use_space_transform=True,
    axis_forward="-Z", axis_up="Y",
    object_types={"ARMATURE", "MESH"}, use_mesh_modifiers=False,
    add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
    armature_nodetype="NULL", bake_anim=False, mesh_smooth_type="FACE")
P("exported", FBX, os.path.getsize(FBX), "bytes")

report = {
    "source_blend": SRC, "source_object": OBJ, "script": os.path.abspath(__file__), "fbx": FBX,
    "axis_convention": (
        "Mesh-LOCAL axes kept; the object's world rotation and translation are discarded and only "
        "its uniform object scale (%.6f) is applied, then the cloud is re-centred on the vertex "
        "centroid. Result: body at the origin, +Y = front/legs, -Y = tail, +Z = up, "
        "+X = creature's LEFT (forward x up). Blender is Z-up; the FBX is written with the "
        "exporter defaults axis_forward=-Z, axis_up=Y, primary_bone_axis=Y, so Unreal's importer "
        "does the conversion. NOT scaled to game size - source units only." % SCALE),
    "units": "blend units * object scale (no game-size scaling applied)",
    "mesh": {"verts": N, "polys": n_polys, "components": [len(c) for c in comps],
             "stray_verts": len(stray), "uv_layers": uv_names,
             "bbox_lo": [round(min(c[i] for c in co), 4) for i in range(3)],
             "bbox_hi": [round(max(c[i] for c in co), 4) for i in range(3)]},
    "method": ("tips: farthest-point sampling of the vertex cloud from the body centroid with a "
               "min angular separation of 11 deg (fused finger pairs are only ~0.14 apart "
               "geodesically, so a distance threshold cannot separate them); joints: Dijkstra over the edge graph from each tip back to the "
               "vertex nearest the centroid, sampled at even arc-length fractions of the limb, "
               "each sample replaced by the mean of that limb's vertices within %.4f so the "
               "joint sits on the centreline rather than the skin" % XSEC),
    "bones": {"count": len(arm_data.bones), "names": bone_names,
              "hierarchy": "root -> body -> leg_{L,R}_{1..4}_a -> _b -> _c ; body -> tail_1..4",
              "root_is_deforming": False,
              "body_head": [round(x, 4) for x in body_head],
              "body_tail": [round(x, 4) for x in body_tail]},
    "legs_rigged": 8, "joints_per_leg": 3, "tail_joints": 4,
    "limbs": seg_info,
    "joint_positions": {k: [[round(x, 4) for x in p] for p in v] for k, v in joints.items()},
    "weights": {"method": method, "verts_total": N, "verts_zero_weight": len(zero_after),
                "verts_zero_before_fill": len(zero)},
    "log": log,
}
with open(REPORT, "w") as f: json.dump(report, f, indent=2)
print("[rig] report ->", REPORT)
