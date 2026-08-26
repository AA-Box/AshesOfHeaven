"""Complete the Soul Slum city with a war-broken skyline, without touching what is there.

LV_Soul_Slum is the project's only authored city: 2,377 actors, 1,818 static meshes, 134
lights, 165 emitters, an authored ~1400 x 530 m slum strip. It ends in nothing. This grows a
destroyed city around it out of the migrated CitySample building kit plus the project's own
Erebus ruin meshes, so the slum reads as one district of a bombed-out metropolis.

Nothing existing is read-only by accident - it is read-only by construction:

  * every actor this script creates carries the tag AH_CityCompletion and lives in the
    outliner folder CityCompletion, and the first thing a run does is delete every actor with
    that tag. Re-running rebuilds; deleting the folder reverts the map to its authored state.
  * placement is rejected against the real positions and radii of the existing actors, so a
    new building can never land on authored geometry.
  * no light, post-process volume, fog or sky actor is created or edited. The slum's own
    lighting stays authoritative.

The map file is copied to Saved/MapBackups before the first save of a session.

Run it through the RUNNING editor, not a commandlet:

    python3 Scripts/RunInEditor.py Scripts/CompleteSoulCity.py

Placement is decided by tracing for real ground, and a `-nullrhi` commandlet has no ticked
physics scene - every trace misses and the whole ring ends up hanging in the air over the
reservoir. Idempotent either way.

Knobs: AH_CITY_SEED, AH_CITY_BUILDINGS, AH_CITY_SPACING_M.
"""

import math
import os
import random
import shutil

import unreal


MAP_PATH = "/Game/SoulCity/Maps/LV_Soul_Slum"
TAG = "AH_CityCompletion"
FOLDER = "CityCompletion"

BUILDING_DIR = "/Game/Building/Library/Kit_Hero_Bldg/LevelInstance"
EREBUS_MESHES = "/Game/Ashes/Environment/Erebus/Meshes"
ROCK_DIRS = ("/Game/Ashes/Environment/Rocks/Collection04",
             "/Game/Ashes/Environment/Rocks/PackVol01")
STREET_PROP_DIR = "/Game/Ashes/Props/Street"

SEED = int(os.environ.get("AH_CITY_SEED", "20260826"))
WANTED = int(os.environ.get("AH_CITY_BUILDINGS", "40"))
SPACING = float(os.environ.get("AH_CITY_SPACING_M", "115")) * 100.0

# Traced search area around the slum, and how fine the search is.
SEARCH_REACH = 130000.0
SEARCH_STEP = 4000.0
# Surfaces that count as buildable ground. Everything else a trace can land on here is either
# authored architecture or an invisible BlockingVolume whose top is nowhere near the terrain.
GROUND_ACTORS = ("Landscape", "Mountain", "Rock", "Cliff", "Terrain", "Flatrock", "Dam")

# 2004 x 2104 cm tile - a 21 m carriageway laid along its local +X. Its bounds origin sits at
# local y = +1000, so the actor has to be pushed half a road-width back or every tile lands
# ten metres off the path it is meant to follow.
ROAD_TILE = "/Game/Road/Kit_City_Road/SM_ROAD_19_20_0_0_road"
ROAD_STEP = 2004.0
ROAD_ORIGIN_Y = 1000.0
ROAD_SINK = 45.0            # bed the tile into the slope so its edges do not float
ROAD_MAX_TILES = 900
ROAD_LINK_MAX = 42000.0     # only link neighbouring ruins closer than 420 m
CULL_DISTANCE = 45000.0     # rubble and props stop drawing at 450 m

# A new building must clear existing geometry by this much, on top of both radii.
CLEARANCE = 6000.0
# Anything bigger than this is backdrop, not an obstacle, and must be excluded from the
# clearance test: the map's SM_SkySphere_Cloudy has a 32,768 m radius centred on the slum and
# rejects every candidate on its own. Merged dam walls, the water splash planes and the
# painted-city mattes are the same story. Measured radius p99 for real geometry is 600 m.
BACKDROP_RADIUS = 50000.0
OBSTACLE_RADIUS_CAP = 12000.0
# "Semi-broken": buildings are tilted off plumb, sunk so the lower floors read as collapsed,
# skirted in rubble, and scorched. The far rings tilt harder and sink deeper - distance is
# where a silhouette can take damage without becoming unreadable.
TILT_DEGREES = (1.5, 4.0, 6.5)
SINK_METRES = ((2.0, 6.0), (4.0, 11.0), (6.0, 16.0))
RUBBLE_PER_BUILDING = (4, 9)
DECALS_PER_BUILDING = 3

