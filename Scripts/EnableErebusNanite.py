"""Enable Nanite on the large static Erebus architecture.

The packaged Mac build reports "[VSM] Non-Nanite Marking Job Queue overflow" on the mid-route
poses. The engine's own text says why: "This occurs when many non-nanite meshes cover a large area
of the shadow map." The shader condition is in VirtualShadowMapBuildPerPageDrawCommands.usf - an
instance whose page rect exceeds MAX_SINGLE_THREAD_MARKING_AREA (8 pages) becomes a "large job",
and more than MARKING_JOB_QUEUE_SIZE (NUM_THREADS_PER_GROUP * 2) of them in one group overflows
the groupshared queue. The whole kit is authored non-Nanite (GenerateErebusArtKit.py sets
create_opts.enable_nanite = False), so every wall, facade and tower goes down that path.

Raising the clipmap range did not help and was reverted: it changes which levels exist, not how
many large non-Nanite instances mark pages. Nanite geometry does not use this path at all.

Scope is deliberately narrow - large static architecture and large static wrecks. Excluded:
  * cloth and thin geometry (BannerDrape, CableSpan, CableSupport) - two-sided/masked or so thin
    that Nanite buys nothing;
  * water and puddles (Puddle, PuddleSet) - translucent, which Nanite does not support;
  * small props (barrels, crates, sandbags, signs, lamps, poles, traps, pipes, scaffold, catwalk,
    rubble chunks) - they never produce a large page rect, so they are not part of the problem.

FallbackPercentTriangles stays at 1.0 on purpose: the auto-generated fallback mesh is what
non-Nanite platforms (mobile) render, and at 1.0 it is the same geometry they render today, so
this is a desktop rendering-path change and not a content change.
"""

import unreal

MESH_DIR = "/Game/Ashes/Environment/Erebus/Meshes/"

LARGE_ARCHITECTURE = [
    # Corridor walls and fortification
    "SM_Erebus_TrenchWall_A", "SM_Erebus_TrenchCorner_A",
    "SM_Erebus_BlastWall_A", "SM_Erebus_BlastWall_B", "SM_Erebus_ArmorBarrier_A",
    "SM_Erebus_BunkerWall_A", "SM_Erebus_BunkerCorner_A", "SM_Erebus_BunkerRoof_A",
    "SM_Erebus_CheckpointGate_A",
    # Facades, blocks and ruins
    "SM_Erebus_Facade_Heavy_A", "SM_Erebus_Facade_Heavy_B", "SM_Erebus_Facade_Broken_A",
    "SM_Erebus_RuinedFacade_A", "SM_Erebus_RuinedFacade_B",
    "SM_Erebus_RuinBlock_A", "SM_Erebus_RuinBlock_B",
    "SM_Erebus_RuinEdge_A", "SM_Erebus_RuinEdge_B",
    "SM_Erebus_TowerSlab_A", "SM_Erebus_Fortress_A", "SM_Erebus_Fortress_B",
    # Industrial structure
    "SM_Erebus_IndustrialWall_A", "SM_Erebus_IndustrialColumn_A", "SM_Erebus_IndustrialSupport_A",
    "SM_Erebus_IndustrialDoor_A", "SM_Erebus_ColumnHeavy_A", "SM_Erebus_BeamHeavy_A",
    "SM_Erebus_StructureFrame_A", "SM_Erebus_StructureFrame_B", "SM_Erebus_GantryTower_A",
    "SM_Erebus_ServiceBay_A", "SM_Erebus_ServiceBay_Destroyed_A",
    "SM_Erebus_Overhang_A", "SM_Erebus_PanelBank_A", "SM_Erebus_VentBank_A",
    # Ground planes - large area, always in the clipmap
    "SM_Erebus_GroundSlab_A", "SM_Erebus_RoadSlab_Cracked_A", "SM_Erebus_BrokenFloor_A",
    "SM_Erebus_MudBase_A", "SM_Erebus_CraterPatch_A", "SM_Erebus_RubbleBerm_A",
    "SM_Erebus_DebrisField_A", "SM_Erebus_DebrisField_B",
    # Distant landmark cluster
    "SM_Erebus_Monolith_A",
    "SM_Erebus_CathedralTower_A", "SM_Erebus_CathedralTower_B", "SM_Erebus_CathedralTower_C",
    "SM_Erebus_CathedralSpire_A", "SM_Erebus_CathedralSpire_B",
    # Large static wrecks
    "SM_Erebus_TankHulk_A", "SM_Erebus_GunshipWreck_A", "SM_Erebus_TruckWreck_A",
    "SM_Erebus_Wreckage_A", "SM_Erebus_Wreckage_B",
]

report = []
failures = 0
for name in LARGE_ARCHITECTURE:
    path = MESH_DIR + name
    mesh = unreal.load_asset(path)
    if not mesh:
        report.append("MISSING %s" % name)
        failures += 1
        continue
    settings = mesh.get_editor_property("nanite_settings")
    settings.set_editor_property("enabled", True)
    # Full-detail fallback: mobile keeps exactly the geometry it has today.
    for prop in ("fallback_percent_triangles", "keep_triangle_percent"):
        try:
            settings.set_editor_property(prop, 1.0)
        except Exception:
            pass
    mesh.set_editor_property("nanite_settings", settings)
    unreal.EditorAssetLibrary.save_asset(path)

    check = unreal.load_asset(path).get_editor_property("nanite_settings")
    enabled = check.get_editor_property("enabled")
    if not enabled:
        failures += 1
    report.append("%s nanite=%s" % (name, enabled))

report.append("total=%d failures=%d" % (len(LARGE_ARCHITECTURE), failures))
with open("/tmp/nanite_report.txt", "w") as handle:
    handle.write("\n".join(report))
