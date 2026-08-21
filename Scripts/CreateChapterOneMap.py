import unreal

MAP_PATH = "/Game/ChapterOne/L_ChapterOne_Greybox"
GAME_MODE_PATH = "/Script/AshesOfHeaven.AHChapterOneGameMode"
SCRATCH_MAP_PATH = "/Engine/Maps/Entry"

# Must match AAHChapterOneDirector::SpawnNavigationCoverage so the saved bounds cover the
# same play space the director builds at runtime.
NAV_BOUNDS_LOCATION = unreal.Vector(12500.0, 0.0, 500.0)
NAV_BOUNDS_SCALE = unreal.Vector(145.0, 32.0, 10.0)

game_mode = unreal.load_class(None, GAME_MODE_PATH)
if not game_mode:
    raise RuntimeError("Could not load AHChapterOneGameMode")

level_subsystem = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def open_map():
    if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
        if not level_subsystem.load_level(MAP_PATH):
            raise RuntimeError("Could not open Chapter One greybox map")
    elif not level_subsystem.new_level(MAP_PATH):
        raise RuntimeError("Could not create Chapter One greybox map")


def find_actor(actor_class):
    for actor in actor_subsystem.get_all_level_actors():
        if isinstance(actor, actor_class):
            return actor
    return None


def save_map():
    if not level_subsystem.save_current_level():
        raise RuntimeError("Could not save Chapter One greybox map")


open_map()

world = level_subsystem.get_current_level().get_outer()
world.get_world_settings().set_editor_property("default_game_mode", game_mode)

# The level is generated empty and all geometry is spawned at runtime, so navigation always
# rebuilds on load. Saving the bounds and the nav data actor into the level makes that the
# configured behaviour instead of the navigation system's recovery path, which is what logged
# "No saved navigation data found for 'Default'" on every launch.
if not find_actor(unreal.NavMeshBoundsVolume):
    bounds = actor_subsystem.spawn_actor_from_class(unreal.NavMeshBoundsVolume, NAV_BOUNDS_LOCATION)
    if not bounds:
        raise RuntimeError("Could not spawn NavMeshBoundsVolume")
    bounds.set_actor_scale3d(NAV_BOUNDS_SCALE)

save_map()

if not find_actor(unreal.RecastNavMesh):
    # Nav data cannot be spawned directly - the navigation system creates it at world init
    # when bounds are present, so bounce through another map to force a reload.
    level_subsystem.load_level(SCRATCH_MAP_PATH)
    open_map()
    nav_data = find_actor(unreal.RecastNavMesh)
    if not nav_data:
        raise RuntimeError("Navigation system did not create nav data for the saved bounds")
    # Geometry only exists at runtime, so the navmesh must be allowed to rebuild then.
    nav_data.set_editor_property("runtime_generation", unreal.RuntimeGenerationType.DYNAMIC)
    save_map()

unreal.log("Chapter One greybox map ready with AHChapterOneGameMode and saved navigation data")
