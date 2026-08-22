import unreal

"""Author L_ErebusOpening_Presentation: the Erebus opening battlefield as a saved level.

Coordinates are ANCHOR-LOCAL: local (0,0,0) is the ErebusOpening stage anchor, which sits
at world (0,0,-50) — the gameplay collision floor top. The runtime director streams this
level in at the canonical anchor (ULevelStreamingDynamic), so authored placement never
hard-codes world positions. Everything here is presentation: no collision, no nav impact.

Regenerating is destructive by design: the level is cleared and rebuilt each run.
"""

LEVEL_PATH = "/Game/Ashes/Environment/Erebus/L_ErebusOpening_Presentation"
MESH_DIR = "/Game/Ashes/Environment/Erebus/Meshes"
BP_DIR = "/Game/Ashes/Blueprints/Environment"
VFX_DIR = "/Game/Ashes/VFX"

LEVELS = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ACTORS = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

SPAWNED = []
MISSING = []


def rot(roll=0.0, pitch=0.0, yaw=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def tag_actor(actor, extra=None):
    tags = [unreal.Name("Phase4Presentation"), unreal.Name("AH.AuthoredZone"), unreal.Name("AH.Zone.Erebus")]
    if extra:
        tags.append(unreal.Name(extra))
    actor.tags = tags


def mesh(name, loc, rotation=(0, 0, 0), scale=(1, 1, 1), label=None):
    asset = unreal.load_asset(MESH_DIR + "/" + name)
    if not asset:
        MISSING.append(name)
        return None
    # spawn_actor_from_object needs actor factories that are absent under -nullrhi;
    # class-spawn plus an explicit component assignment works in every editor mode.
    actor = ACTORS.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*loc), rot(*rotation))
    if not actor:
        MISSING.append(name + " (spawn)")
        return None
    actor.set_actor_scale3d(unreal.Vector(*scale))
    component = actor.get_component_by_class(unreal.StaticMeshComponent)
    if component:
        component.set_editor_property("static_mesh", asset)
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        component.set_editor_property("can_ever_affect_navigation", False)
    tag_actor(actor)
    actor.set_actor_label(label or name)
    SPAWNED.append(actor)
    return actor


def prop(bp_name, loc, rotation=(0, 0, 0), scale=(1, 1, 1)):
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_DIR + "/" + bp_name)
    if not bp_class:
        MISSING.append(bp_name)
        return None
    actor = ACTORS.spawn_actor_from_class(bp_class, unreal.Vector(*loc), rot(*rotation))
    if not actor:
        MISSING.append(bp_name + " (spawn)")
        return None
    actor.set_actor_scale3d(unreal.Vector(*scale))
    tag_actor(actor, "Phase4RuntimeProp")
    actor.set_actor_label(bp_name)
    SPAWNED.append(actor)
    return actor


def vfx(system_name, loc, scale=1.0):
    asset = unreal.load_asset(VFX_DIR + "/" + system_name)
    if not asset:
        MISSING.append(system_name)
        return None
    actor = ACTORS.spawn_actor_from_class(unreal.NiagaraActor, unreal.Vector(*loc), rot())
    if not actor:
        MISSING.append(system_name + " (spawn)")
        return None
    actor.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    component = actor.get_component_by_class(unreal.NiagaraComponent)
    if component:
        component.set_asset(asset)
        component.set_editor_property("auto_activate", True)
        component.set_editor_property("component_tags", [unreal.Name("Phase4PresentationFX")])
    tag_actor(actor)
    actor.set_actor_label(system_name)
    SPAWNED.append(actor)
    return actor


def light(loc, color, intensity, radius, label="ErebusLight"):
    actor = ACTORS.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*loc), rot())
    if not actor:
        return None
    component = actor.get_component_by_class(unreal.PointLightComponent)
    if component:
        component.set_mobility(unreal.ComponentMobility.STATIC)
        component.set_intensity(intensity)
        component.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
        component.set_attenuation_radius(radius)
        component.set_cast_shadows(False)
    tag_actor(actor)
    actor.set_actor_label(label)
    SPAWNED.append(actor)
    return actor


