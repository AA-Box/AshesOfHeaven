"""Complete the Soul Slum city with a war-broken district, without touching what is there.

LV_Soul_Slum is the project's only authored city: 2,377 actors, 1,818 static meshes, 134
lights, 165 emitters, an authored ~1400 x 530 m slum strip. It ends in nothing. This grows a
destroyed district around it out of the migrated CitySample building kit plus the project's
own Erebus ruin meshes, so the slum reads as one quarter of a bombed-out metropolis.

Nothing existing is read-only by accident - it is read-only by construction:

  * every actor this script creates carries the tag AH_CityCompletion and lives in the
    outliner folder CityCompletion, and the first thing a run does is delete every actor with
    that tag. Re-running rebuilds; deleting the folder reverts the map to its authored state.
  * placement is rejected against the measured world bounds of EVERY authored actor, not just
    the static meshes - blocking volumes, brushes and group actors are obstacles too.
  * no light, post-process volume, fog or sky actor is created or edited. The slum's own
    lighting stays authoritative.
  * the only authored actors touched at all are the distance mattes, which are hidden, not
    modified: each one is tagged AH_CityCompletion_Hidden and the next run puts it back.

The map file is copied to Saved/MapBackups before the first save of a session.

Run it through the RUNNING editor, not a commandlet:

    python3 Scripts/RunInEditor.py Scripts/CompleteSoulCity.py

Placement is decided by tracing for real ground, and a `-nullrhi` commandlet has no ticked
physics scene - every trace misses and the whole district ends up hanging in the air over the
reservoir. Idempotent either way.

Knobs: AH_CITY_SEED, AH_CITY_BUILDINGS, AH_CITY_MATTE (hide-unlit|hide-all|keep). They are
read from the EDITOR's environment, not the shell that runs RunInEditor.py - the script is
executed inside the running editor, which does not inherit it. Edit the constant for a one-off.
The measured building pool is cached in Saved/ and re-measures itself whenever the
classification rules below change, so it needs no knob.

Measured facts this script is built on, all from Saved/CityProbe*.json:

  * The CitySample wall meshes carry NO materials. Their material slots are empty on both the
    component and the mesh, because CitySample assigns them from its C++ module, which this
    project deliberately does not migrate. Left alone they render as default grey - that, not
    a bad override, is why an untreated skyline reads as a white strip. The slot NAMES survive
    though ("Bldg_glass", "Bldg_block_limestone_grey", "Bldg_paintedMetal_blue", ...) and they
    are a complete material taxonomy. So: a slot that HAS a material keeps it - that preserves
    the authored rooftops, signs and awnings - and a slot that is empty is filled from its own
    name. One flat concrete over every slot is what made the old pass chalky.
  * Only 512 of 1,635 traced landscape samples are flat, and only 42 survive a smooth-
    neighbourhood test. This is a mountain valley spanning -204 m to +36 m; it cannot host a
    block grid. So streets come first and buildings line them: a street grows by picking the
    heading that costs the least height, which makes it follow the contours on its own, and
    buildings are placed as frontage along the streets it managed to lay.
"""

import json
import math
import os
import shutil

import random

import unreal


MAP_PATH = "/Game/SoulCity/Maps/LV_Soul_Slum"
TAG = "AH_CityCompletion"
HIDDEN_TAG = "AH_CityCompletion_Hidden"
FOLDER = "CityCompletion"

BUILDING_DIRS = ("/Game/Building/Library/Kit_Hero_Bldg/LevelInstance",
                 "/Game/Building/Library/Kit_Ref_Bldg")
EREBUS_MESHES = "/Game/Ashes/Environment/Erebus/Meshes"
ROCK_DIRS = ("/Game/Ashes/Environment/Rocks/Collection04",
             "/Game/Ashes/Environment/Rocks/PackVol01")
STREET_PROP_DIR = "/Game/Ashes/Props/Street"
POOL_CACHE = "CityBuildingPool.json"

SEED = int(os.environ.get("AH_CITY_SEED", "20260826"))
WANTED = int(os.environ.get("AH_CITY_BUILDINGS", "90"))
MATTE_MODE = os.environ.get("AH_CITY_MATTE", "hide-unlit")
REPOOL = bool(os.environ.get("AH_CITY_REPOOL"))

# --- ground ------------------------------------------------------------------------------
# Natural surfaces a building may stand on. Rock and cliff are back on this list on purpose:
# the old pass was wrong not because it accepted them by name but because it never checked the
# SLOPE, so a 60 m building could have its centre on a legal rock and half its footprint over
# a drop. Slope and relief now do that rejecting, which a cliff face fails on its own. Water
# and every authored structure stay off the list, so a trace onto a roof is not ground.
NATURAL_GROUND = ("Landscape", "Terrain", "Rock", "Cliff", "Mountain", "Flatrock")
# The map is a dam reservoir: soul_outdoor_watermesh_2 spans X -38..1540 m, Y -526..660 m with
# its surface at z = -21 m, while the valley floor under it runs down to -204 m. Without this
# test a street walks into the lake and a tower is built standing in it, which is what the
# first pass did. It CANNOT be found by tracing - the water mesh is NO_COLLISION, so a
# visibility ray goes straight through and reports the lake bed as good dry ground. The zone
# is measured from the water actors' own bounds instead.
WATER_LABELS = ("watermesh", "outdoor_water")
WATER_MARGIN = 200.0        # build above the waterline, not level with it
# A contour-following street has a cut slope above it and a fill slope below, so its own
# frontage is sloped by construction - that is what terracing is for. These tolerances let a
# building cut into the hill; they do not let it float. The base is set to the LOWEST sampled
# corner, so the uphill side is buried rather than the downhill side left hanging, and rubble
# is scattered along the cut. Tighter values than these refused 97% of every lot in the map.
# cos(26 deg): the grade a plot centre must beat.
MIN_NORMAL_Z = 0.90
# cos(45 deg): the grade a single perimeter sample may get away with.
EDGE_NORMAL_Z = 0.70
# cos(28 deg): a street may run steeper than a foundation, because its tiles pitch to the hill.
STREET_NORMAL_Z = 0.88
# Three of nine samples may fail ON SLOPE - a boulder should not veto a city block. A sample
# with NO ground under it at all (void, or the lake) is not in that budget: three of those can
# be one entire side of the footprint hanging over the drop, and the podium then becomes a
# rectangle bridging it. Missing ground is a hard refusal wherever in the footprint it lands.
EDGE_FAILURES = 3
# A footprint may not span more than this much height, or the uphill side buries whole floors.
MAX_RELIEF = 2800.0
# No building may bury more than this fraction of its own height in the hillside.
MAX_BURIAL_FRACTION = 0.35
# Traced ground is sampled on this lattice and cached; every consumer shares the cache.
SAMPLE_STEP = 500.0

# --- streets -----------------------------------------------------------------------------
# The CitySample road kit is all 20 m long tiles (only the width varies: 21 / 11 / 6 m). None
# of them fit this valley. Measured: with a rigid 20 m tile, only 1-8 of ~470 street segments
# could take one without a corner hanging in the air or being buried 6 m - the ground is simply
# rough at that scale, with rock outcrops breaking every plane fit. So the carriageway is the
# project's own cracked road slab instead: 2.6 x 2.5 m conforms to almost anything, and a
# broken slab path reads better in a bombed district than intact asphalt would.
ROAD_TILE = "/Game/Ashes/Environment/Erebus/Meshes/SM_Erebus_RoadSlab_Cracked_A"
SLAB_SCALE = 1.0   # the mesh is already 5.2 x 5.0 m
SLAB_OVERLAP = 0.92         # lay them slightly into each other so the path has no seams
ROAD_STEP = 2004.0          # tile length along its local +X
ROAD_WIDTH = 2104.0
ROAD_ORIGIN_Y = 1000.0      # bounds origin sits half a carriageway off centre
ROAD_MAX_TILES = 1200
# Slabs actually laid in the 20 m cell a lot fronts, before that lot counts as having a street.
# grow_streets() plans a network; lay_streets() refuses every slab it cannot seat (1,078 of
# them in the last measured run), and frontage on a street that exists only in the graph is a
# row of buildings facing bare hillside.
FRONT_MIN_SLABS = 2
SLAB_LENGTH = 468.0
# Largest gap a tile may leave under any corner once its own plane is fitted.
ROAD_MAX_RESIDUAL = 60.0
# A road is not a ramp. Tiles steeper than this are not laid at all. Matched to the 28% grade
# STREET_MAX_RISE allows a street to climb, plus a little for lattice quantisation.
ROAD_MAX_TILT = 26.0
# After bedding, this is how deep the far corner may be cut INTO the hill. Generous on purpose:
# bedding already guarantees no corner floats, and a buried edge is a road cut, not a hazard -
# the player walks on terrain there. Floating is the failure mode that matters.
ROAD_MAX_BURY = 300.0
# A hair more, so the bedded tile does not z-fight the ground it is resting on.
ROAD_BED = 10.0
STREET_SEEDS = 14
STREET_MAX_STEPS = 60
STREET_BRANCH_CHANCE = 0.16
STREET_MIN_STEPS = 4
SEED_SCAN_STEP = 5000.0
SEED_SCAN_REACH = 150000.0
SEED_SPACING = 22000.0
# Candidate turns per step. A street prefers to go straight and prefers not to climb.
# The fan has to be wide enough to CONTOUR. At +/-26 degrees a street aimed at a hillside can
# only climb it, and 41 steps in a row were refused as too steep; a real road turns 60 degrees
# and goes around. Cost trades rise against turn, so it only swings wide when it must.
STREET_TURNS = (0.0, -14.0, 14.0, -28.0, 28.0, -42.0, 42.0, -56.0, 56.0)
STREET_STRAIGHT_BONUS = 300.0
STREET_TURN_COST = 3.0
STREET_MAX_RISE = 560.0     # per 20 m step: a 28% grade - steep, but so is San Francisco
# Each seed is tried radially and along both contours; the longest street wins.
STREET_TRIAL_HEADINGS = (0.0, 70.0, -70.0)
SEED_MIN_OPEN_NEIGHBOURS = 4
# Candidate buildings tried per lot, smallest footprint first.
FIT_ATTEMPTS = 16

# --- lots --------------------------------------------------------------------------------
LOT_PITCH = 4600.0          # distance between building lots along a street
SETBACK = 3400.0            # street centreline to building centre
CLEARANCE = 1500.0          # extra gap a new building keeps from anything already there
BACKDROP_RADIUS = 50000.0   # bigger than this is backdrop (skysphere, mattes), not an obstacle
OBSTACLE_RADIUS_CAP = 12000.0
CULL_DISTANCE = 45000.0

