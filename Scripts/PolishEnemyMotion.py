"""Apply motion-only tuning and close Teuthisan loops. Leaves materials, loadouts and story intact.

Run after building the editor target and saving/closing the live editor. Back up the affected
enemy definitions and Teuthisan GameAnims first. Existing animation assets update in place.
"""
import os
import sys
import unreal

sys.path.insert(0, os.path.dirname(__file__))
import AuthorEnemyDefinitions as definitions
import AuthorTeuthisanAnimations as teuthisan

for name, spec in definitions.ARCHETYPES.items():
    path = "/Game/Ashes/Data/Enemies/DA_Enemy_" + name
    asset = unreal.load_asset(path)
    tuned = definitions._locomotion_payload(name, spec, asset)
    # Preserve current clip references and all non-motion fields, on both presentation tiers.
    for tier in ("visuals", "mobile_visuals"):
        visual = asset.get_editor_property(tier)
        motion = visual.get_editor_property("locomotion")
        for field in ("walk_speed", "run_speed", "walk_reference_speed",
                      "run_reference_speed", "attack_impact_time"):
            motion.set_editor_property(field, tuned.get_editor_property(field))
        visual.set_editor_property("locomotion", motion)
        asset.set_editor_property(tier, visual)
    if name == "Spider":
        combat = asset.get_editor_property("combat_defaults")
        combat.set_editor_property("walk_speed", spec["speed"])
        asset.set_editor_property("combat_defaults", combat)
    unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)

for source, targets, span in (("Idle", ("Idle",), 15), ("Crawl", ("Walk", "Run"), 24)):
    _, bones, poses, _ = teuthisan.read_take(teuthisan.SRC + source)
    if source == "Crawl":
        root = bones[0]
        first = poses[0][root][0]
        for pose in poses:
            t, r, s = pose[root]
            pose[root] = (unreal.Vector(first.x, first.y, t.z), r, s)
    teuthisan.close_loop(bones, poses, span)
    for target in targets:
        teuthisan.write_clip("A_Teuthisan_" + target, bones, poses)

unreal.log_warning("Enemy motion tuning and loop closure complete.")