def decal(material_name, loc, rotation=(0, -90, 0), size=(64, 128, 128), label="ErebusDecal"):
    material = unreal.load_asset("/Game/Ashes/Materials/Instances/" + material_name)
    if not material:
        MISSING.append(material_name)
        return None
    actor = ACTORS.spawn_actor_from_class(unreal.DecalActor, unreal.Vector(*loc), rot(*rotation))
    if not actor:
        return None
    component = actor.get_component_by_class(unreal.DecalComponent)
    if component:
        component.set_decal_material(material)
        component.set_editor_property("decal_size", unreal.Vector(*size))
    tag_actor(actor)
    actor.set_actor_label(label)
    SPAWNED.append(actor)
    return actor


def open_clean_level():
    if unreal.EditorAssetLibrary.does_asset_exist(LEVEL_PATH):
        if not LEVELS.load_level(LEVEL_PATH):
            raise RuntimeError("Could not open " + LEVEL_PATH)
        for actor in ACTORS.get_all_level_actors():
            if isinstance(actor, (unreal.WorldSettings, unreal.Brush)):
                continue
            ACTORS.destroy_actor(actor)
    elif not LEVELS.new_level(LEVEL_PATH):
        raise RuntimeError("Could not create " + LEVEL_PATH)


def build_ground():
    for column, cx in enumerate((-1600, 600, 2800, 5000, 7200)):
        for row, cy in enumerate((-1100, 1100)):
            mesh("SM_Erebus_MudBase_A", (cx, cy, -24), (0, 0, (column * 2 + row) % 4 * 90),
                 label="Mud_%d_%d" % (column, row))
    slab_spots = [(-1200, 80, 4), (-760, -120, -8), (-320, 60, 12), (140, -90, -3), (620, 110, 7),
                  (1080, -60, -14), (1560, 90, 5), (2080, -120, -6), (2620, 40, 16), (3140, -70, -2)]
    for index, (sx, sy, yaw) in enumerate(slab_spots):
        mesh("SM_Erebus_GroundSlab_A", (sx, sy, -16), (0, 0, yaw), label="RoadSlab_%d" % index)
    puddles = [(-760, -330, 2.4), (-320, 220, 2.8), (340, -110, 3.6), (1240, 420, 2.9),
               (2300, -260, 3.2), (-1150, 150, 2.0), (1800, 140, 2.4)]
    for index, (px, py, size) in enumerate(puddles):
        mesh("SM_Erebus_Puddle_A", (px, py, 3), (0, 0, (index * 53) % 180), (size, size * 0.72, 1.0),
             label="Puddle_%d" % index)
    rubble_patches = [(-1200, -450, 3), (-880, 380, 2), (-480, -140, 2), (150, 260, 3),
                      (1050, -420, 3), (2050, 180, 3), (3100, -350, 2)]
    for index, (rx, ry, count) in enumerate(rubble_patches):
        for sub in range(count):
            offset_x = ((sub * 73 + index * 31) % 260) - 130
            offset_y = ((sub * 119 + index * 47) % 260) - 130
            big = (sub + index) % 3 == 0
            mesh("SM_Erebus_RubbleLarge_A" if big else "SM_Erebus_RubbleMedium_A",
                 (rx + offset_x, ry + offset_y, -6), (0, 0, (sub * 97 + index * 61) % 360),
                 (0.8 + (sub % 3) * 0.25,) * 3, label="Rubble_%d_%d" % (index, sub))
    for index, (fx, fy, yaw) in enumerate([(420, -260, 24), (1720, 330, -50), (2450, -180, 74)]):
        mesh("SM_Erebus_BrokenFloor_A", (fx, fy, -10), (0, 0, yaw), label="BrokenFloor_%d" % index)


