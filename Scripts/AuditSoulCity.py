"""Measure the CityCompletion layer against the invariants CompleteSoulCity.py claims.

    python3 Scripts/RunInEditor.py Scripts/AuditSoulCity.py

Read-only: it spawns nothing, deletes nothing and saves nothing. It exists because
"nothing the player can stand on floats" and "placement is collision-safe" are claims, and a
claim that is not measured is a hope. Results land in Saved/CitySoulAuditReport.json plus a
summary in Saved/CitySoulAuditReport.txt.

Must run in the RUNNING editor, never a -nullrhi commandlet: a commandlet has no ticked
physics scene, every line_trace reports no blocking hit, and the audit would report the entire
district as hanging over void.

What it measures, and what it deliberately does not:

  * FLOAT - traces down under each tagged piece and reports two different failures. A sample
    that hits NOTHING is the worst case (unsupported over void); a sample that hits something
    too far below is a gap. Both are what ensure_seated() is supposed to prevent.
  * OVERLAP - get_actor_bounds is already a world AABB, so a pairwise test measures the real
    intersection regardless of what the placement test believed the box was. This is the check
    that catches a building turned to its street heading being cleared against its zero-yaw
    extents.
  * NAV - how much of what was built falls outside the navmesh volume. Expected to be non-zero:
    NAV_MAX_HALF is a deliberate playable radius, not a bug. The number is here so the size of
    the decision stays visible.
  * FRONTAGE - buildings with no road slab anywhere near them.

Two traps this script is written around, both of which produce confident wrong answers:

  * NON-COLLIDING DECORATION is not a platform. The frames hanging at a shear line are spawned
    with NoCollision precisely so the player cannot stand on them, and counting them as
    floating geometry is a false positive. Note the enum's str() is
    "<CollisionEnabled.NO_COLLISION: 0>", so the test is a substring, not an equality.
  * AABB CORNERS ARE NOT GEOMETRY. Buildings are PackedLevelActors turned to a street heading
    and tilted off plumb, so their world AABB corners are empty air by construction - sampling
    them reported 26 of 46 buildings floating while every one of them sat on its podium. Only
    the simple convex pieces ensure_seated() actually governs are sampled this way. For the
    same reason a yawed road slab's AABB is wider than the slab, so a sample can fall outside
    the piece it is meant to be testing: a slab that misses one sample of five is bridging a
    gap in the landscape's own collision, not hanging over a drop.
"""
import json
import math
import os

import unreal

TAG = "AH_CityCompletion"
SEAT_MAX_AIR = 120.0        # matches CompleteSoulCity.SEAT_MAX_AIR
NO_ROAD_REACH = 6000.0      # a building further than this from any slab fronts nothing
PROBE = ((0.0, 0.0), (-0.6, -0.6), (0.6, -0.6), (-0.6, 0.6), (0.6, 0.6))

EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
WORLD = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def saved_path(*parts):
    return os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), *parts)


def tags_of(actor):
    try:
        return [str(t) for t in actor.get_editor_property("tags")]
    except Exception:
        return []


def bounds_of(actor):
    try:
        return actor.get_actor_bounds(False)
    except Exception:
        return None, None


def kind_of(label):
    for prefix in ("Road_", "Bldg_", "Podium_", "Fence_", "Nav_", "Break_"):
        if label.startswith(prefix):
            return prefix.rstrip("_")
    return "scatter"


def collides(actor):
    """Only geometry the player can stand on counts as a floating platform."""
    try:
        component = actor.static_mesh_component
    except Exception:
        return True
    if component is None:
        return True
    try:
        return "NO_COLLISION" not in str(component.get_collision_enabled())
    except Exception:
        return True


