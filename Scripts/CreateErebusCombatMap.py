import unreal

MAP_PATH = "/Game/Combat/L_Erebus_CombatPrototype"
GAME_MODE_PATH = "/Script/AshesOfHeaven.AHCombatSliceGameMode"

game_mode = unreal.load_class(None, GAME_MODE_PATH)
if not game_mode:
    raise RuntimeError("Could not load AHCombatSliceGameMode")

world = unreal.EditorAssetLibrary.load_asset(MAP_PATH)
if not world:
    if not unreal.EditorLevelLibrary.new_level(MAP_PATH):
        raise RuntimeError("Could not create Erebus combat map")
    world = unreal.EditorLevelLibrary.get_editor_world()
if not world:
    raise RuntimeError("Could not load newly created Erebus combat map")

world.get_world_settings().set_editor_property("default_game_mode", game_mode)
if not unreal.EditorAssetLibrary.save_loaded_asset(world):
    raise RuntimeError("Could not save Erebus combat map")

unreal.log("Created clean Erebus combat map with AHCombatSliceGameMode")