AR = unreal.AssetRegistryHelpers.get_asset_registry()
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAL = unreal.EditorAssetLibrary
REPORT = []


def load(path):
    return unreal.load_asset(path) if EAL.does_asset_exist(path) else None


def saved_path(*parts):
    return os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), *parts)


def backup_map():
    source = os.path.join(
        unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir()),
        MAP_PATH[len("/Game/"):] + ".umap")
    if not os.path.isfile(source):
        REPORT.append("MISSING map file " + source)
        return
    folder = saved_path("MapBackups")
    os.makedirs(folder, exist_ok=True)
    target = os.path.join(folder, os.path.basename(source))
    if not os.path.isfile(target):
        shutil.copy2(source, target)
        REPORT.append("backed up map to %s" % target)


# --- what is already there ---------------------------------------------------------------
def existing_footprints():
    """(x, y, radius) per authored mesh actor, and the trimmed core bounds."""
    spots = []
    for actor in EAS.get_all_level_actors():
        if has_tag(actor):
            continue
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        mesh = actor.static_mesh_component.get_editor_property("static_mesh")
        if not mesh:
            continue
        scale = actor.get_actor_scale3d()
        extent = mesh.get_bounds().box_extent
        location = actor.get_actor_location()
        radius = max(abs(extent.x * scale.x), abs(extent.y * scale.y))
        if radius > BACKDROP_RADIUS:
            continue
        spots.append((location.x, location.y, min(radius, OBSTACLE_RADIUS_CAP)))
    xs = sorted(s[0] for s in spots)
    ys = sorted(s[1] for s in spots)

    def pct(values, p):
        return values[max(0, min(len(values) - 1, int(len(values) * p)))]

    # A handful of backdrop actors sit kilometres out; the 2-98 band is the authored city.
    bounds = (pct(xs, 0.02), pct(ys, 0.02), pct(xs, 0.98), pct(ys, 0.98))
    return spots, bounds