def build_spawn_area():
    # Rear boundary wall (visual twin of the collision wall at X=-2150).
    for wy in (-800, 0, 800):
        mesh("SM_Erebus_IndustrialWall_A", (-2170, wy, 0), (0, 0, 90), (1.0, 1.0, 1.15), label="RearWall")
    mesh("SM_Erebus_RuinedFacade_B", (-2420, -600, 0), (0, 0, 90), (1.3, 1.3, 1.3), label="RearRuin_A")
    mesh("SM_Erebus_RuinedFacade_A", (-2450, 700, 0), (0, 0, 88), (1.2, 1.2, 1.4), label="RearRuin_B")

    # Trench walls: visual twins of the greybox collision trench (X -650 + i*1150 at Y=-1050 etc).
    for index in range(4):
        wall_x = -650.0 + index * 1150.0
        mesh("SM_Erebus_TrenchWall_A", (wall_x, -1035, 0), (0, 0, 0), (1.0, 1.0, 1.35), label="TrenchS_%d" % index)
        mesh("SM_Erebus_TrenchWall_A", (wall_x + 480.0, 965, 0), (0, 0, 0), (1.0, 1.0, 1.6), label="TrenchN_%d" % index)

    # Left flank wreck cluster with the burning barrel (light lives below).
    prop("BP_Erebus_WreckCluster_A", (-1050, -650, 0), (0, 0, 25), (1.2, 1.2, 1.2))
    mesh("SM_Erebus_UtilityPole_A", (-1120, -280, 12), (24, 88, 0), label="FallenPole")
    mesh("SM_Erebus_Barrel_A", (-990, -240, 0), (6, 0, 4), (1.1, 1.1, 1.1), label="BurnBarrel")
    mesh("SM_Erebus_RubbleMedium_A", (-1010, -420, -4), (0, 0, 140))
    mesh("SM_Erebus_CrateOpen_A", (-1260, -350, 0), (0, 0, 24))
    mesh("SM_Erebus_Crate_A", (-1290, -230, 0), (0, 0, -12))

    # Right flank pipe cluster.
    mesh("SM_Erebus_Pipe_Large_A", (-900, 700, 0), (0, 0, -15))
    mesh("SM_Erebus_Pipe_Large_A", (-880, 760, 84), (0, 0, -13), label="PipeStacked")
    mesh("SM_Erebus_Pipe_Elbow_A", (-580, 640, 0), (0, 0, 165))
    mesh("SM_Erebus_PipeSupport_A", (-1080, 690, 0), (0, 0, -15))
    mesh("SM_Erebus_PipeSupport_A", (-720, 720, 0), (0, 0, -15))
    mesh("SM_Erebus_SandbagRow_A", (-1350, 480, 0), (0, 0, 12), (1.0, 1.0, 0.72))


def build_defensive_line():
    prop("BP_Erebus_Barricade", (-620, -560, 0), (0, 0, -6), (1.2, 1.2, 1.2))
    prop("BP_Erebus_Barricade", (-580, 560, 0), (0, 0, 8), (1.15, 1.15, 1.15))
    prop("BP_Human_ExpeditionLight", (-520, -140, 0), (0, 0, 0), (1.0, 1.0, 1.0))
    mesh("SM_Erebus_ArmorBarrier_A", (-600, -250, 0), (0, 0, 184), label="LineBarrier_S")
    mesh("SM_Erebus_ArmorBarrier_A", (-600, 260, 0), (0, 0, 175), label="LineBarrier_N")
    mesh("SM_Erebus_SandbagRow_A", (-640, -300, 0), (0, 0, 84), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_SandbagRow_A", (-655, -620, 0), (0, 0, 96), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_SandbagRow_A", (-620, 330, 0), (0, 0, 88), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_SandbagRow_A", (-600, 650, 0), (0, 0, 78), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_Crate_A", (-700, 390, 0), (0, 0, 40))
    mesh("SM_Erebus_Barricade_A", (-540, -820, 0), (0, 0, 4))
    light((-540, -140, 310), (1.0, 0.45, 0.15), 5200.0, 860.0, "LineLight")


