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
    # Mud base fields under everything.
    for column, cx in enumerate((-1600, 600, 2800, 5000, 7200)):
        for row, cy in enumerate((-1100, 1100)):
            mesh("SM_Erebus_MudBase_A", (cx, cy, -24), (0, 0, (column * 2 + row) % 4 * 90),
                 label="Mud_%d_%d" % (column, row))

    # Broken road: a cracked slab spine down the corridor with curbs and drainage,
    # so the route reads road-through-mud instead of one flat grey plane.
    slab_x = -1500
    index = 0
    while slab_x < 3200:
        for sy in (-190, 70):
            jitter_yaw = ((index * 41) % 14) - 7
            jitter_y = ((index * 67) % 60) - 30
            mesh("SM_Erebus_RoadSlab_Cracked_A", (slab_x, sy + jitter_y, -14),
                 (0, 0, jitter_yaw + (0 if index % 3 else 90)), label="Road_%d" % index)
            index += 1
        slab_x += 505
    for cx in range(-1400, 3200, 520):
        mesh("SM_Erebus_Curb_A", (cx, -370, -10), (0, 0, ((cx // 520) % 5) - 2), label="CurbS_%d" % cx)
        mesh("SM_Erebus_Curb_A", (cx + 260, 350, -8), (0, 0, ((cx // 520) % 7) - 3), label="CurbN_%d" % cx)
    for dx in (-900, 400, 1700):
        mesh("SM_Erebus_DrainChannel_A", (dx, -300, -10), (0, 0, 90), label="Drain_%d" % dx)

    # Old ground slabs remain as broken pavement patches off the road spine.
    for index, (sx, sy, yaw) in enumerate([(-760, -520, -8), (620, 510, 7), (2080, -520, -6), (3140, 470, -2)]):
        mesh("SM_Erebus_GroundSlab_A", (sx, sy, -16), (0, 0, yaw), label="Pavement_%d" % index)

    # Craters with raised rims where the fires burn (visible fire sources).
    mesh("SM_Erebus_CraterPatch_A", (-350, -620, -12), (0, 0, 20), (1.2, 1.2, 1.0), label="Crater_Fire")
    mesh("SM_Erebus_CraterPatch_A", (1900, -260, -12), (0, 0, 130), (1.5, 1.5, 1.0), label="Crater_Road")
    mesh("SM_Erebus_CraterPatch_A", (2620, 480, -12), (0, 0, 260), (1.1, 1.1, 1.0), label="Crater_N")

    # Integrated puddle sets in depressions, darker rims via grime decals below.
    puddles = [(-760, -330, 1.6, 40), (-320, 220, 1.9, 130), (340, -110, 2.4, 200),
               (1240, 420, 1.8, 310), (2300, -260, 2.1, 80), (-1150, 150, 1.4, 250), (1800, 140, 1.6, 20)]
    for index, (px, py, size, yaw) in enumerate(puddles):
        mesh("SM_Erebus_PuddleSet_A", (px, py, 2), (0, 0, yaw), (size, size * 0.8, 1.0),
             label="Puddle_%d" % index)
        decal("MI_Erebus_Decal_Grime", (px, py, 8), (0, -90, yaw), (40, 240 * size, 210 * size),
              "PuddleRim_%d" % index)

    # Rubble berms at the building feet tie architecture into the ground.
    for index, (bx, by, yaw, s) in enumerate([(-1500, -1000, 8, 1.2), (-600, -1050, -12, 1.4),
                                              (500, -1080, 4, 1.3), (1600, -1020, -6, 1.5),
                                              (-1100, 980, 174, 1.3), (100, 1020, 186, 1.2),
                                              (1200, 1000, 178, 1.4), (2400, 1040, 182, 1.3),
                                              (2700, -1060, 10, 1.2)]):
        mesh("SM_Erebus_RubbleBerm_A", (bx, by, -8), (0, 0, yaw), (s, s, s), label="Berm_%d" % index)

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


def build_street_architecture():
    """Generation-2 corridor architecture: layered fortress facades with secondary
    structure replacing the featureless dark planes the visual gate rejected.
    South side = -Y (details face +Y via yaw 180); north side = +Y (yaw 0)."""
    # South street wall, spawn to midground: stepped depth, alternating language.
    south = [
        ("SM_Erebus_Facade_Heavy_A", -1750, -1290, 180, 1.0),
        ("SM_Erebus_Facade_Broken_A", -150, -1340, 176, 1.1),
        ("SM_Erebus_ServiceBay_A", 900, -1260, 184, 1.2),
        ("SM_Erebus_Facade_Heavy_B", 1750, -1380, 180, 1.15),
        ("SM_Erebus_StructureFrame_A", 2650, -1300, 178, 1.4),
        ("SM_Erebus_Facade_Heavy_A", 3600, -1420, 182, 1.2),
    ]
    for index, (name, fx, fy, yaw, s) in enumerate(south):
        mesh(name, (fx, fy, 0), (0, 0, yaw), (s, s, s), label="StreetS_%d" % index)
    # North street wall.
    north = [
        ("SM_Erebus_Facade_Heavy_B", -1550, 1300, 0, 1.1),
        ("SM_Erebus_ServiceBay_Destroyed_A", -450, 1240, -4, 1.25),
        ("SM_Erebus_Facade_Heavy_A", 550, 1360, 2, 1.05),
        ("SM_Erebus_Facade_Broken_A", 1900, 1330, -2, 1.15),
        ("SM_Erebus_StructureFrame_B", 2900, 1260, 6, 1.5),
        ("SM_Erebus_Facade_Heavy_B", 3700, 1400, -3, 1.2),
    ]
    for index, (name, fx, fy, yaw, s) in enumerate(north):
        mesh(name, (fx, fy, 0), (0, 0, yaw), (s, s, s), label="StreetN_%d" % index)

    # Rooflines: jagged ruin edges + frame silhouettes break the flat parapets.
    for index, (ex, ey, ez, yaw, variant) in enumerate([
            (-1750, -1270, 2530, 180, "B"), (-150, -1320, 2780, 176, "A"),
            (1750, -1360, 2400, 180, "B"), (3600, -1400, 3030, 182, "A"),
            (-1550, 1280, 2300, 0, "A"), (550, 1340, 2660, 2, "B"),
            (1900, 1310, 2900, -2, "A"), (3700, 1380, 2510, -3, "B")]):
        mesh("SM_Erebus_RuinEdge_" + variant, (ex, ey, ez), (0, 0, yaw), (1.4, 1.4, 1.4),
             label="Roofline_%d" % index)
    mesh("SM_Erebus_StructureFrame_B", (-150, -1500, 2530), (0, 0, 174), (1.1, 1.1, 1.1), label="RoofFrame_S")
    mesh("SM_Erebus_StructureFrame_A", (550, 1520, 2620), (0, 0, 4), (0.9, 0.9, 0.9), label="RoofFrame_N")

    # Street-level infrastructure at the building feet (tertiary read).
    mesh("SM_Erebus_VentBank_A", (-1350, -1180, 0), (0, 0, 180), label="Vent_S0")
    mesh("SM_Erebus_PanelBank_A", (-700, -1190, 0), (0, 0, 176), label="Panels_S0")
    mesh("SM_Erebus_IndustrialDoor_A", (-2050, -1170, 0), (0, 0, 184), label="Door_S0")
    mesh("SM_Erebus_VentBank_A", (300, 1180, 0), (0, 0, -2), label="Vent_N0")
    mesh("SM_Erebus_PanelBank_A", (1300, 1190, 0), (0, 0, 4), label="Panels_N0")
    mesh("SM_Erebus_IndustrialDoor_A", (2350, 1180, 0), (0, 0, -4), label="Door_N0")
    mesh("SM_Erebus_Overhang_A", (-700, -1215, 380), (0, 0, 180), (1.1, 1.1, 1.1), label="Overhang_S")
    mesh("SM_Erebus_Overhang_A", (1300, 1215, 420), (0, 0, 0), (1.2, 1.2, 1.2), label="Overhang_N")

    # Catwalk fragment clinging to the broken facade + heavy columns pacing the wall.
    mesh("SM_Erebus_Catwalk_A", (-150, -1180, 860), (0, 0, 176), (1.2, 1.2, 1.2), label="Catwalk_S")
    mesh("SM_Erebus_CatwalkSupport_A", (-400, -1170, 530), (0, 0, 176), label="CatwalkSup_S0")
    mesh("SM_Erebus_Catwalk_A", (1900, 1170, 940), (0, 14, -2), (1.1, 1.1, 1.1), label="Catwalk_N_Fallen")
    for index, cx in enumerate((-1100, 100, 1250, 2400)):
        mesh("SM_Erebus_ColumnHeavy_A", (cx, -1140, 0), (0, 0, index * 45), label="ColS_%d" % index)
        mesh("SM_Erebus_ColumnHeavy_A", (cx + 550, 1140, 0), (0, 0, index * 30 + 10), label="ColN_%d" % index)
    # Leaning heavy beams tie street to walls; collapsed catwalk debris below it.
    mesh("SM_Erebus_BeamHeavy_A", (2650, -1050, 40), (0, -24, 30), label="LeanBeam_S")
    mesh("SM_Erebus_BeamHeavy_A", (2050, 1080, 20), (0, -18, -140), label="LeanBeam_N")


def build_spawn_area():
    # Rear boundary: broken facade + frame silhouettes close the view behind spawn.
    mesh("SM_Erebus_Facade_Broken_A", (-2350, -700, 0), (0, 0, 92), (1.2, 1.2, 1.2), label="RearFacade_S")
    mesh("SM_Erebus_Facade_Heavy_B", (-2400, 500, 0), (0, 0, 88), (1.15, 1.15, 1.3), label="RearFacade_N")
    mesh("SM_Erebus_StructureFrame_B", (-2250, 1300, 0), (0, 0, 96), (1.3, 1.3, 1.3), label="RearFrame")
    mesh("SM_Erebus_RuinEdge_B", (-2350, -680, 3030), (0, 0, 92), (1.3, 1.3, 1.3), label="RearRoofline")

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
    # Fortification row on the south flank.
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

    # Pipe run along the north flank, with a ruptured joint burning at x=2620
    # (visible source for the runtime pipe-fire effect).
    for index in range(4):
        mesh("SM_Erebus_Pipe_Large_A", (1250 + index * 596.0, 700, 0), (0, 0, 0), label="PipeRun_%d" % index)
        mesh("SM_Erebus_PipeSupport_A", (1250 + index * 596.0, 700, 0), (0, 0, 90))
    mesh("SM_Erebus_Pipe_Elbow_A", (3630, 700, 0), (0, 0, 0))
    mesh("SM_Erebus_Pipe_Elbow_A", (2620, -700, 0), (0, 0, 90), label="RupturedPipe")
    mesh("SM_Erebus_Wreckage_B", (2560, -620, 0), (0, 0, 140), (0.9, 0.9, 0.9), label="RupturedPipeDebris")

    # Bunker position (authored assembly).
    prop("BP_Erebus_Bunker_A", (1900, 540, 0), (0, 0, -8), (1.0, 1.0, 1.0))

    # Burning wreck at the defensive line right flank: the visible source for the
    # runtime wreck-fire effect at (1500, 560).
    prop("BP_Erebus_WreckCluster_A", (1500, 560, 0), (0, 0, -35), (1.15, 1.15, 1.15))
    mesh("SM_Erebus_Barrel_A", (1400, 480, 0), (0, 0, 30), label="WreckBarrel")

    # Banner monoliths: the reference's dominant midground vocabulary — plinth bases
    # tie them into the ground like the reference's fortress blocks.
    mesh("SM_Erebus_Monolith_A", (950, -880, 0), (0, 0, 186), (1.0, 1.0, 1.0), label="Monolith_A")
    mesh("SM_Erebus_Monolith_A", (2150, 880, 0), (0, 0, -8), (0.9, 0.9, 1.15), label="Monolith_B")
    mesh("SM_Erebus_Monolith_A", (3350, -860, 0), (0, 0, 172), (1.15, 1.15, 1.25), label="Monolith_C")
    mesh("SM_Erebus_Monolith_A", (4600, 940, 0), (0, 0, 5), (1.1, 1.1, 1.5), label="Monolith_D")
    mesh("SM_Erebus_RubbleBerm_A", (950, -760, -6), (0, 0, 96), (1.3, 1.3, 1.1), label="MonolithBerm_A")
    mesh("SM_Erebus_RubbleBerm_A", (2150, 760, -6), (0, 0, 274), (1.2, 1.2, 1.0), label="MonolithBerm_B")

    # Gantry tower on the right flank.
    mesh("SM_Erebus_GantryTower_A", (1650, 1080, 0), (0, 0, 4))

    # NO standing utility poles on the corridor axis: from the spawn sightline a
    # crossarm against the sky reads as a giant cross (visual gate §18). Poles are
    # fallen or lean far off-axis, crossarms never silhouette against the sky gap.
    mesh("SM_Erebus_UtilityPole_A", (900, 1120, 8), (66, 12, 30), label="LeaningPole_N")
    mesh("SM_Erebus_UtilityPole_A", (2450, -1080, 10), (-58, 20, 205), label="FallenPole_S")
    mesh("SM_Erebus_CableSupport_A", (2900, -600, 0), (0, 0, 20))
    mesh("SM_Erebus_CableSupport_A", (1150, 1060, 0), (74, 0, 130), label="FallenCableSupport")
    mesh("SM_Erebus_IndustrialSupport_A", (2440, 680, 40), (0, -13, 0), label="FallenBeam")

    # Cool ambient bounce fills: fake GI so the street walls read in the overcast
    # key instead of crushing to black (no Lumen on the Mac profile).
    light((-900, 0, 620), (0.42, 0.47, 0.58), 1100.0, 2600.0, "CorridorFill_A")
    light((600, 0, 640), (0.42, 0.47, 0.58), 1100.0, 2600.0, "CorridorFill_B")
    light((2100, 0, 660), (0.42, 0.47, 0.58), 1000.0, 2600.0, "CorridorFill_C")
    light((-960, -210, 280), (1.0, 0.42, 0.12), 3600.0, 760.0, "BarrelFire")
    light((-350, -620, 90), (1.0, 0.38, 0.10), 4200.0, 720.0, "CraterFire")
    light((1500, 560, 160), (1.0, 0.26, 0.07), 5600.0, 950.0, "WreckFire")
    light((2160, 500, 350), (1.0, 0.48, 0.12), 2400.0, 560.0, "BunkerLamp")
    light((2620, -700, 220), (1.0, 0.33, 0.09), 3800.0, 680.0, "PipeFire")
    light((950, -780, 210), (1.0, 0.34, 0.09), 6200.0, 1200.0, "MonolithFire")


def build_background():
    # Three skyline depth layers (visual gate §19), separated by the fog gradient.
    # Near: damaged local architecture with readable silhouette damage.
    near_row = [(4800, -1700, 1.2, "A"), (5520, 1850, 1.4, "B"), (6240, -1600, 1.1, "A"),
                (6960, 1750, 1.6, "B"), (7680, -1900, 1.3, "A"), (8400, 1650, 1.2, "B")]
    for index, (bx, by, scale, variant) in enumerate(near_row):
        mesh("SM_Erebus_RuinBlock_" + variant, (bx, by, 0), (0, 0, (index * 37) % 20 - 10),
             (scale, scale, scale), label="Skyline_%d" % index)
        if index % 2 == 0:
            mesh("SM_Erebus_RuinEdge_B", (bx, by - 260 * (1 if by < 0 else -1), 1560 * scale),
                 (0, 0, (index * 37) % 20 - 10), (1.8, 1.8, 2.0), label="SkylineEdge_%d" % index)
    mesh("SM_Erebus_Facade_Broken_A", (4600, 1650, 0), (0, 0, -8), (1.5, 1.5, 1.6), label="NearRuinFacade_N")
    mesh("SM_Erebus_StructureFrame_B", (5300, -1750, 0), (0, 0, 14), (2.2, 2.2, 2.4), label="NearRuinFrame_S")

    # Mid: larger industrial blocks partially lost in haze.
    mid_row = [(9120, -1750, 1.5, "A"), (9840, 1900, 1.3, "B"), (10560, -1650, 1.2, "A"),
               (11280, 1800, 1.7, "B"), (12000, -1850, 1.4, "A"), (12720, 1950, 1.3, "B"),
               (9500, 2900, 1.9, "B"), (10300, -3100, 2.3, "B"), (11280, 2900, 1.9, "B")]
    for index, (bx, by, scale, variant) in enumerate(mid_row):
        mesh("SM_Erebus_RuinBlock_" + variant, (bx, by, 0), (0, 0, (index * 53) % 24 - 12),
             (scale, scale, scale), label="SkylineMid_%d" % index)

    # Far: continuous city mass closing the vanishing point, plus flank masses.
    far_row = [(14200, -600, 3.0, "B"), (14800, 900, 3.4, "B"), (15400, -1800, 3.2, "A"),
               (16000, 300, 3.8, "B"), (15200, 2600, 3.0, "B"), (16600, -3000, 3.5, "A"),
               (17200, 1800, 3.6, "B"), (14600, -3600, 2.8, "B"), (16200, 3800, 3.2, "A"),
               (17800, -900, 4.0, "B"), (18400, 2800, 3.4, "B")]
    for index, (bx, by, scale, variant) in enumerate(far_row):
        mesh("SM_Erebus_RuinBlock_" + variant, (bx, by, 0), (0, 0, (index * 29) % 30 - 15),
             (scale, scale, scale), label="SkylineFar_%d" % index)

    # Cathedral (visual gate §17): enormous, vertical, structurally unique, largely
    # black, substantially taller than the human skyline. Fluted tower cluster right
    # of the corridor axis so it dominates the gap without blocking the route read.
    # Nearly on-axis so the towers punch through the corridor sky gap above the
    # transit gate from the spawn/reference view, instead of hiding behind the
    # street wall (§17: the landmark must be obvious and pull the eye).
    mesh("SM_Erebus_CathedralTower_A", (9200, 500, 0), (0, 0, 20), (1.0, 1.0, 1.0), label="Cathedral_Main")
    mesh("SM_Erebus_CathedralTower_B", (8300, 1400, 0), (0, 0, -15), (1.0, 1.0, 1.0), label="Cathedral_Flank")
    mesh("SM_Erebus_CathedralTower_C", (10200, -400, 0), (0, 0, 45), (1.0, 1.0, 1.0), label="Cathedral_Fore")
    mesh("SM_Erebus_RuinBlock_B", (8900, 1000, 0), (0, 0, 8), (2.2, 2.2, 1.1), label="CathedralBase_A")
    mesh("SM_Erebus_RuinBlock_B", (9600, 200, 0), (0, 0, -14), (2.0, 2.0, 0.9), label="CathedralBase_B")


def build_vfx_and_decals():
    # Niagara lives in the runtime director (BuildErebusZoneEffects): components saved
    # into the streamed level do not render in packaged builds. Lights/decals stay here.
    light((6100, -1900, 300), (1.0, 0.36, 0.10), 9000.0, 3000.0, "DistantFire_S")
    light((8400, 2300, 300), (1.0, 0.32, 0.08), 9000.0, 3200.0, "DistantFire_N")

    decal("MI_Erebus_Decal_Scorch", (-990, -240, 6), (0, -90, 0), (80, 190, 190), "Scorch_Barrel")
    decal("MI_Erebus_Decal_Scorch", (-350, -620, 6), (0, -90, 70), (100, 300, 300), "Scorch_Crater")
    decal("MI_Erebus_Decal_Scorch", (950, -800, 6), (0, -90, 30), (110, 320, 320), "Scorch_MonolithFire")
    decal("MI_Erebus_Decal_Scorch", (1500, 560, 6), (0, -90, -20), (90, 280, 280), "Scorch_Wreck")
    decal("MI_Erebus_Decal_Scorch", (2620, -700, 6), (0, -90, 45), (80, 240, 240), "Scorch_Pipe")
    decal("MI_Erebus_Decal_Grime", (-2100, 0, 180), (0, 0, 0), (70, 300, 220), "Grime_RearWall")
    decal("MI_Erebus_Decal_Grime", (952, -822, 400), (0, 0, 6), (70, 380, 420), "Grime_Monolith_A")
    decal("MI_Erebus_Decal_Grime", (2148, 822, 420), (0, 0, 186), (70, 380, 420), "Grime_Monolith_B")
    decal("MI_Erebus_Decal_Scorch", (760, -600, 120), (0, 0, 8), (60, 240, 200), "Scorch_BlastWall")
    decal("MI_Erebus_Decal_Grime", (-150, -1320, 300), (0, 0, 176), (80, 520, 620), "Grime_FacadeBreak_S")
    decal("MI_Erebus_Decal_Grime", (550, 1330, 340), (0, 0, 2), (80, 520, 620), "Grime_Facade_N")
    decal("MI_Erebus_Decal_Scorch", (-450, 1220, 220), (0, 0, -4), (80, 420, 380), "Scorch_ServiceBay_N")


def run():
    open_clean_level()
    build_ground()
    build_street_architecture()
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