def trace_ground(world, x, y):
    """(z, surface label) under a point, or None where there is only water or void.

    HitResult exposes nothing through get_editor_property in 5.8 - `blocking_hit` raises
    "Failed to find property". to_dict() is the accessor that works.
    """
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, 80000.0), unreal.Vector(x, y, -120000.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    data = hit.to_dict()
    if not data.get("blocking_hit"):
        return None
    actor = data.get("hit_actor")
    if actor is None or has_tag(actor):
        return None
    return data["location"].z, actor.get_actor_label()


def is_ground(label):
    return any(token.lower() in label.lower() for token in GROUND_ACTORS)


def land_points(world, centre, half, spots):
    """Traced, buildable, well-spread positions outside the authored slum."""
    found = []
    steps = int(SEARCH_REACH / SEARCH_STEP)
    for ix in range(-steps, steps + 1):
        for iy in range(-steps, steps + 1):
            x = centre[0] + ix * SEARCH_STEP
            y = centre[1] + iy * SEARCH_STEP
            # Outside the authored footprint, measured as an ellipse around it.
            ex = (x - centre[0]) / (half[0] + CLEARANCE)
            ey = (y - centre[1]) / (half[1] + CLEARANCE)
            if ex * ex + ey * ey < 1.0:
                continue
            result = trace_ground(world, x, y)
            if result is None:
                continue
            z, label = result
            if not is_ground(label):
                continue
            if not is_clear(x, y, 6000.0, spots):
                continue
            found.append((x, y, z))
    # Greedy thinning: nearest-to-the-slum first, so the city grows outward from the edge.
    found.sort(key=lambda p: (p[0] - centre[0]) ** 2 + (p[1] - centre[1]) ** 2)
    chosen = []
    for point in found:
        if all((point[0] - c[0]) ** 2 + (point[1] - c[1]) ** 2 >= SPACING * SPACING
               for c in chosen):
            chosen.append(point)
        if len(chosen) >= WANTED:
            break
    REPORT.append("traced %d land points outside the slum, kept %d after %.0f m spacing"
                  % (len(found), len(chosen), SPACING / 100.0))
    return chosen


def clear_of(x, y, margin, spots):
    for sx, sy, sr in spots:
        if (x - sx) ** 2 + (y - sy) ** 2 < (margin + sr) ** 2:
            return False
    return True


def is_clear(x, y, radius, spots):
    for sx, sy, sr in spots:
        if (x - sx) ** 2 + (y - sy) ** 2 < (radius + sr + CLEARANCE) ** 2:
            return False
    return True


# --- spawning ----------------------------------------------------------------------------
def has_tag(actor):
    try:
        return any(str(t) == TAG for t in actor.get_editor_property("tags"))
    except Exception:
        return False


def claim(actor, label):
    """Tag + folder, so every addition is findable and removable as one selection.

    `add_actor_tag` does not exist on PackedLevelActor in 5.8, and an unreal.Array assigned
    in place is a copy - the list has to be rebuilt and written back.
    """
    tags = [t for t in actor.get_editor_property("tags")]
    if not any(str(t) == TAG for t in tags):
        tags.append(unreal.Name(TAG))
        actor.set_editor_property("tags", tags)
    try:
        actor.set_folder_path(unreal.Name(FOLDER))
    except Exception:
        pass
    actor.set_actor_label(label)
    return actor


def spawn_class(cls, location, rotation, label):
    actor = EAS.spawn_actor_from_class(cls, unreal.Vector(*location), rotation)
    return claim(actor, label) if actor else None


def spawn_mesh(mesh, location, rotation, scale, label, material=None):
    actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), rotation)
    if not actor:
        return None
    component = actor.static_mesh_component
    component.set_editor_property("static_mesh", mesh)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if material:
        component.set_material(0, material)
    return claim(actor, label)


def clear_previous():
    removed = 0
    for actor in EAS.get_all_level_actors():
        if has_tag(actor):
            EAS.destroy_actor(actor)
            removed += 1
    REPORT.append("removed %d actors from a previous run" % removed)


# --- content ------------------------------------------------------------------------------
def hero_buildings():
    AR.scan_paths_synchronous([BUILDING_DIR], True)
    out = []
    for asset in AR.get_assets_by_path(BUILDING_DIR, recursive=True):
        name = str(asset.package_name)
        if str(asset.asset_class_path.asset_name) != "Blueprint":
            continue
        # "<name>_Level01" is the ground floor only - a district built from those is stumps.
        if name.endswith("_Level01"):
            continue
        blueprint = unreal.load_asset(name)
        generated = blueprint.generated_class() if blueprint else None
        if generated:
            out.append(generated)
    return out


def meshes_matching(folder, needles):
    AR.scan_paths_synchronous([folder], True)
    out = []
    for asset in AR.get_assets_by_path(folder, recursive=True):
        if str(asset.asset_class_path.asset_name) != "StaticMesh":
            continue
        name = str(asset.package_name).split("/")[-1]
        if not needles or any(n in name for n in needles):
            mesh = unreal.load_asset(str(asset.package_name))
            if mesh:
                out.append(mesh)
    return out