def build_midground():
    # Fortification row on the south flank (replaces the five dark cubes).
    for index in range(5):
        row_x = 250.0 + index * 520.0
        if index % 2 == 0:
            mesh("SM_Erebus_BlastWall_A", (row_x, -540, 0), (0, 0, 4 if index % 4 == 0 else -4))
        else:
            mesh("SM_Erebus_ArmorBarrier_A", (row_x, -540, 0), (0, 0, -6))
        mesh("SM_Erebus_IndustrialColumn_A", (row_x + 160.0, -520, 0), (0, 0, index * 30))

    mesh("SM_Erebus_BlastWall_A", (760, -620, 0), (0, 0, 8), (1.5, 1.5, 1.4), label="BigBlastWall")
    mesh("SM_Erebus_BlastWall_B", (960, -640, 0), (0, 0, 12), (1.5, 1.5, 1.4), label="BigBlastWallB")
    prop("BP_Erebus_Barricade", (1500, 460, 0), (0, 0, -5), (1.25, 1.25, 1.25))
    prop("BP_Erebus_PipeCluster", (2350, 650, 0), (0, 0, 18), (1.35, 1.35, 1.35))
    prop("BP_Erebus_WreckCluster_A", (3020, -420, 0), (0, 0, -20), (1.4, 1.4, 1.4))
    prop("BP_Human_ExpeditionLight", (4100, 540, 0), (0, 0, 0), (1.0, 1.0, 1.0))

    # Pipe run along the north flank.
    for index in range(4):
        mesh("SM_Erebus_Pipe_Large_A", (1250 + index * 596.0, 700, 0), (0, 0, 0), label="PipeRun_%d" % index)
        mesh("SM_Erebus_PipeSupport_A", (1250 + index * 596.0, 700, 0), (0, 0, 90))
    mesh("SM_Erebus_Pipe_Elbow_A", (3630, 700, 0), (0, 0, 0))

    # Bunker position (authored assembly) where the damaged wall stood.
    prop("BP_Erebus_Bunker_A", (1900, 540, 0), (0, 0, -8), (1.0, 1.0, 1.0))
    mesh("SM_Erebus_Wreckage_B", (1220, 300, 0), (0, 0, -12), (1.1, 1.1, 1.1), label="LogisticsWreck")
    mesh("SM_Erebus_Barrel_A", (1180, 250, 0), (0, 0, 30))

    # Banner monoliths: the reference's dominant midground vocabulary.
    mesh("SM_Erebus_Monolith_A", (950, -880, 0), (0, 0, 186), (1.0, 1.0, 1.0), label="Monolith_A")
    mesh("SM_Erebus_Monolith_A", (2150, 880, 0), (0, 0, -8), (0.9, 0.9, 1.15), label="Monolith_B")
    mesh("SM_Erebus_Monolith_A", (3350, -860, 0), (0, 0, 172), (1.15, 1.15, 1.25), label="Monolith_C")
    mesh("SM_Erebus_Monolith_A", (4600, 940, 0), (0, 0, 5), (1.1, 1.1, 1.5), label="Monolith_D")

    # Gantry tower on the right flank.
    mesh("SM_Erebus_GantryTower_A", (1650, 1080, 0), (0, 0, 4))

    # Utility poles pace the route.
    for index in range(4):
        mesh("SM_Erebus_UtilityPole_A", (500 + index * 900.0, 850, 0), ((index % 3) * 2 - 2, 0, index * 40))
    mesh("SM_Erebus_CableSupport_A", (2900, -600, 0), (0, 0, 20))
    mesh("SM_Erebus_IndustrialSupport_A", (2440, 680, 40), (0, -13, 0), label="FallenBeam")

    # Ruined facades wall in the corridor edges.
    facades = [(600, -1450, 4, "A"), (2600, -1520, -6, "A"), (3800, 1500, 182, "B"),
               (5200, -1600, 8, "B"), (6100, 1560, 176, "A")]
    for index, (fx, fy, yaw, variant) in enumerate(facades):
        mesh("SM_Erebus_RuinedFacade_" + variant, (fx, fy, 0), (0, 0, yaw),
             (1.6, 1.6, 1.7), label="Facade_%d" % index)

    light((-960, -210, 280), (1.0, 0.42, 0.12), 3600.0, 760.0, "BarrelFire")
    light((1180, 260, 150), (1.0, 0.23, 0.06), 5200.0, 900.0, "WreckFire")
    light((2160, 500, 350), (1.0, 0.48, 0.12), 2400.0, 560.0, "BunkerLamp")
    light((950, -780, 210), (1.0, 0.34, 0.09), 6200.0, 1200.0, "MonolithFire")