def ground_under(actor, origin, extent):
    """(worst air gap, samples that hit nothing) under a piece, ignoring the piece itself."""
    under = origin.z - extent.z
    start = origin.z + extent.z + 100.0
    worst, missed = None, 0
    for dx, dy in PROBE:
        px, py = origin.x + extent.x * dx, origin.y + extent.y * dy
        hit = unreal.SystemLibrary.line_trace_single(
            WORLD, unreal.Vector(px, py, start), unreal.Vector(px, py, start - 300000.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, True, [actor],
            unreal.DrawDebugTrace.NONE, True)
        data = hit.to_dict() if hit else None
        if not data or not data.get("blocking_hit"):
            missed += 1
            continue
        air = under - data["location"].z
        if worst is None or air > worst:
            worst = air
    return worst, missed


def main():
    actors = EAS.get_all_level_actors()
    mine = [a for a in actors if TAG in tags_of(a)]
    buckets = {}
    for actor in mine:
        buckets.setdefault(kind_of(actor.get_actor_label()), []).append(actor)

    report = {"total_actors": len(actors), "tagged": len(mine),
              "authored": len(actors) - len(mine),
              "by_kind": {k: len(v) for k, v in sorted(buckets.items())}}

    # --- does anything the player can stand on float? --------------------------------------
    # Bldg is absent on purpose: see the AABB note in the module docstring.
    void, gaps, checked, skipped = [], [], 0, 0
    for kind in ("Road", "Podium", "Break", "scatter"):
        for actor in buckets.get(kind, []):
            origin, extent = bounds_of(actor)
            if origin is None or extent.z <= 0.0:
                continue
            if not collides(actor):
                skipped += 1
                continue
            checked += 1
            worst, missed = ground_under(actor, origin, extent)
            label = actor.get_actor_label()
            if missed:
                void.append((label, kind, missed))
            elif worst is not None and worst > SEAT_MAX_AIR:
                gaps.append((label, kind, round(worst / 100.0, 2)))

    def tally(rows):
        out = {}
        for row in rows:
            out[row[1]] = out.get(row[1], 0) + 1
        return out

    # A piece missing one sample of five is bridging a hole in the landscape's collision while
    # resting on ground everywhere else. Missing most of them is a piece over a drop.
    report["float"] = {
        "checked": checked, "skipped_no_collision": skipped,
        "over_void": len(void), "over_void_by_kind": tally(void),
        "mostly_unsupported": sum(1 for v in void if v[2] >= 3),
        "void_sample_histogram": {str(n): sum(1 for v in void if v[2] == n)
                                  for n in range(1, len(PROBE) + 1)},
        "air_over_%.0fcm" % SEAT_MAX_AIR: len(gaps), "gap_by_kind": tally(gaps),
        "worst_gap_m": max([g[2] for g in gaps], default=0.0),
        "examples": sorted(gaps, key=lambda g: -g[2])[:10] or [v[0] for v in void[:10]]}

    # --- do placed buildings intersect each other? -----------------------------------------
    boxes = []
    for actor in buckets.get("Bldg", []):
        origin, extent = bounds_of(actor)
        if origin is not None:
            boxes.append((actor.get_actor_label(), origin.x, origin.y,
                          abs(extent.x), abs(extent.y)))
    pairs = []
    for i in range(len(boxes)):
        for j in range(i + 1, len(boxes)):
            a, b = boxes[i], boxes[j]
            depth = min((a[3] + b[3]) - abs(a[1] - b[1]), (a[4] + b[4]) - abs(a[2] - b[2]))
            if depth > 0.0:
                pairs.append((a[0], b[0], round(depth / 100.0, 1)))
    report["overlap"] = {"buildings": len(boxes), "intersecting_pairs": len(pairs),
                         "worst_m": max([p[2] for p in pairs], default=0.0),
                         "examples": sorted(pairs, key=lambda p: -p[2])[:10]}

    # --- does the navmesh cover what was built? --------------------------------------------
    nav = next((a for a in mine if "NavMeshBounds" in type(a).__name__), None)
    if nav is None:
        report["nav"] = {"volume": None}
    else:
        norigin, nextent = bounds_of(nav)
        built = outside = 0
        for kind in ("Road", "Bldg", "Podium"):
            for actor in buckets.get(kind, []):
                point = actor.get_actor_location()
                built += 1
                if (abs(point.x - norigin.x) > abs(nextent.x)
                        or abs(point.y - norigin.y) > abs(nextent.y)):
                    outside += 1
        report["nav"] = {
            "box_m": [round(abs(nextent.x) * 2 / 100.0), round(abs(nextent.y) * 2 / 100.0)],
            "centre_m": [round(norigin.x / 100.0), round(norigin.y / 100.0)],
            "built_actors": built, "outside_navmesh": outside,
            "outside_pct": round(100.0 * outside / max(1, built), 1)}

    # --- does every building front a road that actually got laid? --------------------------
    roads = [(a.get_actor_location().x, a.get_actor_location().y)
             for a in buckets.get("Road", [])]
    lonely = []
    for label, bx, by, _hx, _hy in boxes:
        near = min((math.hypot(bx - rx, by - ry) for rx, ry in roads), default=float("inf"))
        if near > NO_ROAD_REACH:
            lonely.append((label, round(near / 100.0, 1)))
    report["frontage"] = {
        "road_slabs": len(roads),
        "buildings_with_no_road_within_%.0fm" % (NO_ROAD_REACH / 100.0): len(lonely),
        "examples": sorted(lonely, key=lambda e: -e[1])[:10]}

    with open(saved_path("CitySoulAuditReport.json"), "w") as handle:
        json.dump(report, handle, indent=2)

    float_data = report["float"]
    lines = [
        "%d actors in the level, %d tagged %s, %d authored"
        % (report["total_actors"], report["tagged"], TAG, report["authored"]),
        "by kind: %s" % ", ".join("%s %d" % kv for kv in report["by_kind"].items()),
        "float: %d standable pieces checked (%d skipped as non-colliding decoration)"
        % (float_data["checked"], float_data["skipped_no_collision"]),
        "  %d have a support sample over nothing (%s); %d of those miss 3+ of %d samples"
        % (float_data["over_void"],
           ", ".join("%s %d" % kv for kv in float_data["over_void_by_kind"].items()) or "none",
           float_data["mostly_unsupported"], len(PROBE)),
        "  sample-miss histogram: %s" % float_data["void_sample_histogram"],
        "  %d hang more than %.0f cm over what IS below them (worst %.1f m)"
        % (float_data["air_over_%.0fcm" % SEAT_MAX_AIR], SEAT_MAX_AIR,
           float_data["worst_gap_m"]),
        "overlap: %d buildings, %d intersecting pairs, worst %.1f m"
        % (report["overlap"]["buildings"], report["overlap"]["intersecting_pairs"],
           report["overlap"]["worst_m"]),
    ]
    if report["nav"].get("volume", True) is not None:
        lines.append("nav: %s m box covers all but %d of %d built actors (%.1f%% outside - "
                     "NAV_MAX_HALF is a deliberate playable radius)"
                     % (report["nav"]["box_m"], report["nav"]["outside_navmesh"],
                        report["nav"]["built_actors"], report["nav"]["outside_pct"]))
    lines.append("frontage: %d road slabs, %d buildings with no slab within %.0f m"
                 % (report["frontage"]["road_slabs"],
                    report["frontage"]["buildings_with_no_road_within_%.0fm"
                                       % (NO_ROAD_REACH / 100.0)],
                    NO_ROAD_REACH / 100.0))
    with open(saved_path("CitySoulAuditReport.txt"), "w") as handle:
        handle.write("\n".join(lines) + "\n")
    for line in lines:
        unreal.log(line)


main()