def lay_road(world, authored, start, end, label, budget):
    """Tile a road along a straight run, following the terrain and skipping anything blocked.

    Gaps are deliberate: where the trace finds water, void or authored geometry the tile is
    simply not placed, which reads as a road broken by the same war that broke the buildings.
    """
    dx, dy = end[0] - start[0], end[1] - start[1]
    length = math.hypot(dx, dy)
    if length < ROAD_STEP:
        return 0
    ux, uy = dx / length, dy / length
    yaw = math.degrees(math.atan2(uy, ux))
    radians = math.radians(yaw)
    # Cancel the mesh's own origin offset, rotated into world space.
    ox = -(-math.sin(radians) * ROAD_ORIGIN_Y)
    oy = -(math.cos(radians) * ROAD_ORIGIN_Y)

    mesh = load(ROAD_TILE)
    if not mesh:
        REPORT.append("MISSING " + ROAD_TILE)
        return 0

    laid = 0
    for step in range(int(length / ROAD_STEP)):
        if budget[0] <= 0:
            break
        travel = (step + 0.5) * ROAD_STEP
        px, py = start[0] + ux * travel, start[1] + uy * travel
        here = trace_ground(world, px, py)
        if here is None or not is_ground(here[1]):
            continue
        # Roads use their own tight margin against AUTHORED geometry only. The building
        # clearance (60 m, plus a 60 m radius per placed ruin) is what a road must ignore -
        # it leads to those buildings.
        if not clear_of(px, py, 900.0, authored):
            continue
        ahead = trace_ground(world, px + ux * ROAD_STEP * 0.5, py + uy * ROAD_STEP * 0.5)
        behind = trace_ground(world, px - ux * ROAD_STEP * 0.5, py - uy * ROAD_STEP * 0.5)
        pitch = 0.0
        if ahead and behind:
            pitch = -math.degrees(math.atan2(ahead[0] - behind[0], ROAD_STEP))
        spawn_mesh(mesh, (px + ox, py + oy, here[0] - ROAD_SINK),
                   unreal.Rotator(pitch=pitch, yaw=yaw, roll=0.0), (1.0, 1.0, 1.0),
                   "%s_%03d" % (label, step))
        laid += 1
        budget[0] -= 1
    return laid


def connect_roads(world, authored, centre, half, chosen):
    """A spur from every ruin back toward the slum, plus links between angular neighbours."""
    budget = [ROAD_MAX_TILES]
    tiles = 0
    for index, (x, y, _z) in enumerate(chosen):
        dx, dy = centre[0] - x, centre[1] - y
        length = math.hypot(dx, dy) or 1.0
        # Stop at the authored footprint rather than driving a road through the slum.
        stop = min(1.0, (half[0] + half[1]) * 0.5 / length)
        tiles += lay_road(world, authored, (x, y),
                          (x + dx * (1.0 - stop), y + dy * (1.0 - stop)),
                          "Road_Spur%02d" % index, budget)

    ordered = sorted(chosen, key=lambda p: math.atan2(p[1] - centre[1], p[0] - centre[0]))
    for index in range(len(ordered)):
        a = ordered[index]
        b = ordered[(index + 1) % len(ordered)]
        if math.hypot(b[0] - a[0], b[1] - a[1]) <= ROAD_LINK_MAX:
            tiles += lay_road(world, authored, (a[0], a[1]), (b[0], b[1]),
                              "Road_Link%02d" % index, budget)
    REPORT.append("laid %d road tiles (%d of budget left)" % (tiles, budget[0]))
    return tiles


def add_navigation(world, centre, half, chosen):
    """The map ships no NavMeshBoundsVolume at all, so nothing here was ever navigable."""
    if not chosen:
        return
    # Bounded to the slum plus a 300 m skirt. Covering every traced ruin would be a 2.0 x 2.2 km
    # navmesh - minutes to build and pointless, since the far ruins are silhouette.
    margin = 30000.0
    zs = [p[2] for p in chosen]
    mid = (centre[0], centre[1], (min(zs) + max(zs)) * 0.5 if zs else 0.0)
    extent = (2.0 * (half[0] + margin), 2.0 * (half[1] + margin), 40000.0)
    volume = spawn_class(unreal.NavMeshBoundsVolume, mid,
                         unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), "Nav_CityCompletion")
    if not volume:
        REPORT.append("FAILED could not spawn NavMeshBoundsVolume")
        return
    # A spawned volume's brush is a 200 uu cube.
    volume.set_actor_scale3d(unreal.Vector(extent[0] / 200.0, extent[1] / 200.0,
                                           extent[2] / 200.0))
    # Adding the volume makes the engine spawn a RecastNavMesh of its own. Untagged it would
    # survive a revert, leaving an actor the authored map never had.
    for actor in EAS.get_all_level_actors():
        if "RecastNavMesh" in type(actor).__name__ and not has_tag(actor):
            claim(actor, "Nav_RecastCityCompletion")
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    REPORT.append("nav volume %.0f x %.0f x %.0f m, RebuildNavigation issued"
                  % (extent[0] / 100.0, extent[1] / 100.0, extent[2] / 100.0))


