"""Rebuild only the Hound walk in a saved, backed-up live editor.

Preserves all other takes.
"""
import importlib.util
import json
import os
import unreal

root = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
path = os.path.join(root, "Scripts", "AuthorCreatureAnimations.py")
spec = importlib.util.spec_from_file_location("hound_walk_author", path)
motion = importlib.util.module_from_spec(spec)
spec.loader.exec_module(motion)
motion.author_hound_walk()
asset_path = "/Game/Ashes/Data/Enemies/DA_Enemy_Hound"
asset = unreal.load_asset(asset_path)
for tier in ("visuals", "mobile_visuals"):
    visual = asset.get_editor_property(tier)
    locomotion = visual.get_editor_property("locomotion")
    locomotion.set_editor_property("walk_reference_speed", 50.0)
    locomotion.set_editor_property("walk_speed", 5.0)
    locomotion.set_editor_property("run_speed", 140.0)
    visual.set_editor_property("locomotion", locomotion)
    asset.set_editor_property(tier, visual)
unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False)
with open(motion.REPORT_PATH, "w") as handle:
    json.dump(motion.report, handle, indent=2)