# --- damage ------------------------------------------------------------------------------
# A 6 degree lean on a 190 m tower reads as a placement bug, not bomb damage. Damage now comes
# from removing instances - shearing floors off and collapsing a bay - so the lean is a hint.
TILT_DEGREES = {"tower": 0.9, "mid": 1.8, "low": 2.8}
# Barely any sink now: the podium carries the slope, and damage comes from shearing.
SINK_METRES = {"tower": (0.0, 0.6), "mid": (0.0, 0.8), "low": (0.0, 0.5)}
PODIUM_MESH = "/Engine/BasicShapes/Cube"
PODIUM_MIN = 60.0           # below this the plot is flat enough to sit straight on
PODIUM_MAX = 900.0          # a plinth taller than this is a retaining wall, not a building
PODIUM_SKIRT = 120.0        # push the base below the lowest sample so it cannot float
PODIUM_INSET = 0.96         # slightly inside the footprint so it does not poke through walls
# Clear air allowed under any loose piece before it is culled outright.
SEAT_MAX_AIR = 120.0
# Fencing the edge of the map's collision, where a step off is an endless fall.
FENCE_STEP = 800.0
FENCE_HEIGHT = 1200.0
FENCE_MARGIN = 6000.0
FENCE_MAX = 1500
# The deliberate playable radius. Streets seed up to SEED_SCAN_REACH out and then grow another
# ~1.2 km, so sizing the navmesh to the authored slum left most of the walkable district with
# no navigation; sizing it to the full street extent would ask Recast to build over the whole
# valley. Everything past this is scenery you can see but not fight in.
NAV_MAX_HALF = 100000.0
NAV_MARGIN = 20000.0
LOCAL_FENCE_CELL = 500.0
SHEAR_CHANCE = {"tower": 0.75, "mid": 0.6, "low": 0.35}
SHEAR_KEEP = (0.35, 0.85)   # fraction of height left standing
SHEAR_RAGGED = 1800.0       # instances within this of the cut survive by luck
BAY_CHANCE = 0.55           # a vertical slice of facade collapses
BAY_ARC = (0.18, 0.42)      # fraction of the circle the collapsed bay spans

AR = unreal.AssetRegistryHelpers.get_asset_registry()
EAS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LES = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
EAL = unreal.EditorAssetLibrary
REPORT = []
CULLED = [0]
SOLIDS = [0, 0]     # plain mesh components sheared away, plain components left standing


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


# --- tagging ------------------------------------------------------------------------------
def tags_of(actor):
    try:
        return [str(t) for t in actor.get_editor_property("tags")]
    except Exception:
        return []


def has_tag(actor, tag=TAG):
    return tag in tags_of(actor)


def set_tags(actor, names):
    actor.set_editor_property("tags", [unreal.Name(n) for n in names])


def claim(actor, label):
    """Tag + folder, so every addition is findable and removable as one selection.

    `add_actor_tag` does not exist on PackedLevelActor in 5.8, and an unreal.Array assigned
    in place is a copy - the list has to be rebuilt and written back.
    """
    names = tags_of(actor)
    if TAG not in names:
        names.append(TAG)
        set_tags(actor, names)
    try:
        actor.set_folder_path(unreal.Name(FOLDER))
    except Exception:
        pass
    actor.set_actor_label(label)
    return actor


def spawn_class(cls, location, rotation, label):
    actor = EAS.spawn_actor_from_class(cls, unreal.Vector(*location), rotation)
    return claim(actor, label) if actor else None


def spawn_mesh(mesh, location, rotation, scale, label, tier=1, collides=True):
    actor = EAS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*location), rotation)
    if not actor:
        return None
    component = actor.static_mesh_component
    component.set_editor_property("static_mesh", mesh)
    component.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    actor.set_actor_scale3d(unreal.Vector(*scale))
    if not collides:
        # Decoration the player must never stand on. Anything spawned off the ground - the
        # frames hanging at a shear line 100 m up - is an invisible platform if it collides.
        # The profile name is the setter that sticks on an editor StaticMeshActor;
        # set_collision_enabled alone is overwritten by the body instance's own profile.
        component.set_collision_profile_name("NoCollision")
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    dress_component(component, tier)
    return claim(actor, label)


def clear_previous():
    """Delete our actors and put the mattes we hid back the way we found them."""
    removed = restored = 0
    for actor in EAS.get_all_level_actors():
        names = tags_of(actor)
        if TAG in names:
            EAS.destroy_actor(actor)
            removed += 1
        elif HIDDEN_TAG in names:
            actor.set_actor_hidden_in_game(False)
            try:
                actor.set_is_temporarily_hidden_in_editor(False)
            except Exception:
                pass
            set_tags(actor, [n for n in names if n != HIDDEN_TAG])
            restored += 1
    REPORT.append("removed %d actors and un-hid %d mattes from a previous run"
                  % (removed, restored))


# --- materials ----------------------------------------------------------------------------
# CitySample names every material slot after the surface it wanted. That name is all that
# survived the migration, and it is enough.
SLOT_BUCKETS = (
    ("glass", "glass"),
    ("brick", "brick"),
    ("paintedmetal", "steel"),
    ("paintedstone", "stone"),
    ("paint", "steel"),
    ("metal", "metal"),
    ("limestone", "stone"),
    ("granite", "stone"),
    ("block", "stone"),
    ("stone", "stone"),
    ("concrete", "stone"),
    ("road", "road"),
    ("asphalt", "road"),
    ("tarmac", "road"),
    ("sidewalk", "road"),
    ("curb", "road"),
)

# Measured BaseTint albedo is in the comment after each entry. The old pass put 0.23 on every
# slot of every building; a skyline of one flat mid-grey is what clipped. Distance darkens.
PALETTE_PATHS = {
    0: {"stone": "MI_Erebus_RuinDark",          # 0.105
        "brick": "MI_Erebus_Concrete_Panels",   # 0.26 - the one accent, kept for variation
        "metal": "MI_Erebus_Metal_Bare",        # 0.175
        "steel": "MI_Erebus_Steel_Dark",        # 0.085
        "glass": "MI_Erebus_Glass_Damaged",     # 0.03, roughness 0.35 - real glass, up close
        "road": "MI_Erebus_RoadAsphalt"},       # 0.016
    1: {"stone": "MI_Erebus_Concrete_Burned",   # 0.06
        "brick": "MI_Erebus_RuinDark",          # 0.105
        "metal": "MI_Erebus_Rust",              # 0.062
        "steel": "MI_Erebus_Steel_Scorched",    # 0.034
        "glass": "MI_Erebus_Steel_Scorched",    # roughness 0.96: a blown-out frame, no glint
        "road": "MI_Erebus_RoadAsphalt"},
    2: {"stone": "MI_Erebus_Concrete_Ground",   # 0.029 - the far ring recedes, not glows
        "brick": "MI_Erebus_Concrete_Burned",   # 0.06
        "metal": "MI_Erebus_Rust",
        "steel": "MI_Erebus_Steel_Scorched",
        "glass": "MI_Erebus_Concrete_Burned",   # far windows read as dark voids, not mirrors
        "road": "MI_Erebus_RoadAsphalt"},
}
PALETTE = {}
SLOT_PLAN = {}      # mesh name -> [bucket or None per slot]
DRESSED = [0, 0]    # slots filled, slots left alone


def build_palette():
    missing = []
    for tier, buckets in PALETTE_PATHS.items():
        PALETTE[tier] = {}
        for bucket, name in buckets.items():
            mi = load("/Game/Ashes/Materials/Instances/" + name)
            if mi is None:
                missing.append(name)
            PALETTE[tier][bucket] = mi
    if missing:
        REPORT.append("MISSING material instances: %s" % ", ".join(sorted(set(missing))))


def bucket_for(slot_name):
    key = str(slot_name).lower()
    for token, bucket in SLOT_BUCKETS:
        if token in key:
            return bucket
    return None


def slot_plan(mesh):
    """Per slot: the bucket to fill, or None where the slot already has real art.

    A slot that ships a material keeps it. That is what preserves the authored rooftops,
    awnings, fire escapes and signs instead of burying them under one concrete.
    """
    name = mesh.get_name()
    plan = SLOT_PLAN.get(name)
    if plan is not None:
        return plan
    plan = []
    try:
        for entry in mesh.get_editor_property("static_materials"):
            interface = entry.get_editor_property("material_interface")
            if interface is not None:
                plan.append(None)        # authored art - leave it
                continue
            plan.append(bucket_for(entry.get_editor_property("material_slot_name")) or "stone")
    except Exception:
        plan = []
    SLOT_PLAN[name] = plan
    return plan


def dress_component(component, tier):
    mesh = component.get_editor_property("static_mesh")
    if not mesh:
        return
    palette = PALETTE.get(tier) or PALETTE.get(1) or {}
    plan = slot_plan(mesh)
    for slot in range(component.get_num_materials()):
        bucket = plan[slot] if slot < len(plan) else "stone"
        if bucket is None or component.get_material(slot) is not None:
            DRESSED[1] += 1
            continue
        material = palette.get(bucket)
        if material:
            component.set_material(slot, material)
            DRESSED[0] += 1


def dress_actor(actor, tier):
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        dress_component(component, tier)