def apply_culling():
    """No HLOD: `unreal.HierarchicalLODUtilities` does not exist in this build's python API.

    Distance culling on the scatter is the part that is scriptable, and it is where the actor
    count actually hurts - the buildings are packed level actors with their own instancing.
    """
    culled = scatter = 0
    for actor in EAS.get_all_level_actors():
        if not has_tag(actor) or not isinstance(actor, unreal.StaticMeshActor):
            continue
        label = actor.get_actor_label()
        if not label.startswith(("Rubble_", "Debris_", "Road_")):
            continue
        scatter += 1
        component = actor.static_mesh_component
        for name in ("ld_max_draw_distance", "l_d_max_draw_distance", "max_draw_distance",
                     "cached_max_draw_distance"):
            try:
                component.set_editor_property(name, CULL_DISTANCE)
                culled += 1
                break
            except Exception:
                continue
    REPORT.append("cull distance %.0f m on %d of %d scatter actors"
                  % (CULL_DISTANCE / 100.0, culled, scatter))


def main():
    LES.load_level(MAP_PATH)
    backup_map()
    clear_previous()

    rng = random.Random(SEED)
    spots, bounds = existing_footprints()
    x0, y0, x1, y1 = bounds
    centre = ((x0 + x1) * 0.5, (y0 + y1) * 0.5)
    half = (max((x1 - x0) * 0.5, 1.0), max((y1 - y0) * 0.5, 1.0))
    REPORT.append("authored core %.0f x %.0f m around (%.0f, %.0f) m"
                  % ((x1 - x0) / 100.0, (y1 - y0) / 100.0, centre[0] / 100.0, centre[1] / 100.0))

    buildings = hero_buildings()
    ruins = meshes_matching(EREBUS_MESHES, ("Ruin", "Rubble", "Wreck"))
    rocks = []
    for folder in ROCK_DIRS:
        rocks += meshes_matching(folder, ())
    props = meshes_matching(STREET_PROP_DIR, ())
    if not buildings:
        REPORT.append("FAILED no hero buildings; run Scripts/MigrateCitySampleKit.py")
        return
    REPORT.append("%d buildings, %d ruin meshes, %d rocks, %d street props"
                  % (len(buildings), len(ruins), len(rocks), len(props)))

    # Every tier gets an Erebus surface. CitySample facades are modern glass and polished
    # stone: left alone, a tower catches the moon and reads as intact and new, which is the
    # opposite of the brief. Near ruins take dry concrete, far ones burnt.
    burned = load("/Game/Ashes/Materials/Instances/MI_Erebus_Concrete_Burned")
    dry = (load("/Game/Ashes/Materials/Instances/MI_Erebus_Concrete_Dry")
           or load("/Game/Ashes/Materials/Instances/MI_Erebus_Concrete_Panels") or burned)
    decal_materials = [m for m in (
        load("/Game/Ashes/Materials/Instances/MI_Erebus_Decal_Scorch"),
        load("/Game/Ashes/Materials/Instances/MI_Erebus_Decal_Grime")) if m]

    world = unreal.UnrealEditorSubsystem().get_editor_world()
    chosen = land_points(world, centre, half, spots)
    authored = list(spots)   # snapshot before the new buildings are appended

    placed = rubble = decals = 0
    far = max((p[0] - centre[0]) ** 2 + (p[1] - centre[1]) ** 2 for p in chosen) if chosen else 1.0
    for index, (x, y, base) in enumerate(chosen):
        # Tier by distance: the nearest ruins are the ones the player walks up to, so they stay
        # closest to plumb and keep their own facades. Distance buys damage.
        reach = ((x - centre[0]) ** 2 + (y - centre[1]) ** 2) / far
        tier = 0 if reach < 0.34 else (1 if reach < 0.7 else 2)
        tilt = TILT_DEGREES[tier]
        sink_lo, sink_hi = SINK_METRES[tier]
        override = dry if tier == 0 else burned

        generated = buildings[rng.randrange(len(buildings))]
        sink = rng.uniform(sink_lo, sink_hi) * 100.0
        rotation = unreal.Rotator(pitch=rng.uniform(-tilt, tilt),
                                  yaw=rng.uniform(0.0, 360.0),
                                  roll=rng.uniform(-tilt, tilt))
        actor = EAS.spawn_actor_from_class(
            generated, unreal.Vector(x, y, base - sink), rotation)
        if not actor:
            continue
        claim(actor, "Ruin_T%d_%02d" % (tier, index))
        if override:
            # StaticMeshComponent, not InstancedStaticMeshComponent: a packed level actor keeps
            # some parts - notably tower glass - on plain mesh components, and overriding only
            # the ISMs leaves a mirror-finish skyscraper standing in a bombed city.
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                for slot in range(max(1, component.get_num_materials())):
                    component.set_material(slot, override)
        placed += 1
        spots.append((x, y, 6000.0))

        pool = ruins + rocks
        for piece in range(rng.randint(*RUBBLE_PER_BUILDING)):
            if not pool:
                break
            theta = rng.uniform(0.0, 2.0 * math.pi)
            distance = rng.uniform(2500.0, 7000.0)
            px = x + math.cos(theta) * distance
            py = y + math.sin(theta) * distance
            ground = trace_ground(world, px, py)
            if ground is None or not is_ground(ground[1]):
                continue
            scale = rng.uniform(0.8, 2.6)
            spawn_mesh(pool[rng.randrange(len(pool))], (px, py, ground[0]),
                       unreal.Rotator(pitch=rng.uniform(-12.0, 12.0),
                                      yaw=rng.uniform(0.0, 360.0),
                                      roll=rng.uniform(-12.0, 12.0)),
                       (scale, scale, scale), "Rubble_T%d_%02d_%02d" % (tier, index, piece))
            rubble += 1

        if props and tier == 0:
            for piece in range(2):
                theta = rng.uniform(0.0, 2.0 * math.pi)
                distance = rng.uniform(3000.0, 8000.0)
                px = x + math.cos(theta) * distance
                py = y + math.sin(theta) * distance
                ground = trace_ground(world, px, py)
                if ground is None or not is_ground(ground[1]):
                    continue
                spawn_mesh(props[rng.randrange(len(props))], (px, py, ground[0]),
                           unreal.Rotator(pitch=0.0, yaw=rng.uniform(0.0, 360.0), roll=0.0),
                           (1.0, 1.0, 1.0), "Debris_T%d_%02d_%02d" % (tier, index, piece))

        for piece in range(DECALS_PER_BUILDING if decal_materials else 0):
            theta = rng.uniform(0.0, 2.0 * math.pi)
            distance = rng.uniform(1500.0, 8000.0)
            dx = x + math.cos(theta) * distance
            dy = y + math.sin(theta) * distance
            ground = trace_ground(world, dx, dy)
            if ground is None or not is_ground(ground[1]):
                continue
            decal = spawn_class(
                unreal.DecalActor, (dx, dy, ground[0] + 60.0),
                unreal.Rotator(pitch=-90.0, yaw=rng.uniform(0.0, 360.0), roll=0.0),
                "Scorch_T%d_%02d_%02d" % (tier, index, piece))
            if decal:
                decal.set_decal_material(decal_materials[rng.randrange(len(decal_materials))])
                size = rng.uniform(4.0, 13.0)
                decal.set_actor_scale3d(unreal.Vector(1.0, size, size))
                decals += 1

    REPORT.append("placed %d buildings on traced ground, %d rubble, %d decals"
                  % (placed, rubble, decals))
    connect_roads(world, authored, centre, half, chosen)
    apply_culling()
    add_navigation(world, centre, half, chosen)

    if not unreal.EditorLoadingAndSavingUtils.save_map(
            unreal.EditorLevelLibrary.get_editor_world(), MAP_PATH):
        REPORT.append("FAILED could not save " + MAP_PATH)


try:
    main()
finally:
    with open(saved_path("CompleteSoulCityReport.txt"), "w") as handle:
        handle.write("\n".join(REPORT) + "\n")
