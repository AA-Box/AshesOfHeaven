import unreal

MAP_PATH = "/Game/ChapterOne/L_ChapterOne_Greybox"
GAME_MODE_PATH = "/Script/AshesOfHeaven.AHChapterOneGameMode"

game_mode = unreal.load_class(None, GAME_MODE_PATH)
if not game_mode:
    raise RuntimeError("Could not load AHChapterOneGameMode")

world = unreal.EditorAssetLibrary.load_asset(MAP_PATH)
if not world:
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        raise RuntimeError("Could not create Chapter One greybox map")
    world = unreal.EditorLevelLibrary.get_editor_world()
if not world:
    raise RuntimeError("Could not load newly created Chapter One greybox map")

world.get_world_settings().set_editor_property("default_game_mode", game_mode)
if not unreal.EditorAssetLibrary.save_loaded_asset(world):
    raise RuntimeError("Could not save Chapter One greybox map")

unreal.log("Created Chapter One greybox map with AHChapterOneGameMode")