# --- terrain -------------------------------------------------------------------------------
class Terrain(object):
    """Cached ground traces. Every consumer shares one lattice, so a street that re-crosses
    its own neighbourhood costs nothing.

    HitResult exposes nothing through get_editor_property in 5.8 - `blocking_hit` raises
    "Failed to find property". to_dict() is the accessor that works.
    """

    def __init__(self, world, water=()):
        self.world = world
        self.water = list(water)
        self.cache = {}
        self.traces = 0

    def submerged(self, x, y, z):
        for wx, wy, hx, hy, surface in self.water:
            if abs(x - wx) < hx and abs(y - wy) < hy and z < surface + WATER_MARGIN:
                return True
        return False

    def at(self, x, y):
        """(z, normal_z) of real ground under a point, or None for water, void or roof."""
        key = (int(x / SAMPLE_STEP), int(y / SAMPLE_STEP))
        if key in self.cache:
            return self.cache[key]
        px, py = key[0] * SAMPLE_STEP, key[1] * SAMPLE_STEP
        self.traces += 1
        result = None
        # MULTI, not single. A single downward trace stops on the first thing it touches, and
        # the slum covers its own terrain with 1,817 authored meshes - so every sample over a
        # shanty roof reported "no ground" and the whole district refused to seed. Taking the
        # highest NATURAL hit out of the full list finds the terrain under the roofs, and
        # whether a structure stands on it is clear_of()'s job, not the trace's.
        hits = unreal.SystemLibrary.line_trace_multi(
            self.world, unreal.Vector(px, py, 90000.0), unreal.Vector(px, py, -150000.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
        if isinstance(hits, tuple):
            hits = hits[-1]
        for hit in (hits or []):
            data = hit.to_dict()
            actor = data.get("hit_actor")
            if actor is None or has_tag(actor):
                continue
            label = actor.get_actor_label().lower()
            if not any(t.lower() in label for t in NATURAL_GROUND):
                continue
            normal = data.get("impact_normal") or data.get("normal")
            ground_z = data["location"].z
            if self.submerged(px, py, ground_z):
                break        # the lake bed is not buildable ground
            result = (ground_z, normal.z if normal else 0.0)
            break
        self.cache[key] = result
        return result

    def exact(self, x, y):
        """Ground z at the PRECISE point, uncached.

        at() snaps to a SAMPLE_STEP lattice and returns that cell's height, which is fine for
        searching but wrong for seating: on a 20% slope a 3.5 m lateral snap is 70 cm of error,
        and that is a road tile hanging in the air. Anything the player can stand on is seated
        with this instead.
        """
        hits = unreal.SystemLibrary.line_trace_multi(
            self.world, unreal.Vector(x, y, 90000.0), unreal.Vector(x, y, -150000.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
        if isinstance(hits, tuple):
            hits = hits[-1]
        for hit in (hits or []):
            data = hit.to_dict()
            actor = data.get("hit_actor")
            if actor is None or has_tag(actor):
                continue
            label = actor.get_actor_label().lower()
            if not any(t.lower() in label for t in NATURAL_GROUND):
                continue
            z = data["location"].z
            return None if self.submerged(x, y, z) else z
        return None

    def lowest(self, x, y, radius):
        """Lowest ground under a footprint of this radius, or None.

        A loose piece seated on its CENTRE hangs its downhill corners over the slope - that is
        an 8 x 13 m broken facade with 11 m of air under one end, and a player can stand on it.
        Seating on the lowest sample buries the uphill side instead, which is what rubble does.
        """
        best = self.exact(x, y)
        if best is None:
            return None
        for dx, dy in ((radius, 0.0), (-radius, 0.0), (0.0, radius), (0.0, -radius),
                       (radius * 0.7, radius * 0.7), (-radius * 0.7, -radius * 0.7)):
            z = self.exact(x + dx, y + dy)
            if z is not None and z < best:
                best = z
        return best

    def flat_at(self, x, y, min_normal_z=MIN_NORMAL_Z):
        got = self.at(x, y)
        return got if got and got[1] >= min_normal_z else None

    def footprint(self, x, y, half_x, half_y, yaw, why=None, height=None):
        """Sample a real footprint, not just its centre.

        Returns (low, base, relief) or None. base is the HIGHEST sample - the building stands
        at grade on the high side of the plot - and low is the lowest, which is how deep the
        podium under it has to reach. relief is the difference, and it is what gets rejected,
        so the uphill wall does not swallow two floors.
        """
        def refuse(reason):
            if why is not None:
                why[reason] = why.get(reason, 0) + 1
            return None

        if self.flat_at(x, y) is None:
            return refuse("centre")  # the plot centre itself must be flat natural ground
        radians = math.radians(yaw)
        cos_a, sin_a = math.cos(radians), math.sin(radians)
        zs, failures = [], 0
        for u in (-1.0, 0.0, 1.0):
            for v in (-1.0, 0.0, 1.0):
                lx, ly = u * half_x, v * half_y
                px = x + lx * cos_a - ly * sin_a
                py = y + lx * sin_a + ly * cos_a
                got = self.at(px, py)
                if got is None:
                    # No natural ground here at all: over the edge of the world, or the lake.
                    # This is NOT a slope failure to be budgeted against - it is the strongest
                    # evidence the footprint is not on the ground, so it refuses outright.
                    return refuse("void")
                if got[1] < EDGE_NORMAL_Z:
                    failures += 1    # a boulder or a steep step; a podium can absorb these
                    if failures > EDGE_FAILURES:
                        return refuse("edges")
                    continue
                zs.append(got[0])
        # Those nine come from at(), which answers from a SAMPLE_STEP (5 m) lattice - so a
        # corner two metres past the lip is answered by a cell three metres inland and the
        # "no ground here" refusal never fires. The four corners are exactly what the podium
        # has to reach, so they are re-taken at their real positions. Only a candidate that
        # already passed the cheap cached pass pays for these traces.
        for u in (-1.0, 1.0):
            for v in (-1.0, 1.0):
                lx, ly = u * half_x, v * half_y
                z = self.exact(x + lx * cos_a - ly * sin_a, y + lx * sin_a + ly * cos_a)
                if z is None:
                    return refuse("void")
                zs.append(z)
        relief = max(zs) - min(zs)
        # A big building is allowed a proportionally bigger cut, the way a real hillside block
        # sits on a podium. A flat cap keeps a tower from being buried to its third floor.
        limit = min(MAX_RELIEF, max(600.0, 0.16 * math.hypot(half_x * 2.0, half_y * 2.0)))
        # The base is the lowest corner, so relief IS how deep the uphill side gets buried.
        # Budgeting it by footprint alone buried a 9 m row house to its roof; cap it against
        # the building's own height so nothing loses more than a quarter of itself to the hill.
        if height:
            limit = min(limit, height * MAX_BURIAL_FRACTION)
        # A plot is only acceptable if a podium can actually fill it. Without this the tallest
        # steps got no plinth - the building still stood on the high corner and its downhill
        # side hung in the air, which is the exact failure this was meant to remove.
        limit = min(limit, PODIUM_MAX - PODIUM_SKIRT)
        if relief > limit:
            return refuse("relief")
        return (min(zs), max(zs), relief)


# --- what is already there -----------------------------------------------------------------
# Lights, sound, cameras, emitters, captures and the landscape itself are not obstacles.
SKIP_OBSTACLE_CLASSES = frozenset((
    "PointLight", "SpotLight", "DirectionalLight", "SkyLight", "RectLight",
    "AmbientSound", "CameraActor", "CineCameraActor", "Emitter",
    "SphereReflectionCapture", "BoxReflectionCapture", "PlanarReflection",
    "Landscape", "LandscapeStreamingProxy", "PrecomputedVisibilityVolume",
    "PostProcessVolume", "ExponentialHeightFog", "CullDistanceVolume",
    "NavMeshBoundsVolume", "RecastNavMesh", "SkyAtmosphere", "VolumetricCloud",
))


def water_zones():
    """(x, y, half_x, half_y, surface_z) for every body of water in the map."""
    zones = []
    for actor in EAS.get_all_level_actors():
        if has_tag(actor):
            continue
        label = actor.get_actor_label().lower()
        if not any(t in label for t in WATER_LABELS):
            continue
        try:
            origin, extent = actor.get_actor_bounds(False)
        except Exception:
            continue
        zones.append((origin.x, origin.y, abs(extent.x), abs(extent.y), origin.z + extent.z))
    if zones:
        REPORT.append("water: %d bodies, highest surface %.0f m"
                      % (len(zones), max(z[4] for z in zones) / 100.0))
    else:
        REPORT.append("water: none found - nothing will be rejected as submerged")
    return zones


def existing_obstacles():
    """(x, y, radius) for EVERY authored actor, plus the trimmed core bounds.

    The old pass only looked at StaticMeshActor. This map also holds 135 BlockingVolumes, 12
    GroupActors, 8 Brushes and 22 plain Actors - all of them things a 60 m building must not
    be dropped on top of. get_actor_bounds covers every class in one call.
    """
    spots = []
    for actor in EAS.get_all_level_actors():
        if has_tag(actor):
            continue
        if type(actor).__name__ in SKIP_OBSTACLE_CLASSES:
            continue
        try:
            origin, extent = actor.get_actor_bounds(False)
        except Exception:
            continue
        half_x, half_y = abs(extent.x), abs(extent.y)
        if max(half_x, half_y) <= 1.0 or max(half_x, half_y) > BACKDROP_RADIUS:
            continue
        spots.append((origin.x, origin.y,
                      min(half_x, OBSTACLE_RADIUS_CAP), min(half_y, OBSTACLE_RADIUS_CAP)))
    xs = sorted(s[0] for s in spots)
    ys = sorted(s[1] for s in spots)

    def pct(values, p):
        return values[max(0, min(len(values) - 1, int(len(values) * p)))]

    # A handful of backdrop actors sit kilometres out; the 2-98 band is the authored city.
    bounds = (pct(xs, 0.02), pct(ys, 0.02), pct(xs, 0.98), pct(ys, 0.98))
    return spots, bounds


def clear_of(x, y, margin, spots, half_x=0.0, half_y=0.0):
    """Inflated axis-aligned box-vs-box test.

    A circumscribed radius is what strangled the first attempt: a 200 x 5 m authored wall
    became a 100 m no-build disc, and 1,974 of those blanketed every buildable slope. World
    bounds are already axis-aligned, so testing the box costs the same and is honest.

    half_x/half_y are the extents of the thing being placed. Leaving them at zero tests only
    its centre, which let a 125 m wide building straddle a neighbour whose box its centre
    happened to miss - 13 intersecting pairs in the audit.
    """
    for sx, sy, hx, hy in spots:
        if abs(x - sx) < hx + half_x + margin and abs(y - sy) < hy + half_y + margin:
            return False
    return True


def world_half(half_x, half_y, yaw):
    """Axis-aligned half extents of a box that has been turned by yaw.

    clear_of() is an axis-aligned test and the pool's extents are measured at zero yaw, but a
    building is turned to its street's heading - so the box being tested was not the box being
    placed. The 92 x 14 m row house at 45 deg spans 75 x 75 m on world axes, and testing it as
    92 x 14 let it lie diagonally through its neighbour while clear_of() reported it clear.
    """
    radians = math.radians(yaw)
    cos_a, sin_a = abs(math.cos(radians)), abs(math.sin(radians))
    return (cos_a * half_x + sin_a * half_y, sin_a * half_x + cos_a * half_y)


def solid_ground(world, x, y):
    """Is there anything here to stand on that this pass did not build itself?

    The fences run AFTER the roads and podiums are laid, so a plain downward trace fired at a
    road slab that overhangs the void hits the slab and reports solid ground - which is exactly
    the hole the fence exists to close, and the same trap for a BlockingVolume the first fence
    pass already placed. Our own actors are skipped; authored geometry counts as ground.
    """
    hits = unreal.SystemLibrary.line_trace_multi(
        world, unreal.Vector(x, y, 150000.0), unreal.Vector(x, y, -200000.0),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if isinstance(hits, tuple):
        hits = hits[-1]
    for hit in (hits or []):
        actor = hit.to_dict().get("hit_actor")
        if actor is not None and not has_tag(actor):
            return True
    return False


def road_cell(x, y):
    """The street lattice. Shared by street growth and by the paving test for a lot."""
    return (int(x / ROAD_STEP), int(y / ROAD_STEP))


# --- the building pool ---------------------------------------------------------------------
# Measured exclusions. Only two names are excluded outright; everything else is judged on its
# measured size. "_Level01" used to be on this list because those blueprints reported 20,008 m
# tall bounds - but that was a measurement bug, not a bad asset: several of these kits carry a
# component pinned at world origin, so bounds taken from an actor parked 60 km up span the
# whole 60 km. Measuring at the origin gives real numbers, and the "_Level01" ground-floor
# variants turn out to be exactly the small infill this district was missing.
#   *setDressing   - 2.8 m of street furniture, not a building
#   FWY_Clover_*   - freeway cloverleaf interchanges; they were being placed as towers
EXCLUDE_TOKENS = ("setdressing", "fwy_")
MIN_FOOTPRINT = 500.0
MIN_HEIGHT = 800.0
MAX_HEIGHT = 45000.0


def pool_rules():
    """Fingerprint of the classification, so the cache invalidates when the rules move."""
    return [list(EXCLUDE_TOKENS), MIN_FOOTPRINT, MIN_HEIGHT, MAX_HEIGHT]


def measure_pool():
    """Spawn every candidate once, measure it, classify it. Cached - it costs a minute."""
    cache = saved_path(POOL_CACHE)
    if not REPOOL and os.path.isfile(cache):
        try:
            with open(cache) as handle:
                data = json.load(handle)
            if data.get("version") == 2 and data.get("rules") == pool_rules():
                REPORT.append("building pool from cache: %d usable" % len(data["entries"]))
                return data["entries"]
        except Exception:
            pass

    AR.scan_paths_synchronous(list(BUILDING_DIRS), True)
    # Spawn AT the origin: a stray component pinned to world zero then adds nothing to the
    # bounds instead of stretching them from here to wherever the actor was parked.
    park = unreal.Vector(0.0, 0.0, 0.0)
    entries, rejected = [], {}
    catalogue = []
    for directory in BUILDING_DIRS:
        catalogue += list(AR.get_assets_by_path(directory, recursive=True))
    for asset in catalogue:
        if str(asset.asset_class_path.asset_name) != "Blueprint":
            continue
        package = str(asset.package_name)
        short = package.split("/")[-1]
        low = short.lower()
        token = next((t for t in EXCLUDE_TOKENS if t in low), None)
        if token:
            rejected[token] = rejected.get(token, 0) + 1
            continue
        blueprint = unreal.load_asset(package)
        generated = blueprint.generated_class() if blueprint else None
        if not generated:
            continue
        actor = EAS.spawn_actor_from_class(generated, park, unreal.Rotator())
        if not actor:
            continue
        try:
            origin, extent = actor.get_actor_bounds(False)
            ex, ey, ez = abs(extent.x), abs(extent.y), abs(extent.z)
            # A component still stuck at world zero shows up as bounds centred far from the
            # spawn point. Reject rather than place a building with phantom extents.
            if max(abs(origin.x), abs(origin.y)) > max(ex, ey) + 1000.0:
                rejected["offset_bounds"] = rejected.get("offset_bounds", 0) + 1
                EAS.destroy_actor(actor)
                continue
            if min(ex, ey) < MIN_FOOTPRINT:
                rejected["degenerate"] = rejected.get("degenerate", 0) + 1
            elif not (MIN_HEIGHT <= ez * 2.0 <= MAX_HEIGHT):
                rejected["height"] = rejected.get("height", 0) + 1
            else:
                height = ez * 2.0
                entries.append({
                    "pkg": package, "name": short,
                    "half": [ex, ey], "height": height,
                    "kind": "tower" if height >= 8000.0 else (
                        "mid" if height >= 2500.0 else "low"),
                    "area": ex * ey,
                })
        except Exception as exc:
            REPORT.append("pool measure failed on %s: %s" % (short, exc))
        EAS.destroy_actor(actor)

    entries.sort(key=lambda e: -e["height"])
    try:
        with open(cache, "w") as handle:
            json.dump({"version": 2, "rules": pool_rules(), "entries": entries},
                      handle, indent=1)
    except Exception:
        pass
    REPORT.append("building pool: %d usable (%s)"
                  % (len(entries), ", ".join("%s x%d" % kv for kv in sorted(rejected.items()))))
    return entries


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


# --- streets first, buildings second -------------------------------------------------------
def seed_cells(terrain, authored, centre, half):
    """Scan for open ground to start streets from.

    The blind ellipse this replaced fired half its seeds into void: the landscape is centred
    at (1098, 320) m while the slum sits at (488, -81) m, so a symmetric ring around the slum
    aims most of its spokes off the edge of the world. Scanning finds where the ground is.
    """
    found, lattice, order = [], set(), []
    steps = int(SEED_SCAN_REACH / SEED_SCAN_STEP)
    for ix in range(-steps, steps + 1):
        for iy in range(-steps, steps + 1):
            x = centre[0] + ix * SEED_SCAN_STEP
            y = centre[1] + iy * SEED_SCAN_STEP
            # Outside the authored footprint, measured as an ellipse around it.
            ex = (x - centre[0]) / (half[0] + 6000.0)
            ey = (y - centre[1]) / (half[1] + 6000.0)
            if ex * ex + ey * ey < 1.0:
                continue
            if terrain.flat_at(x, y, STREET_NORMAL_Z) is None:
                continue
            if not clear_of(x, y, ROAD_WIDTH * 0.6, authored):
                continue
            found.append((x, y))
            order.append((ix, iy))
            lattice.add((ix, iy))
    # A seed in a rocky notch dies in two steps. Require open ground all around it.
    open_enough = [(x, y) for (x, y), (ix, iy) in zip(found, list(order))
                   if sum(1 for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                          if (dx or dy) and (ix + dx, iy + dy) in lattice)
                   >= SEED_MIN_OPEN_NEIGHBOURS]
    # Nearest first, so the district grows out of the slum edge rather than starting in a field.
    open_enough.sort(key=lambda p: (p[0] - centre[0]) ** 2 + (p[1] - centre[1]) ** 2)
    seeds = []
    for point in open_enough:
        if all((point[0] - q[0]) ** 2 + (point[1] - q[1]) ** 2 >= SEED_SPACING * SEED_SPACING
               for q in seeds):
            seeds.append(point)
        if len(seeds) >= STREET_SEEDS:
            break
    REPORT.append("scanned %d open cells, %d with open surroundings, seeded %d streets"
                  % (len(found), len(open_enough), len(seeds)))
    return seeds


def grow_streets(terrain, authored, centre, half, rng):
    """Grow a street network outward from the slum, following the terrain.

    Each step picks the heading that costs the least height out of a wide fan, so a street
    contours around a slope instead of dying against it. This is the inversion the old pass
    needed: it placed buildings first and then fired straight spurs at the slum, which is why
    the result read as scattered towers with roads pointing at them. Here the streets are the
    skeleton and the buildings are frontage on them.
    """
    occupied = set()
    stops = {}

    def grow(x, y, heading, budget):
        line, taken = [(x, y)], set()
        for _step in range(budget):
            here = terrain.at(x, y)
            if here is None:
                break
            best = None
            for turn in STREET_TURNS:
                candidate = heading + turn
                radians = math.radians(candidate)
                nx = x + math.cos(radians) * ROAD_STEP
                ny = y + math.sin(radians) * ROAD_STEP
                key = road_cell(nx, ny)
                if key in occupied or key in taken:
                    stops["occupied"] = stops.get("occupied", 0) + 1
                    continue
                there = terrain.flat_at(nx, ny, STREET_NORMAL_Z)
                if there is None:
                    stops["no_ground"] = stops.get("no_ground", 0) + 1
                    continue
                rise = abs(there[0] - here[0])
                if rise > STREET_MAX_RISE:
                    stops["too_steep"] = stops.get("too_steep", 0) + 1
                    continue
                if not clear_of(nx, ny, ROAD_WIDTH * 0.6, authored):
                    stops["blocked"] = stops.get("blocked", 0) + 1
                    continue
                cost = (rise + abs(turn) * STREET_TURN_COST
                        - (STREET_STRAIGHT_BONUS if turn == 0.0 else 0.0))
                if best is None or cost < best[0]:
                    best = (cost, nx, ny, candidate)
            if best is None:
                break
            _cost, x, y, heading = best
            taken.add(road_cell(x, y))
            line.append((x, y))
        return line, taken, heading

    streets = []
    queue = []
    for sx, sy in seed_cells(terrain, authored, centre, half):
        radial = math.degrees(math.atan2(sy - centre[1], sx - centre[0]))
        queue.append((sx, sy, radial, STREET_MAX_STEPS, 0, True))

    guard = 0
    while queue and guard < 120:
        guard += 1
        x, y, heading, budget, depth, trial = queue.pop(0)
        # A seed aimed straight at a hill dies; the same seed along the contour does not.
        headings = ([heading + t for t in STREET_TRIAL_HEADINGS] if trial else [heading])
        best_line = best_taken = None
        for candidate in headings:
            line, taken, _end = grow(x, y, candidate, budget)
            if best_line is None or len(line) > len(best_line):
                best_line, best_taken = line, taken
        if not best_line or len(best_line) < STREET_MIN_STEPS:
            continue
        occupied.update(best_taken)
        streets.append(best_line)
        if depth < 2:
            # Cross-streets off the spine give the grid its junctions.
            for index in range(2, len(best_line) - 1):
                if rng.random() >= STREET_BRANCH_CHANCE:
                    continue
                ax, ay = best_line[index - 1]
                bx, by = best_line[index]
                spine = math.degrees(math.atan2(by - ay, bx - ax))
                queue.append((bx, by, spine + rng.choice((90.0, -90.0)),
                              max(8, budget // 2), depth + 1, False))
    REPORT.append("grew %d streets, %d segments, %d ground traces; refused steps: %s"
                  % (len(streets), sum(len(s) - 1 for s in streets), terrain.traces,
                     ", ".join("%s %d" % kv for kv in sorted(stops.items())) or "none"))
    return streets


def rotate_local(rot, x, y, z):
    """UE's FRotationMatrix applied to a local vector. rotate_vector is not exposed in 5.8."""
    pitch, yaw, roll = math.radians(rot.pitch), math.radians(rot.yaw), math.radians(rot.roll)
    cp, sp = math.cos(pitch), math.sin(pitch)
    cy, sy = math.cos(yaw), math.sin(yaw)
    cr, sr = math.cos(roll), math.sin(roll)
    ax = (cp * cy, cp * sy, sp)
    ay = (sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp)
    az = (-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)
    return (x * ax[0] + y * ay[0] + z * az[0],
            x * ax[1] + y * ay[1] + z * az[1],
            x * ax[2] + y * ay[2] + z * az[2])


def bed_tile(terrain, actor):
    """Push a laid tile down until no corner floats. Returns how deep the worst corner ends up.

    Fitting a plane through four traced corners is not enough on its own: the tile is then
    rotated about a pivot 10 m outside itself, so where its corners finally land is not where
    the fit sampled. Rather than re-derive that through the full rotation, the tile is measured
    where it actually is and dropped until the highest corner rests on the hill. Whatever is
    left is a corner cut INTO the ground, which is what a road on a slope should do.
    """
    try:
        origin, _extent = actor.get_actor_bounds(False)
    except Exception:
        return None
    rotation = actor.get_actor_rotation()
    gaps = []
    for u in (-1.0, 1.0):
        for v in (-1.0, 1.0):
            ox, oy, oz = rotate_local(rotation, u * SLAB_LENGTH * 0.5,
                                      v * ROAD_WIDTH * 0.5, 0.0)
            ground = terrain.exact(origin.x + ox, origin.y + oy)
            if ground is None:
                return None
            gaps.append((origin.z + oz) - ground)
    lift = max(gaps) + ROAD_BED
    location = actor.get_actor_location()
    actor.set_actor_location(
        unreal.Vector(location.x, location.y, location.z - lift), False, False)
    return max(gaps) - min(gaps) + ROAD_BED   # how deep the far corner is now cut in


def seat_tile(actor, x, y, z):
    """Shift an actor until its bounds centre is exactly where it was asked to be."""
    try:
        origin, _extent = actor.get_actor_bounds(False)
    except Exception:
        return
    location = actor.get_actor_location()
    actor.set_actor_location(
        unreal.Vector(location.x + (x - origin.x),
                      location.y + (y - origin.y),
                      location.z + (z - origin.z)), False, False)


def configure_road():
    """Take the slab's real dimensions from the mesh instead of trusting constants.

    ROAD_STEP stays the street-growth step (20 m) - it shapes the network. SLAB_LENGTH and
    ROAD_WIDTH describe the piece actually laid, and the lot setback keys off ROAD_WIDTH so
    buildings front a 4.5 m lane rather than a boulevard that is not there.
    """
    global SLAB_LENGTH, ROAD_WIDTH, ROAD_ORIGIN_Y
    mesh = load(ROAD_TILE)
    if not mesh:
        REPORT.append("MISSING " + ROAD_TILE)
        return None
    bounds = mesh.get_bounds()
    SLAB_LENGTH = abs(bounds.box_extent.x) * 2.0 * SLAB_SCALE
    ROAD_WIDTH = abs(bounds.box_extent.y) * 2.0 * SLAB_SCALE
    ROAD_ORIGIN_Y = bounds.origin.y * SLAB_SCALE
    REPORT.append("road slab %s at %.1fx: %.0f x %.0f cm"
                  % (ROAD_TILE.split("/")[-1], SLAB_SCALE, SLAB_LENGTH, ROAD_WIDTH))
    return mesh


def lay_streets(terrain, streets):
    """Lay a path of small cracked slabs along every street, each seated on its own ground.

    Every slab is bedded until no corner of it floats - that is the invariant, because a
    walkable surface hanging over the hill is a platform the player can stand on in mid air.
    Slabs that cannot be seated are simply not laid, and the gaps read as a road broken by the
    same war that broke the buildings.

    Returns {street cell: slabs laid there}, because the refusals are not rare - most of a
    planned street can end up with no carriageway at all, and place_frontage() must line the
    road that exists rather than the one grow_streets() drew.
    """
    mesh = load(ROAD_TILE)
    if not mesh:
        return {}
    half_l, half_w = SLAB_LENGTH * 0.5, ROAD_WIDTH * 0.5
    pitch_along = SLAB_LENGTH * SLAB_OVERLAP
    paving = {}
    laid = no_ground = twisted = steep = buried = 0
    for index, line in enumerate(streets):
        for step in range(len(line) - 1):
            if laid >= ROAD_MAX_TILES:
                break
            (ax, ay), (bx, by) = line[step], line[step + 1]
            run = math.hypot(bx - ax, by - ay)
            if run <= 0.0:
                continue
            yaw = math.degrees(math.atan2(by - ay, bx - ax))
            radians = math.radians(yaw)
            cos_a, sin_a = math.cos(radians), math.sin(radians)
            ux, uy = (bx - ax) / run, (by - ay) / run

            travelled = pitch_along * 0.5
            slab = 0
            while travelled < run and laid < ROAD_MAX_TILES:
                # Cancel the mesh's own bounds offset, then sample where the slab really goes.
                along = travelled
                centre_x = ax + ux * along + sin_a * ROAD_ORIGIN_Y
                centre_y = ay + uy * along - cos_a * ROAD_ORIGIN_Y
                travelled += pitch_along
                slab += 1

                corners, missing = {}, False
                for u in (-1.0, 1.0):
                    for v in (-1.0, 1.0):
                        lx, ly = u * half_l, v * half_w
                        z = terrain.exact(centre_x + lx * cos_a - ly * sin_a,
                                          centre_y + lx * sin_a + ly * cos_a)
                        if z is None:
                            missing = True
                            break
                        corners[(u, v)] = z
                    if missing:
                        break
                if missing:
                    no_ground += 1
                    continue

                middle = sum(corners.values()) / 4.0
                slope_x = ((corners[(1.0, -1.0)] + corners[(1.0, 1.0)])
                           - (corners[(-1.0, -1.0)] + corners[(-1.0, 1.0)])) / (2.0 * SLAB_LENGTH)
                slope_y = ((corners[(-1.0, 1.0)] + corners[(1.0, 1.0)])
                           - (corners[(-1.0, -1.0)] + corners[(1.0, -1.0)])) / (2.0 * ROAD_WIDTH)
                residual = max(abs(corners[(u, v)] - (middle + slope_x * u * half_l
                                                      + slope_y * v * half_w))
                               for (u, v) in corners)
                if residual > ROAD_MAX_RESIDUAL:
                    twisted += 1
                    continue

                tilt_pitch = -math.degrees(math.atan(slope_x))
                tilt_roll = math.degrees(math.atan(slope_y))
                if abs(tilt_pitch) > ROAD_MAX_TILT or abs(tilt_roll) > ROAD_MAX_TILT:
                    steep += 1
                    continue

                tile = spawn_mesh(mesh, (centre_x, centre_y, middle),
                                  unreal.Rotator(pitch=tilt_pitch, yaw=yaw, roll=tilt_roll),
                                  (SLAB_SCALE, SLAB_SCALE, SLAB_SCALE),
                                  "Road_%02d_%03d_%02d" % (index, step, slab), tier=1)
                if tile is None:
                    continue
                seat_tile(tile, centre_x, centre_y, middle)
                cut = bed_tile(terrain, tile)
                if cut is None or cut > ROAD_MAX_BURY:
                    EAS.destroy_actor(tile)
                    buried += 1
                    continue
                # Keyed on the CENTRELINE, not the slab: the slab is offset perpendicular by
                # the mesh's own bounds origin, which is enough to land it in the next cell, and
                # the lot test that reads this samples the centreline.
                key = road_cell(ax + ux * along, ay + uy * along)
                paving[key] = paving.get(key, 0) + 1
                laid += 1
    REPORT.append("laid %d road slabs (refused: %d no ground, %d twisted, %d too steep, "
                  "%d would bury an edge)" % (laid, no_ground, twisted, steep, buried))
    REPORT.append("%d street cells carry %d+ slabs and can take frontage; %d took at least one"
                  % (sum(1 for n in paving.values() if n >= FRONT_MIN_SLABS), FRONT_MIN_SLABS,
                     len(paving)))
    return paving


def component_span(actor, component):
    """(low_z, high_z) of a mesh component in world space, or None if it cannot be measured.

    Rotation is ignored: this is only ever compared against a cut with a SHEAR_RAGGED (18 m)
    margin, which is wider than the error a turned panel can introduce.
    """
    try:
        mesh = component.static_mesh
        if mesh is None:
            return None
        bounds = mesh.get_bounds()
        location = component.get_world_location()
        scale = abs(component.get_world_scale().z)
    except Exception:
        try:
            location = actor.get_actor_location()
            return (location.z, location.z)   # unmeasurable: treat as at the base, keep it
        except Exception:
            return None
    centre = location.z + bounds.origin.z * scale
    half = abs(bounds.box_extent.z) * scale
    return (centre - half, centre + half)


def shear_solids(actor, cut):
    """Shear the components that are NOT instanced, and count whatever is left standing.

    Removing ISM instances is most of the job but not all of it: get_components_by_class
    (StaticMeshComponent) returned 121 of 121 as ISM on both probed towers, so on those there
    is nothing else - but that is 2 of 46 pool members, and one plain facade component left
    intact above a shear line is an unbroken tower with its floors missing underneath. Anything
    wholly above the break loses its visibility and its collision; the rest is tallied into the
    report so the audit is a number in Saved/ rather than an assumption.
    """
    for component in actor.get_components_by_class(unreal.StaticMeshComponent):
        if isinstance(component, unreal.InstancedStaticMeshComponent):
            continue
        span = component_span(actor, component)
        if span is None or span[0] <= cut + SHEAR_RAGGED:
            SOLIDS[1] += 1
            continue
        try:
            component.set_visibility(False, True)
            component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            SOLIDS[0] += 1
        except Exception as exc:
            SOLIDS[1] += 1
            REPORT.append("could not shear a plain component: %s" % exc)


def shear(actor, kind, rng):
    """Take the top off, and collapse a vertical bay of facade.

    Instances are removed, not hidden, so the silhouette really changes. Verified safe: an ISM
    on a spawned packed level actor is per-actor data - removing instances on one placement
    leaves the source blueprint and every other placement untouched (probe: bp_isolated True).
    """
    if rng.random() > SHEAR_CHANCE.get(kind, 0.5):
        return 0, None
    try:
        bounds_origin, bounds_extent = actor.get_actor_bounds(False)
    except Exception:
        return 0, None
    base_z = bounds_origin.z - bounds_extent.z
    height = bounds_extent.z * 2.0
    if height <= 0.0:
        return 0, None
    keep = rng.uniform(*SHEAR_KEEP)
    cut = base_z + height * keep
    bay = rng.random() < BAY_CHANCE
    bay_from = rng.uniform(0.0, 2.0 * math.pi)
    bay_arc = rng.uniform(*BAY_ARC) * 2.0 * math.pi
    bay_top = base_z + height * rng.uniform(0.25, keep)
    origin = actor.get_actor_location()

    removed = 0
    for component in actor.get_components_by_class(unreal.InstancedStaticMeshComponent):
        count = component.get_instance_count()
        if count <= 0:
            continue
        drop = []
        for index in range(count):
            point = component.get_instance_transform(index, True).translation
            if point.z > cut + SHEAR_RAGGED:
                drop.append(index)
                continue
            if point.z > cut - SHEAR_RAGGED:
                # Ragged edge: survival falls off through the band, so the break is not a
                # straight saw cut across every wall at the same height.
                if rng.random() < (point.z - cut + SHEAR_RAGGED) / (2.0 * SHEAR_RAGGED):
                    drop.append(index)
                    continue
            if bay and point.z < bay_top:
                angle = (math.atan2(point.y - origin.y, point.x - origin.x)
                         - bay_from + 4.0 * math.pi) % (2.0 * math.pi)
                if angle < bay_arc:
                    drop.append(index)
        if drop:
            try:
                component.remove_instances(drop)
                removed += len(drop)
            except Exception as exc:
                REPORT.append("shear failed: %s" % exc)
    shear_solids(actor, cut)
    return removed, cut


def ensure_seated(terrain, actor, limit=SEAT_MAX_AIR):
    """Delete anything that ended up with clear air beneath it. Measured, not assumed.

    Seating maths gets a piece close, but a large mesh dropped near a lip can still overhang.
    This is the backstop that makes "nothing the player can stand on floats" a property of the
    level rather than a hope: measure what is actually under the thing, and if it is hanging,
    it does not ship.

    A sample that hits NOTHING is the worst case, not an excuse. It used to be skipped as "off
    the edge of the landscape, not a floating slab" - backwards: a corner with the void under it
    is unsupported by definition, which is how rubble kept surviving with one end over the drop.
    """
    if actor is None:
        return None
    try:
        origin, extent = actor.get_actor_bounds(False)
    except Exception:
        return actor
    under = origin.z - extent.z
    start = origin.z + extent.z + 100.0
    worst = None
    for dx, dy in ((0.0, 0.0), (-0.6, -0.6), (0.6, -0.6), (-0.6, 0.6), (0.6, 0.6)):
        hit = unreal.SystemLibrary.line_trace_single(
            terrain.world, unreal.Vector(origin.x + extent.x * dx, origin.y + extent.y * dy, start),
            unreal.Vector(origin.x + extent.x * dx, origin.y + extent.y * dy, start - 300000.0),
            unreal.TraceTypeQuery.ECC_VISIBILITY, True, [actor], unreal.DrawDebugTrace.NONE, True)
        data = hit.to_dict() if hit else None
        if not data or not data.get("blocking_hit"):
            worst = float("inf")           # nothing at all under this corner: it is over void
            break
        air = under - data["location"].z
        if worst is None or air > worst:
            worst = air
    if worst is not None and worst > limit:
        EAS.destroy_actor(actor)
        CULLED[0] += 1
        return None
    return actor


def bed_in(mesh, scale):
    """How far to sink a loose piece so it sits IN the ground instead of balancing on it."""
    try:
        return abs(mesh.get_bounds().box_extent.z) * scale * 0.30
    except Exception:
        return 0.0


def footprint_radius(mesh, scale):
    try:
        extent = mesh.get_bounds().box_extent
        return max(abs(extent.x), abs(extent.y)) * scale
    except Exception:
        return 200.0


def cap_break(terrain, x, y, cut_z, half_x, half_y, tier, rng, caps, label):
    """Exposed frame at the shear line and collapse debris at the foot of it."""
    made = 0
    for index, mesh in enumerate(caps.get("frame", [])[:2]):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        px = x + math.cos(angle) * half_x * 0.55
        py = y + math.sin(angle) * half_y * 0.55
        if spawn_mesh(mesh, (px, py, cut_z - rng.uniform(200.0, 900.0)),
                      unreal.Rotator(pitch=rng.uniform(-14.0, 14.0),
                                     yaw=math.degrees(angle) + 90.0,
                                     roll=rng.uniform(-10.0, 10.0)),
                      (1.0, 1.0, 1.0), "%s_Frame%d" % (label, index), tier,
                      collides=False):
            made += 1
    for index, mesh in enumerate(caps.get("collapse", [])):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        distance = max(half_x, half_y) * rng.uniform(0.8, 1.5)
        px = x + math.cos(angle) * distance
        py = y + math.sin(angle) * distance
        scale = rng.uniform(1.0, 2.4)
        ground = terrain.lowest(px, py, footprint_radius(mesh, scale))
        if ground is None:
            continue
        if ensure_seated(terrain, spawn_mesh(
                mesh, (px, py, ground - bed_in(mesh, scale)),
                unreal.Rotator(pitch=rng.uniform(-8.0, 8.0),
                               yaw=rng.uniform(0.0, 360.0),
                               roll=rng.uniform(-8.0, 8.0)),
                (scale, scale, scale), "%s_Collapse%d" % (label, index), tier)):
            made += 1
    return made


# --- the distance mattes -------------------------------------------------------------------
def hide_mattes(mode):
    """Hide the painted skyline, reversibly.

    The map's backdrop is 13 matte cards 1.7-5.9 km out and up to 1.29 km high, and 9 of them
    use M_LV_SoulDistanceBD001_Unlit. An unlit material ignores the scene's exposure entirely,
    so those cards render at full texture brightness and clip - that is the continuous glowing
    band behind the towers, and no amount of exposure tuning touches it. The 3 lit cards do
    respond to the map's lighting and sit correctly, so the default only hides the unlit ones.
    Nothing is modified: each card is tagged, and clear_previous() puts it back.
    """
    if mode == "keep":
        REPORT.append("mattes kept (AH_CITY_MATTE=keep)")
        return
    hidden = kept = 0
    for actor in EAS.get_all_level_actors():
        if has_tag(actor):
            continue
        label = actor.get_actor_label()
        mesh_name = ""
        if isinstance(actor, unreal.StaticMeshActor):
            mesh = actor.static_mesh_component.get_editor_property("static_mesh")
            mesh_name = mesh.get_name() if mesh else ""
        if "matte" not in (label + mesh_name).lower():
            continue
        if mode == "hide-unlit":
            unlit = False
            for component in actor.get_components_by_class(unreal.StaticMeshComponent):
                for slot in range(max(1, component.get_num_materials())):
                    material = component.get_material(slot)
                    if material and "unlit" in material.get_name().lower():
                        unlit = True
            if not unlit:
                kept += 1
                continue
        actor.set_actor_hidden_in_game(True)
        try:
            actor.set_is_temporarily_hidden_in_editor(True)
        except Exception:
            pass
        names = tags_of(actor)
        if HIDDEN_TAG not in names:
            names.append(HIDDEN_TAG)
            set_tags(actor, names)
        hidden += 1
    REPORT.append("hid %d distance mattes (%s), left %d lit ones" % (hidden, mode, kept))


# --- housekeeping --------------------------------------------------------------------------
def apply_culling():
    """No HLOD: `unreal.HierarchicalLODUtilities` does not exist in this build's python API.

    Distance culling on the scatter is the part that is scriptable, and it is where the actor
    count actually hurts - the buildings are packed level actors with their own instancing.
    """
    culled = scatter = 0
    for actor in EAS.get_all_level_actors():
        if not has_tag(actor) or not isinstance(actor, unreal.StaticMeshActor):
            continue
        if not actor.get_actor_label().startswith(("Rubble_", "Debris_", "Road_", "Break_")):
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


def fence_void(world, terrain):
    """Wall off the edge where the map's collision stops and nothing begins.

    The landscape's 29 collision components cover a stepped rectangle; step off it and a
    downward trace finds nothing at all, so the player falls until the engine's world bounds
    check destroys them. WorldSettings.KillZ is -1048575 - effectively unset - and it is not
    ours to change, being a property rather than a taggable actor. Fencing is additive: each
    section is a tagged BlockingVolume, so deleting the folder puts the map back.

    Only the boundary that this district actually exposes is fenced - 11 of our roads and
    podiums sit within 15 m of the drop and none had a BlockingVolume within 30 m of them.
    """
    xs, ys = [], []
    for actor in EAS.get_all_level_actors():
        if not has_tag(actor):
            continue
        if not actor.get_actor_label().startswith(("Road_", "Bldg_", "Podium_")):
            continue
        p = actor.get_actor_location()
        xs.append(p.x)
        ys.append(p.y)
    if not xs:
        return 0
    x0, x1 = min(xs) - FENCE_MARGIN, max(xs) + FENCE_MARGIN
    y0, y1 = min(ys) - FENCE_MARGIN, max(ys) + FENCE_MARGIN

    nx = int((x1 - x0) / FENCE_STEP) + 1
    ny = int((y1 - y0) / FENCE_STEP) + 1
    grid = {}
    for i in range(nx):
        for j in range(ny):
            grid[(i, j)] = solid_ground(world, x0 + i * FENCE_STEP, y0 + j * FENCE_STEP)

    # The wall goes on the VOID side of the lip, not on the last walkable cell - putting it on
    # solid ground would wall off metres of ground the player is entitled to stand on.
    boundary = set()
    for (i, j), ok in grid.items():
        if ok:
            continue
        for di, dj in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            if grid.get((i + di, j + dj), False):
                boundary.add((i, j))
                break

    # Merge runs along j so the fence is a few long walls, not hundreds of little cubes.
    runs, used = [], set()
    for (i, j) in sorted(boundary):
        if (i, j) in used:
            continue
        end = j
        while (i, end + 1) in boundary:
            end += 1
            used.add((i, end))
        used.add((i, j))
        runs.append((i, j, end))

    made = 0
    for (i, j0, j1) in runs:
        if made >= FENCE_MAX:
            break
        cx = x0 + i * FENCE_STEP
        cy = y0 + (j0 + j1) * 0.5 * FENCE_STEP
        length = (j1 - j0 + 1) * FENCE_STEP
        # The cell itself is void, so take the height from whichever neighbour is solid.
        ground = None
        for dx, dy in ((FENCE_STEP, 0.0), (-FENCE_STEP, 0.0), (0.0, FENCE_STEP),
                       (0.0, -FENCE_STEP), (0.0, 0.0)):
            ground = terrain.exact(cx + dx, cy + dy)
            if ground is not None:
                break
        if ground is None:
            continue
        volume = spawn_class(unreal.BlockingVolume, (cx, cy, ground + FENCE_HEIGHT * 0.5 - 200.0),
                             unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0),
                             "Fence_%03d" % made)
        if not volume:
            continue
        # A spawned volume's brush is a 200 uu cube.
        volume.set_actor_scale3d(unreal.Vector(FENCE_STEP / 200.0, length / 200.0,
                                               FENCE_HEIGHT / 200.0))
        volume.set_actor_hidden_in_game(True)
        try:
            volume.set_is_temporarily_hidden_in_editor(True)
        except Exception:
            pass
        made += 1
    REPORT.append("fenced the drop with %d blocking volumes over %d void-edge cells "
                  "(WorldSettings.KillZ is unset at -1048575; not changed - it is a property, "
                  "not a taggable actor)" % (made, len(boundary)))
    return made


def fence_local(world, terrain, start_index):
    """Close the small holes the coarse boundary sweep steps over.

    The boundary grid samples every 8 m, so a gap in the collision narrower than that falls
    between its samples - six of the eleven roads sitting beside a drop were next to exactly
    that kind of interior hole. This probes a tight ring around each thing this pass placed
    and walls whatever void it finds, which is bounded work rather than a finer global grid.
    """
    wanted, made = set(), 0
    for actor in EAS.get_all_level_actors():
        if not has_tag(actor):
            continue
        if not actor.get_actor_label().startswith(("Road_", "Bldg_", "Podium_")):
            continue
        origin = actor.get_actor_location()
        for step in range(8):
            angle = 2.0 * math.pi * step / 8.0
            for reach in (600.0, 1200.0, 1800.0):
                px = origin.x + math.cos(angle) * reach
                py = origin.y + math.sin(angle) * reach
                if solid_ground(world, px, py):
                    continue
                key = (int(px / LOCAL_FENCE_CELL), int(py / LOCAL_FENCE_CELL))
                wanted.add(key)
    for (i, j) in sorted(wanted):
        if made >= FENCE_MAX:
            break
        cx, cy = i * LOCAL_FENCE_CELL, j * LOCAL_FENCE_CELL
        # Snapping the void sample to a cell can move it up to half a cell onto real ground.
        # Re-test where the wall would actually stand, or it blocks somewhere walkable.
        if solid_ground(world, cx, cy):
            continue
        ground = None
        for dx, dy in ((LOCAL_FENCE_CELL, 0.0), (-LOCAL_FENCE_CELL, 0.0),
                       (0.0, LOCAL_FENCE_CELL), (0.0, -LOCAL_FENCE_CELL)):
            ground = terrain.exact(cx + dx, cy + dy)
            if ground is not None:
                break
        if ground is None:
            continue
        volume = spawn_class(unreal.BlockingVolume,
                             (cx, cy, ground + FENCE_HEIGHT * 0.5 - 200.0),
                             unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0),
                             "Fence_%03d" % (start_index + made))
        if not volume:
            continue
        volume.set_actor_scale3d(unreal.Vector(LOCAL_FENCE_CELL / 200.0,
                                               LOCAL_FENCE_CELL / 200.0,
                                               FENCE_HEIGHT / 200.0))
        volume.set_actor_hidden_in_game(True)
        try:
            volume.set_is_temporarily_hidden_in_editor(True)
        except Exception:
            pass
        made += 1
    REPORT.append("walled %d interior holes the 8 m boundary sweep stepped over" % made)
    return made


def add_navigation(world, centre, half):
    """The map ships no NavMeshBoundsVolume at all, so nothing here was ever navigable.

    Sized to what was actually built, not to the authored slum. Streets seed as far as
    SEED_SCAN_REACH (1.5 km) and then grow ~1.2 km more, so "authored bounds + 300 m" left most
    of a district the player can now walk into with no navmesh at all - and while the additions
    were distant skyline that was survivable, they are not that any more. The box grows over
    every road, podium and building this pass placed, and is then clamped to NAV_MAX_HALF about
    the authored centre: that clamp is the deliberate playable radius, because asking Recast to
    cover the whole valley is minutes of build time for ground nothing fights over.
    """
    xs = [centre[0] - half[0], centre[0] + half[0]]
    ys = [centre[1] - half[1], centre[1] + half[1]]
    zs = []
    for actor in EAS.get_all_level_actors():
        if not has_tag(actor):
            continue
        if not actor.get_actor_label().startswith(("Road_", "Bldg_", "Podium_")):
            continue
        point = actor.get_actor_location()
        xs.append(point.x)
        ys.append(point.y)
        zs.append(point.z)
    cx, cy = (min(xs) + max(xs)) * 0.5, (min(ys) + max(ys)) * 0.5
    half_x = (max(xs) - min(xs)) * 0.5 + NAV_MARGIN
    half_y = (max(ys) - min(ys)) * 0.5 + NAV_MARGIN
    clamped = []
    if half_x > NAV_MAX_HALF:
        cx, half_x = centre[0], NAV_MAX_HALF
        clamped.append("X")
    if half_y > NAV_MAX_HALF:
        cy, half_y = centre[1], NAV_MAX_HALF
        clamped.append("Y")
    # Placed z values are ground level (a building is spawned at its base), so the box only has
    # to reach an agent's worth above the highest of them - a navmesh does not cover roofs.
    low = (min(zs) if zs else -25000.0) - 2000.0
    high = (max(zs) if zs else 15000.0) + 10000.0
    height = max(20000.0, high - low)
    extent = (2.0 * half_x, 2.0 * half_y, height)
    volume = spawn_class(unreal.NavMeshBoundsVolume, (cx, cy, (low + high) * 0.5),
                         unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0), "Nav_CityCompletion")
    if not volume:
        REPORT.append("FAILED could not spawn NavMeshBoundsVolume")
        return
    # A spawned volume's brush is a 200 uu cube.
    volume.set_actor_scale3d(unreal.Vector(extent[0] / 200.0, extent[1] / 200.0,
                                           extent[2] / 200.0))
    # The volume's wireframe is editor-only visualisation, and it is what turned the last
    # screenshot into a cage of yellow lines. It must never draw in a build either.
    volume.set_actor_hidden_in_game(True)
    try:
        volume.set_is_temporarily_hidden_in_editor(True)
    except Exception:
        pass
    # Adding the volume makes the engine spawn a RecastNavMesh of its own. Untagged it would
    # survive a revert, leaving an actor the authored map never had.
    for actor in EAS.get_all_level_actors():
        if "RecastNavMesh" in type(actor).__name__ and not has_tag(actor):
            claim(actor, "Nav_RecastCityCompletion")
    unreal.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    REPORT.append("nav volume %.0f x %.0f x %.0f m at (%.0f, %.0f) m over the built district, "
                  "hidden, RebuildNavigation issued%s"
                  % (extent[0] / 100.0, extent[1] / 100.0, extent[2] / 100.0,
                     cx / 100.0, cy / 100.0,
                     (" - clamped on %s to the %.0f m playable radius"
                      % ("/".join(clamped), NAV_MAX_HALF / 100.0)) if clamped else ""))


def tidy_viewport():
    """Leave nothing selected. A screenshot taken straight after a run showed the whole
    completion folder highlighted, which reads as broken geometry rather than a selection."""
    try:
        EAS.set_selected_level_actors([])
    except Exception as exc:
        REPORT.append("could not clear selection: %s" % exc)


# --- frontage ------------------------------------------------------------------------------
def spawn_podium(x, y, low, top, half_x, half_y, yaw, tier, label):
    """A plinth from the lowest ground under the plot up to the building's floor.

    Without it a flat-based building on a slope has to be buried to its lowest corner, which
    put a median 5.8 m of hillside over the ground floor. With it the building stands on the
    high side at grade and the plinth walls the low side, which is how a real hillside block
    is built - and it leaves a solid, walkable edge instead of a gap under the downhill wall.
    """
    cube = load(PODIUM_MESH)
    if not cube:
        return None
    height = max(PODIUM_MIN, (top - low) + PODIUM_SKIRT)
    if height > PODIUM_MAX:
        return None
    centre_z = top - height * 0.5
    actor = spawn_mesh(cube, (x, y, centre_z),
                       unreal.Rotator(pitch=0.0, yaw=yaw, roll=0.0),
                       ((half_x * 2.0 * PODIUM_INSET) / 100.0,
                        (half_y * 2.0 * PODIUM_INSET) / 100.0,
                        height / 100.0),
                       label, tier)
    if actor:
        # The engine cube ships WorldGridMaterial, which is the bright default - never leave it.
        material = (PALETTE.get(tier) or PALETTE.get(1) or {}).get("stone")
        if material:
            actor.static_mesh_component.set_material(0, material)
    return actor


def pick_kind(reach, downtown_offset, rng):
    """A skyline, not a uniform ring.

    Low-rise leans against the slum so the shanty grows into a street rather than stopping at
    a tower wall; height climbs outward; and one sector gets a downtown so the horizon has a
    centre of mass instead of an even scatter.
    """
    if abs(downtown_offset) < 0.62 and reach > 0.33:
        weights = (("tower", 0.60), ("mid", 0.33), ("low", 0.07))
    else:
        weights = (("low", max(0.06, 0.78 - reach)),
                   ("mid", 0.45),
                   ("tower", max(0.03, reach * 0.55)))
    total = sum(w for _k, w in weights)
    roll = rng.uniform(0.0, total)
    for kind, weight in weights:
        roll -= weight
        if roll <= 0.0:
            return kind
    return weights[-1][0]


def scatter_around(terrain, x, y, half_x, half_y, tier, rng, pools, label):
    """Rubble, street furniture and scorch, kept to the plot rather than sprayed in a disc."""
    made = 0
    for index in range(rng.randint(3, 7)):
        pool = pools["rubble"]
        if not pool:
            break
        angle = rng.uniform(0.0, 2.0 * math.pi)
        distance = max(half_x, half_y) * rng.uniform(1.05, 1.9)
        px, py = x + math.cos(angle) * distance, y + math.sin(angle) * distance
        scale = rng.uniform(0.7, 2.2)
        piece = pool[rng.randrange(len(pool))]
        ground = terrain.lowest(px, py, footprint_radius(piece, scale))
        if ground is None:
            continue
        if ensure_seated(terrain, spawn_mesh(
                piece, (px, py, ground - bed_in(piece, scale)),
                unreal.Rotator(pitch=rng.uniform(-12.0, 12.0),
                               yaw=rng.uniform(0.0, 360.0),
                               roll=rng.uniform(-12.0, 12.0)),
                (scale, scale, scale), "Rubble_%s_%02d" % (label, index), tier)):
            made += 1
    if tier == 0 and pools["props"]:
        for index in range(2):
            angle = rng.uniform(0.0, 2.0 * math.pi)
            distance = max(half_x, half_y) * rng.uniform(1.1, 1.7)
            px, py = x + math.cos(angle) * distance, y + math.sin(angle) * distance
            prop = pools["props"][rng.randrange(len(pools["props"]))]
            ground = terrain.lowest(px, py, footprint_radius(prop, 1.0))
            if ground is None:
                continue
            if ensure_seated(terrain, spawn_mesh(
                    prop, (px, py, ground - bed_in(prop, 1.0)),
                    unreal.Rotator(pitch=0.0, yaw=rng.uniform(0.0, 360.0), roll=0.0),
                    (1.0, 1.0, 1.0), "Debris_%s_%02d" % (label, index), tier)):
                made += 1
    for index in range(2 if pools["decals"] else 0):
        angle = rng.uniform(0.0, 2.0 * math.pi)
        distance = max(half_x, half_y) * rng.uniform(0.9, 2.0)
        px, py = x + math.cos(angle) * distance, y + math.sin(angle) * distance
        ground = terrain.exact(px, py)
        if ground is None:
            continue
        decal = spawn_class(unreal.DecalActor, (px, py, ground + 60.0),
                            unreal.Rotator(pitch=-90.0, yaw=rng.uniform(0.0, 360.0), roll=0.0),
                            "Scorch_%s_%02d" % (label, index))
        if decal:
            decal.set_decal_material(pools["decals"][rng.randrange(len(pools["decals"]))])
            size = rng.uniform(4.0, 11.0)
            decal.set_actor_scale3d(unreal.Vector(1.0, size, size))
            made += 1
    return made


def street_walk(line):
    """(x, y, heading) sampler along a polyline, plus its total length."""
    marks, total = [], 0.0
    for index in range(len(line) - 1):
        (ax, ay), (bx, by) = line[index], line[index + 1]
        length = math.hypot(bx - ax, by - ay)
        if length <= 0.0:
            continue
        marks.append((total, length, ax, ay, (bx - ax) / length, (by - ay) / length))
        total += length

    def at(distance):
        for start, length, ax, ay, ux, uy in marks:
            if distance <= start + length:
                travel = distance - start
                return ax + ux * travel, ay + uy * travel, math.atan2(uy, ux)
        return None

    return at, total


def place_frontage(terrain, streets, spots, street_points, paving, pool, centre, rng,
                   pools, caps):
    """Walk every street and fill the lots on both sides of it.

    Each side keeps its own cursor and advances by the frontage of whatever actually landed,
    so a 184 m row house does not get stacked on top of itself every 46 m.
    """
    by_kind = {"low": [], "mid": [], "tower": []}
    for entry in pool:
        by_kind[entry["kind"]].append(entry)
    if not any(by_kind.values()):
        return 0

    by_area = sorted(pool, key=lambda e: e["area"])
    downtown = rng.uniform(0.0, 2.0 * math.pi)
    far = max((math.hypot(p[0] - centre[0], p[1] - centre[1])
               for line in streets for p in line), default=1.0) or 1.0

    stats = {"placed": 0, "sheared": 0, "instances_removed": 0, "scatter": 0,
             "busy": 0, "tries": 0, "on_street": 0, "podiums": 0, "unpaved": 0}
    why = {}
    for street_index, line in enumerate(streets):
        at, total = street_walk(line)
        if total < LOT_PITCH:
            continue
        for side in (1.0, -1.0):
            cursor = LOT_PITCH * 0.5
            while cursor < total and stats["placed"] < WANTED:
                sample = at(cursor)
                if sample is None:
                    break
                cx, cy, heading = sample
                # Front the road that got built, not the one that got planned. lay_streets()
                # refuses any slab it cannot seat and refused 1,078 of them in the last measured
                # run, so a stretch of a planned street can have no carriageway at all - and a
                # row of buildings facing bare hillside is not frontage.
                if paving.get(road_cell(cx, cy), 0) < FRONT_MIN_SLABS:
                    stats["unpaved"] += 1
                    cursor += LOT_PITCH * 0.5
                    continue
                normal = (-math.sin(heading), math.cos(heading))
                reach = min(1.0, math.hypot(cx - centre[0], cy - centre[1]) / far)
                bearing = math.atan2(cy - centre[1], cx - centre[0])
                offset = (bearing - downtown + 3.0 * math.pi) % (2.0 * math.pi) - math.pi
                kind = pick_kind(reach, offset, rng)

                # Fit the building to the ground, do not pick one and hope. Choosing at random
                # first refused 743 of 747 lots: the pool's widest members are a 51 x 98 m
                # podium and a 92 x 14 m row house, and nothing that big fits a valley where
                # only 2.6% of cells hold a flat 90 m. Smallest footprint first, preferred
                # height class first, so intent survives but a lot still gets built on.
                # Preferred class first, then EVERY class by footprint. Truncating the list
                # before the fallbacks meant a tower lot was only ever offered the eight
                # smallest towers - still 60 m wide - and never the 18 m infill that fits.
                order = []
                for option in sorted(by_kind.get(kind, []), key=lambda e: e["area"]):
                    order.append(option)
                for option in by_area:
                    if option not in order:
                        order.append(option)

                entry = seat = None
                yaw = 0.0
                x = y = 0.0
                box_x = box_y = 0.0
                for option in order[:FIT_ATTEMPTS]:
                    half_x, half_y = option["half"]
                    stats["tries"] += 1
                    try_yaw = math.degrees(heading) + (0.0 if side > 0.0 else 180.0)
                    lot = ROAD_WIDTH * 0.5 + half_y + 400.0
                    try_x = cx + normal[0] * side * lot
                    try_y = cy + normal[1] * side * lot
                    found = terrain.footprint(try_x, try_y, half_x, half_y, try_yaw, why,
                                              option["height"])
                    if found is None:
                        continue
                    # The extents are measured at zero yaw and this building is turned to the
                    # street, so the box the axis-aligned test needs is the turned one.
                    try_box = world_half(half_x, half_y, try_yaw)
                    if not clear_of(try_x, try_y, CLEARANCE, spots, try_box[0], try_box[1]):
                        stats["busy"] += 1
                        continue
                    # Do not sit on some OTHER street. Measured against the inscribed radius,
                    # because the narrow side is what faces a carriageway.
                    keep = min(half_x, half_y) + ROAD_WIDTH * 0.5 + 200.0
                    if any((try_x - sx) ** 2 + (try_y - sy) ** 2 < keep * keep
                           for sx, sy in street_points):
                        stats["on_street"] += 1
                        continue
                    entry, seat, yaw, x, y = option, found, try_yaw, try_x, try_y
                    box_x, box_y = try_box
                    break

                if entry is None:
                    cursor += LOT_PITCH * 0.5
                    continue
                half_x, half_y = entry["half"]

                low, base, _relief = seat
                tier = 0 if reach < 0.34 else (1 if reach < 0.7 else 2)
                sink = rng.uniform(*SINK_METRES[kind]) * 100.0
                relief = base - low
                blueprint = unreal.load_asset(entry["pkg"])
                generated = blueprint.generated_class() if blueprint else None
                if not generated:
                    cursor += LOT_PITCH
                    continue
                tilt = TILT_DEGREES[kind]
                actor = EAS.spawn_actor_from_class(
                    generated, unreal.Vector(x, y, base - sink),
                    unreal.Rotator(pitch=rng.uniform(-tilt, tilt), yaw=yaw,
                                   roll=rng.uniform(-tilt, tilt)))
                if not actor:
                    cursor += LOT_PITCH
                    continue
                label = "S%02d_%s_T%d_%02d" % (street_index, kind, tier, stats["placed"])
                claim(actor, "Bldg_" + label)
                dress_actor(actor, tier)
                removed, cut = shear(actor, kind, rng)
                if removed:
                    stats["sheared"] += 1
                    stats["instances_removed"] += removed
                    stats["scatter"] += cap_break(terrain, x, y, cut, half_x, half_y,
                                                  tier, rng, caps, "Break_" + label)
                stats["scatter"] += scatter_around(terrain, x, y, half_x, half_y, tier,
                                                   rng, pools, label)
                if relief > PODIUM_MIN:
                    if spawn_podium(x, y, low, base - sink, half_x, half_y, yaw, tier,
                                    "Podium_" + label):
                        stats["podiums"] += 1
                # The world box, not the local one - the next building tests against this.
                spots.append((x, y, box_x, box_y))
                stats["placed"] += 1
                cursor += 2.0 * half_x + 900.0

    REPORT.append("placed %d of %d lot attempts as street frontage "
                  "(%d on podiums, %d sheared, %d instances removed, %d scatter actors)"
                  % (stats["placed"], stats["tries"], stats["podiums"], stats["sheared"],
                     stats["instances_removed"], stats["scatter"]))
    REPORT.append("lot refusals: %s, occupied %d, on another street %d, unpaved street %d"
                  % (", ".join("%s %d" % kv for kv in sorted(why.items())) or "none",
                     stats["busy"], stats["on_street"], stats["unpaved"]))
    return stats["placed"]


def main():
    LES.load_level(MAP_PATH)
    backup_map()
    clear_previous()
    build_palette()

    rng = random.Random(SEED)
    spots, bounds = existing_obstacles()
    x0, y0, x1, y1 = bounds
    centre = ((x0 + x1) * 0.5, (y0 + y1) * 0.5)
    half = (max((x1 - x0) * 0.5, 1.0), max((y1 - y0) * 0.5, 1.0))
    REPORT.append("authored core %.0f x %.0f m around (%.0f, %.0f) m, %d obstacles"
                  % ((x1 - x0) / 100.0, (y1 - y0) / 100.0,
                     centre[0] / 100.0, centre[1] / 100.0, len(spots)))

    pool = measure_pool()
    if not pool:
        REPORT.append("FAILED no usable buildings; run Scripts/MigrateCitySampleKit.py")
        return

    ruins = meshes_matching(EREBUS_MESHES, ("Rubble", "Debris", "Wreck", "RuinEdge",
                                            "RoadSlab", "CraterPatch"))
    rocks = []
    for folder in ROCK_DIRS:
        rocks += meshes_matching(folder, ())
    pools = {
        "rubble": ruins + rocks,
        "props": meshes_matching(STREET_PROP_DIR, ()),
        "decals": [m for m in (load("/Game/Ashes/Materials/Instances/MI_Erebus_Decal_Scorch"),
                               load("/Game/Ashes/Materials/Instances/MI_Erebus_Decal_Grime"))
                   if m],
    }
    caps = {
        "frame": meshes_matching(EREBUS_MESHES, ("StructureFrame", "BeamHeavy")),
        "collapse": meshes_matching(EREBUS_MESHES, ("Facade_Broken", "Facade_Heavy",
                                                    "DebrisField", "RubbleBerm", "RuinBlock")),
    }
    REPORT.append("%d pool buildings, %d rubble meshes, %d props, %d cap meshes"
                  % (len(pool), len(pools["rubble"]), len(pools["props"]),
                     len(caps["frame"]) + len(caps["collapse"])))

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    terrain = Terrain(world, water_zones())
    if configure_road() is None:
        REPORT.append("FAILED road tile missing; run Scripts/MigrateCitySampleKit.py")
        return
    authored = list(spots)   # snapshot before anything new is appended

    streets = grow_streets(terrain, authored, centre, half, rng)
    if not streets:
        REPORT.append("FAILED no street could be grown - terrain rejected every seed")
        return
    paving = lay_streets(terrain, streets)
    if not paving:
        REPORT.append("FAILED not one road slab could be seated - nothing to front")
        return
    # Streets are kept OUT of the box list. A building's half extents are local, but the box
    # test compares them on world axes, so a 125 m long building was being refused against the
    # very street it fronts. They get their own distance test against the inscribed radius.
    street_points = [p for line in streets for p in line]

    place_frontage(terrain, streets, spots, street_points, paving, pool, centre, rng,
                   pools, caps)
    hide_mattes(MATTE_MODE)
    apply_culling()
    # Fences first. They are BlockingVolumes, so they are geometry Recast has to see - issuing
    # RebuildNavigation before they exist bakes a navmesh that runs straight over the lip.
    fenced = fence_void(world, terrain)
    fence_local(world, terrain, fenced)
    add_navigation(world, centre, half)
    tidy_viewport()
    REPORT.append("culled %d loose pieces that ended up with air beneath them" % CULLED[0])
    REPORT.append("plain (non-instanced) mesh components: %d sheared away above the break, "
                  "%d left standing at or below it" % (SOLIDS[0], SOLIDS[1]))
    REPORT.append("material slots: %d filled from slot names, %d left as authored art"
                  % (DRESSED[0], DRESSED[1]))

    if not unreal.EditorLoadingAndSavingUtils.save_map(
            unreal.EditorLevelLibrary.get_editor_world(), MAP_PATH):
        REPORT.append("FAILED could not save " + MAP_PATH)


try:
    main()
finally:
    with open(saved_path("CompleteSoulCityReport.txt"), "w") as handle:
        handle.write("\n".join(REPORT) + "\n")