def build_background():
    # Two depth rows of ruined city mass framing the corridor (silhouettes, non-colliding).
    near_row = [(4800, -1700, 1.2, "A"), (5520, 1850, 1.4, "B"), (6240, -1600, 1.1, "A"),
                (6960, 1750, 1.6, "B"), (7680, -1900, 1.3, "A"), (8400, 1650, 1.2, "B"),
                (9120, -1750, 1.5, "A"), (9840, 1900, 1.3, "B"), (10560, -1650, 1.2, "A"),
                (11280, 1800, 1.7, "B"), (12000, -1850, 1.4, "A"), (12720, 1950, 1.3, "B")]
    for index, (bx, by, scale, variant) in enumerate(near_row):
        mesh("SM_Erebus_RuinBlock_" + variant, (bx, by, 0), (0, 0, (index * 37) % 20 - 10),
             (scale, scale, scale), label="Skyline_%d" % index)
    far_row = [(5400, 2900, 1.9, "B"), (6380, -2700, 1.7, "A"), (7360, 3100, 2.2, "B"),
               (8340, -2900, 1.8, "B"), (9320, 2700, 2.0, "A"), (10300, -3100, 2.3, "B"),
               (11280, 2900, 1.9, "B"), (12260, -2700, 2.1, "A"), (13240, 3000, 2.4, "B")]
    for index, (bx, by, scale, variant) in enumerate(far_row):
        mesh("SM_Erebus_RuinBlock_" + variant, (bx, by, 0), (0, 0, (index * 53) % 24 - 12),
             (scale, scale, scale), label="SkylineFar_%d" % index)

    # Cathedral: the destination landmark, center-right, partly in the smoke.
    mesh("SM_Erebus_CathedralSpire_A", (6800, 2400, 0), (0, 0, 15), (2.2, 2.2, 0.75), label="Cathedral_Main")
    mesh("SM_Erebus_CathedralSpire_B", (6300, 1700, 0), (0, 0, -35), (1.8, 1.8, 0.65), label="Cathedral_Flank")
    mesh("SM_Erebus_CathedralSpire_B", (7400, 3000, 0), (0, 0, 40), (1.6, 1.6, 0.6), label="Cathedral_Flank2")
    mesh("SM_Erebus_RuinBlock_B", (9900, 1000, 0), (0, 0, 8), (2.2, 2.2, 1.1), label="CathedralBase_A")
    mesh("SM_Erebus_RuinBlock_B", (10600, 700, 0), (0, 0, -14), (2.0, 2.0, 0.9), label="CathedralBase_B")


def build_vfx_and_decals():
    # Niagara lives in the runtime director (BuildErebusZoneEffects): components saved
    # into the streamed level do not render in packaged builds. Lights/decals stay here.
    light((6100, -1900, 300), (1.0, 0.36, 0.10), 9000.0, 3000.0, "DistantFire_S")
    light((8400, 2300, 300), (1.0, 0.32, 0.08), 9000.0, 3200.0, "DistantFire_N")

    decal("MI_Erebus_Decal_Scorch", (-990, -240, 6), (0, -90, 0), (80, 190, 190), "Scorch_Barrel")
    decal("MI_Erebus_Decal_Scorch", (950, -800, 6), (0, -90, 30), (110, 320, 320), "Scorch_MonolithFire")
    decal("MI_Erebus_Decal_Scorch", (1200, 280, 6), (0, -90, -20), (90, 260, 260), "Scorch_Wreck")
    decal("MI_Erebus_Decal_Grime", (-2100, 0, 180), (0, 0, 0), (70, 300, 220), "Grime_RearWall")
    decal("MI_Erebus_Decal_Grime", (952, -822, 400), (0, 0, 6), (70, 380, 420), "Grime_Monolith_A")
    decal("MI_Erebus_Decal_Grime", (2148, 822, 420), (0, 0, 186), (70, 380, 420), "Grime_Monolith_B")
    decal("MI_Erebus_Decal_Scorch", (760, -600, 120), (0, 0, 8), (60, 240, 200), "Scorch_BlastWall")


def run():
    open_clean_level()
    build_ground()
    build_spawn_area()
    build_defensive_line()
    build_midground()
    build_background()
    build_vfx_and_decals()
    if not LEVELS.save_current_level():
        raise RuntimeError("Could not save " + LEVEL_PATH)
    unreal.log("[ErebusZone] authored %d actors (%d missing assets: %s)" % (
        len(SPAWNED), len(MISSING), ", ".join(sorted(set(MISSING))) if MISSING else "none"))
    if MISSING:
        unreal.log_error("[ErebusZone] missing assets prevented a complete zone build")


run()
