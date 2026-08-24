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
    # Mud base fields under everything — extra rows cover the widened vista floor.
    for column, cx in enumerate((-4000, -1600, 600, 2800, 5000, 7200)):
        for row, cy in enumerate((-1100, 1100, -3200, 3200, -5400, 5400)):
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
    mesh("SM_Erebus_CraterPatch_A", (-650, -560, -12), (0, 0, 20), (1.2, 1.2, 1.0), label="Crater_Fire")
    mesh("SM_Erebus_CraterPatch_A", (1900, -260, -12), (0, 0, 130), (1.5, 1.5, 1.0), label="Crater_Road")
    mesh("SM_Erebus_CraterPatch_A", (2620, 480, -12), (0, 0, 260), (1.1, 1.1, 1.0), label="Crater_N")

    # Integrated puddle sets in depressions, darker rims via grime decals below.
    puddles = [(-760, -330, 1.6, 40), (-320, 220, 1.9, 130), (340, -110, 2.4, 200),
               (1240, 420, 1.8, 310), (2300, -260, 2.1, 80), (-1150, 150, 1.4, 250), (1800, 140, 1.6, 20),
               (700, 260, 2.0, 95), (2900, 120, 2.3, 170), (-550, -60, 1.5, 305), (1520, -200, 1.7, 250)]
    for index, (px, py, size, yaw) in enumerate(puddles):
        mesh("SM_Erebus_PuddleSet_A", (px, py, 2), (0, 0, yaw), (size, size * 0.8, 1.0),
             label="Puddle_%d" % index)
        decal("MI_Erebus_Decal_Grime", (px, py, 8), (0, -90, yaw), (40, 240 * size, 210 * size),
              "PuddleRim_%d" % index)

    # Rubble berms: some at the new wall feet, some as mid-field battlefield mounds.
    for index, (bx, by, yaw, s) in enumerate([(-1500, -1750, 8, 1.4), (-600, -1800, -12, 1.5),
                                              (1600, -1750, -6, 1.5), (2700, -1820, 10, 1.4),
                                              (-1100, 1700, 174, 1.4), (100, 1750, 186, 1.3),
                                              (1200, 1720, 178, 1.5), (2400, 1780, 182, 1.4),
                                              (-600, -1050, -12, 1.2), (1600, -1020, -6, 1.2),
                                              (100, 1020, 186, 1.1), (2400, 1040, 182, 1.1)]):
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
        mesh("SM_Erebus_BrokenFloor_A", (fx, fy, -26), (0, 2, yaw), label="BrokenFloor_%d" % index)

    # Baked debris fields: churned-battlefield ground storytelling in single
    # draw calls (visual gate sections 14/22 - clusters, not carpet).
    for index, (dx, dy, variant, yaw, s) in enumerate([
            (-1150, -420, "A", 30, 1.0), (-400, 350, "A", 140, 1.1), (550, -520, "B", 75, 1.0),
            (1350, 420, "A", 210, 1.2), (2250, -350, "B", 160, 1.1), (3300, 250, "A", 20, 1.3),
            (-800, 900, "B", 300, 0.9), (1900, -900, "B", 250, 1.2)]):
        # z 0 / flatter scale: at z=2 the cut plane rim silhouetted as a floating
        # disc against the mud (gate feedback round 4 'circular blockout object').
        mesh("SM_Erebus_DebrisField_" + variant, (dx, dy, 0), (0, 0, yaw), (s, s, 0.45),
             label="Debris_%d" % index)


def build_street_architecture():
    """Phase 4.8 recompose for the visual gate. The chapter is a ~350m diorama
    (1uu = 1cm), so the reference's composition maps to: huge fortress masses
    anchoring both frame edges 20-30m out (cropped by the frame top), an
    elevated fortress block at mid-left, a near tower slab at right, layered
    ruins dissolving into haze beyond, and an open sky lane down the +X axis
    for the distant Cathedral. Comparison camera: (-1380,-120,65) yaw 0.
    South side = -Y (route faces +Y via yaw 180); north side = +Y (yaw 0)."""
    # --- Frame-edge anchors (primary masses, crop the frame top) ---
    mesh("SM_Erebus_Fortress_A", (700, -2150, 0), (0, 0, 184), (1.0, 1.0, 1.0),
         label="Anchor_Fortress_L")
    mesh("SM_Erebus_BannerDrape_B", (350, -1190, 3300), (0, 0, 184), (2.0, 2.0, 1.7),
         label="Banner_Fortress_L")
    mesh("SM_Erebus_TowerSlab_A", (900, 1950, 0), (0, 0, -6), (1.0, 1.0, 1.0),
         label="Anchor_Tower_R")
    mesh("SM_Erebus_BannerDrape_A", (830, 1400, 3300), (0, 0, -6), (1.0, 1.0, 1.0),
         label="Banner_Tower_R")

    # --- Mid-left: the elevated fortress block (reference center-left icon) ---
    mesh("SM_Erebus_Fortress_B", (3700, -2250, 0), (0, 0, 172), (1.0, 1.0, 1.0),
         label="Mid_Fortress_Elevated")
    mesh("SM_Erebus_RubbleBerm_A", (3100, -1750, -8), (0, 0, 20), (2.0, 2.0, 1.4),
         label="Mid_Fortress_Apron")

    # --- Mid-right: heavy facade + destroyed service bay, banners hung ---
    mesh("SM_Erebus_Facade_Heavy_A", (3100, 2400, 0), (0, 0, -8), (1.3, 1.3, 1.3),
         label="Mid_Facade_R")
    mesh("SM_Erebus_BannerDrape_B", (2650, 1690, 2500), (0, 0, -8), (1.0, 1.0, 1.0),
         label="Banner_Facade_R")
    mesh("SM_Erebus_ServiceBay_Destroyed_A", (1750, 1650, 0), (0, 0, -4), (1.25, 1.25, 1.25),
         label="Mid_ServiceBay_R")

    # --- Rear-flank fills behind the anchors (silhouette continuity) ---
    south = [
        ("SM_Erebus_Facade_Broken_A", -900, -2900, 170, 1.25),
        ("SM_Erebus_StructureFrame_B", 5300, -3100, 168, 1.5),
        ("SM_Erebus_Facade_Heavy_B", -2200, -2400, 120, 1.2),
        ("SM_Erebus_RuinedFacade_B", 1500, -3300, 8, 1.6),
        ("SM_Erebus_Facade_Heavy_B", 6300, -3400, 182, 1.4),
        ("SM_Erebus_StructureFrame_A", 5500, -2500, 178, 1.4),
    ]
    for index, (name, fx, fy, yaw, s) in enumerate(south):
        mesh(name, (fx, fy, 0), (0, 0, yaw), (s, s, s), label="StreetS_%d" % index)
    north = [
        ("SM_Erebus_Facade_Heavy_B", -1600, 2300, 24, 1.25),
        ("SM_Erebus_StructureFrame_B", 2600, 2900, -6, 1.5),
        ("SM_Erebus_Facade_Broken_A", 4300, 2600, -2, 1.35),
        ("SM_Erebus_RuinBlock_A", 5600, 3300, -12, 1.3),
        ("SM_Erebus_Facade_Heavy_A", 7000, 3800, -3, 1.4),
    ]
    for index, (name, fx, fy, yaw, s) in enumerate(north):
        mesh(name, (fx, fy, 0), (0, 0, yaw), (s, s, s), label="StreetN_%d" % index)

    # Roofline damage caps on the most visible rear masses.
    for index, (ex, ey, ez, yaw, variant) in enumerate([
            (-900, -2880, 2760, 170, "B"), (-1600, 2280, 2450, 24, "A"),
            (4300, 2580, 2900, -2, "B")]):
        mesh("SM_Erebus_RuinEdge_" + variant, (ex, ey, ez), (0, 0, yaw), (1.5, 1.5, 1.5),
             label="Roofline_%d" % index)

    # --- Vista rubble aprons tying the anchors into the ground ---
    mesh("SM_Erebus_RubbleBerm_A", (200, -1450, -8), (0, 0, 24), (1.8, 1.8, 1.3), label="VistaBerm_A")
    mesh("SM_Erebus_RubbleBerm_A", (900, -1600, -8), (0, 0, -18), (2.0, 2.0, 1.5), label="VistaBerm_B")
    mesh("SM_Erebus_RubbleBerm_A", (1400, 1500, -8), (0, 0, 156), (1.7, 1.7, 1.2), label="VistaBerm_C")
    for index, (rx, ry) in enumerate([(300, -1750), (750, -1950), (1250, -1800), (700, 1700)]):
        mesh("SM_Erebus_RubbleLarge_A", (rx, ry, -6), (0, 0, index * 77), (1.4, 1.4, 1.4),
             label="VistaRubble_%d" % index)

    # --- Street-level infrastructure at the anchor feet (tertiary read) ---
    mesh("SM_Erebus_VentBank_A", (-100, -2280, 0), (0, 0, 184), label="Vent_S0")
    mesh("SM_Erebus_PanelBank_A", (1300, -2350, 0), (0, 0, 180), label="Panels_S0")
    mesh("SM_Erebus_IndustrialDoor_A", (-500, -2300, 0), (0, 0, 184), label="Door_S0")
    mesh("SM_Erebus_VentBank_A", (500, 1780, 0), (0, 0, -6), label="Vent_N0")
    mesh("SM_Erebus_PanelBank_A", (1450, 1750, 0), (0, 0, -4), label="Panels_N0")
    mesh("SM_Erebus_IndustrialDoor_A", (250, 1800, 0), (0, 0, -6), label="Door_N0")
    mesh("SM_Erebus_Overhang_A", (-450, -2290, 380), (0, 0, 184), (1.1, 1.1, 1.1), label="Overhang_S")
    mesh("SM_Erebus_Overhang_A", (300, 1830, 420), (0, 0, -6), (1.2, 1.2, 1.2), label="Overhang_N")

    # Abandoned repair scaffold against the south fortress face + broken shop
    # signage on both flanks: street-level authored identity (gate feedback round 4).
    mesh("SM_Erebus_Scaffold_A", (1450, -1900, 0), (0, 0, 96), (1.15, 1.15, 1.15), label="Scaffold_S")
    mesh("SM_Erebus_SignFrame_A", (-150, -1330, 520), (0, 0, 4), (1.4, 1.4, 1.4), label="Sign_S")
    mesh("SM_Erebus_SignFrame_A", (1620, 1545, 430), (0, 0, 186), (1.3, 1.3, 1.3), label="Sign_N")

    mesh("SM_Erebus_Catwalk_A", (2300, -2550, 860), (0, 0, 168), (1.2, 1.2, 1.2), label="Catwalk_S")
    mesh("SM_Erebus_CatwalkSupport_A", (2100, -2520, 530), (0, 0, 168), label="CatwalkSup_S0")
    mesh("SM_Erebus_Catwalk_A", (2600, 2850, 940), (0, 14, -6), (1.1, 1.1, 1.1), label="Catwalk_N_Fallen")
    # Broken colonnade stubs (full columns sealed the sky in earlier rounds).
    for index, cx in enumerate((-1200, 0, 1150, 2300)):
        stub_s = 0.30 + (index % 3) * 0.14
        stub_n = 0.24 + ((index + 1) % 3) * 0.16
        mesh("SM_Erebus_ColumnHeavy_A", (cx, -2250, 0), (index * 3 - 4, 0, index * 45),
             (1.0, 1.0, stub_s), label="ColS_%d" % index)
        mesh("SM_Erebus_ColumnHeavy_A", (cx + 550, 2230, 0), (0, index * 2 - 3, index * 30 + 10),
             (1.0, 1.0, stub_n), label="ColN_%d" % index)
    mesh("SM_Erebus_ColumnHeavy_A", (600, -2100, 30), (0, -86, 20), (1.0, 1.0, 0.9), label="ColS_Fallen")
    mesh("SM_Erebus_BeamHeavy_A", (2750, -2150, 40), (0, -24, 30), label="LeanBeam_S")
    mesh("SM_Erebus_BeamHeavy_A", (2150, 2170, 20), (0, -18, -140), label="LeanBeam_N")


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
    # Pulled back-left and down to 1.0: at 1.2 next to the spawn it shoved the
    # new girder tangle into the corridor's vanishing point (round-4 D8 capture).
    prop("BP_Erebus_WreckCluster_A", (-1150, -840, 0), (0, 0, 45), (1.0, 1.0, 1.0))
    mesh("SM_Erebus_UtilityPole_A", (-1120, -280, 12), (24, 88, 0), label="FallenPole")
    mesh("SM_Erebus_Barrel_A", (-1120, -560, 0), (6, 0, 4), (1.1, 1.1, 1.1), label="BurnBarrel")
    mesh("SM_Erebus_RubbleMedium_A", (-1010, -420, -4), (0, 0, 140))
    mesh("SM_Erebus_CrateOpen_A", (-1260, -350, 0), (0, 0, 24))
    mesh("SM_Erebus_Crate_A", (-1290, -230, 0), (0, 0, -12))

    # Foreground pipes across the mud at the comparison frame's bottom-left.
    mesh("SM_Erebus_Pipe_Large_A", (-1010, -650, 0), (0, 4, 58), (1.1, 1.1, 1.1), label="FG_Pipe_A")
    mesh("SM_Erebus_Pipe_Large_A", (-860, -790, 8), (0, -3, 52), (1.0, 1.0, 1.0), label="FG_Pipe_B")
    mesh("SM_Erebus_PipeSupport_A", (-940, -700, 0), (0, 62, 55), label="FG_PipeSupport")
    mesh("SM_Erebus_RubbleMedium_A", (-1080, -740, -4), (0, 0, 96), (0.7, 0.7, 0.7), label="FG_PipeRubble")

    # Right flank pipe cluster.
    mesh("SM_Erebus_Pipe_Large_A", (-900, 700, 0), (0, 0, -35))
    mesh("SM_Erebus_Pipe_Large_A", (-880, 760, 84), (0, 0, -33), label="PipeStacked")
    mesh("SM_Erebus_Pipe_Elbow_A", (-460, 940, 0), (0, 0, 140))
    mesh("SM_Erebus_PipeSupport_A", (-1080, 690, 0), (0, 0, -15))
    mesh("SM_Erebus_PipeSupport_A", (-720, 720, 0), (0, 0, -15))
    mesh("SM_Erebus_SandbagRow_A", (-1350, 480, 0), (0, 0, 12), (1.0, 1.0, 0.72))
    # Near-camera right cover cluster, restaged (gate feedback round 4: the old
    # debris disc + plate stack read as circular blockout trash bottom-right).
    mesh("SM_Erebus_SandbagRow_A", (-820, 400, 0), (0, 0, 74), (1.1, 1.1, 0.8), label="FG_Sandbags_R")
    mesh("SM_Erebus_Crate_A", (-1000, 250, 0), (0, 0, 28), label="FG_Crate_R")
    mesh("SM_Erebus_CrateOpen_A", (-940, 360, 0), (0, 0, -35), label="FG_CrateOpen_R")
    mesh("SM_Erebus_Barrel_A", (-1060, 430, 0), (0, 0, 0), label="FG_Barrel_R")
    mesh("SM_Erebus_RubbleMedium_A", (-880, 190, -4), (0, 0, 200), (0.8, 0.8, 0.8), label="FG_Rubble_R")
    mesh("SM_Erebus_TankTrap_A", (-640, 500, 0), (0, 0, 35), (1.0, 1.0, 1.0), label="FG_TankTrap_R")
    mesh("SM_Erebus_TankTrap_A", (-460, 680, 0), (0, 0, -70), (0.9, 0.9, 0.9), label="FG_TankTrap_R2")
    mesh("SM_Erebus_PuddleSet_A", (-880, 560, 0.5), (0, 0, 210), (1.4, 1.1, 1.0), label="FG_Puddle_R")
    mesh("SM_Erebus_SignFrame_A", (-540, 760, 20), (12, 74, 160), (1.1, 1.1, 1.1), label="FG_FallenSign_R")


def build_defensive_line():
    prop("BP_Erebus_Barricade", (-620, -560, 0), (0, 0, -6), (1.2, 1.2, 1.2))
    prop("BP_Erebus_Barricade", (-580, 560, 0), (0, 0, 8), (1.15, 1.15, 1.15))
    prop("BP_Human_ExpeditionLight", (-590, -460, 0), (0, 0, 25), (1.0, 1.0, 1.0))
    mesh("SM_Erebus_SandbagRow_A", (-600, -220, 0), (0, 0, 86), (1.15, 1.15, 0.85), label="LineBags_S")
    mesh("SM_Erebus_ArmorBarrier_A", (-560, 240, 0), (0, 0, 155), label="LineBarrier_N")
    # Nothing tall dead-center: the corridor's vanishing point must stay open
    # (D12: even an armor plate here re-created the central-slab read). The
    # center is held by low sandbags and the worklight only.
    mesh("SM_Erebus_RubbleMedium_A", (-520, -420, -6), (0, 0, 240), (0.9, 0.9, 0.9), label="LineRubble")
    mesh("SM_Erebus_SandbagRow_A", (-640, -300, 0), (0, 0, 84), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_SandbagRow_A", (-655, -620, 0), (0, 0, 96), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_SandbagRow_A", (-620, 330, 0), (0, 0, 88), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_SandbagRow_A", (-600, 650, 0), (0, 0, 78), (1.0, 1.0, 0.72))
    mesh("SM_Erebus_Crate_A", (-700, 390, 0), (0, 0, 40))
    mesh("SM_Erebus_Barricade_A", (-540, -820, 0), (0, 0, 4))
    # Tank-trap pair on the line flanks: war-zone identity without fencing off the
    # corridor read (a full row of crossed silhouettes walled the center in D11).
    mesh("SM_Erebus_TankTrap_A", (-440, -420, 0), (0, 0, 15), label="LineTrap_A")
    mesh("SM_Erebus_TankTrap_A", (-390, 460, 0), (0, 0, 120), (0.95, 0.95, 0.95), label="LineTrap_D")
    light((-540, -140, 310), (1.0, 0.45, 0.15), 7800.0, 980.0, "LineLight")


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

    # Banner monoliths: midground vocabulary, pushed past the new fortress masses.
    mesh("SM_Erebus_Monolith_A", (7600, -2600, 0), (0, 0, 186), (1.0, 1.0, 1.0), label="Monolith_A")
    mesh("SM_Erebus_Monolith_A", (5000, 2600, 0), (0, 0, -8), (1.1, 1.1, 1.0), label="Monolith_B")
    mesh("SM_Erebus_Monolith_A", (9600, -2500, 0), (0, 0, 172), (1.3, 1.3, 1.0), label="Monolith_C")
    mesh("SM_Erebus_Monolith_A", (8000, 2400, 0), (0, 0, 5), (1.2, 1.2, 1.1), label="Monolith_D")
    mesh("SM_Erebus_RubbleBerm_A", (7600, -2460, -6), (0, 0, 96), (1.6, 1.6, 1.2), label="MonolithBerm_A")
    mesh("SM_Erebus_RubbleBerm_A", (5000, 2460, -6), (0, 0, 274), (1.5, 1.5, 1.1), label="MonolithBerm_B")

    # Corridor terminus: authored checkpoint gate reads as architecture where the
    # legacy transit posts silhouetted as floating boxes (visual gate sections 2/13).
    mesh("SM_Erebus_CheckpointGate_A", (3600, 0, 0), (0, 0, 0), (0.92, 0.92, 0.92), label="CheckpointGate")
    decal("MI_Erebus_Decal_Grime", (3450, -700, 500), (0, 0, 0), (90, 320, 480), "Grime_Gate_S")
    decal("MI_Erebus_Decal_Scorch", (3450, 640, 300), (0, 0, 0), (80, 260, 300), "Scorch_Gate_N")
    mesh("SM_Erebus_SandbagRow_A", (3260, -540, 0), (0, 0, 96), (1.0, 1.0, 0.72), label="GateBags_S")
    mesh("SM_Erebus_Barrel_A", (3300, 620, 0), (0, 0, 40), label="GateBarrel")
    # Gate checkpoint dressing: faction banners on both column faces toward the
    # player, tank traps flanking the road slot, warm floodlight pool under the
    # beam so the gate anchors the corridor instead of reading as dead mass.
    mesh("SM_Erebus_BannerDrape_A", (3440, -700, 1400), (0, 0, 90), (0.85, 0.85, 0.85), label="GateBanner_S")
    mesh("SM_Erebus_BannerDrape_B", (3440, 700, 1380), (0, 0, 90), (0.8, 0.8, 0.8), label="GateBanner_N")
    mesh("SM_Erebus_TankTrap_A", (3320, -320, 0), (0, 0, 25), label="GateTrap_S")
    mesh("SM_Erebus_TankTrap_A", (3350, 300, 0), (0, 0, -55), (1.1, 1.1, 1.1), label="GateTrap_N")
    mesh("SM_Erebus_WorkLight_A", (3380, -180, 0), (0, 0, 160), label="GateWorkLight")
    light((3380, 0, 1250), (1.0, 0.55, 0.22), 9500.0, 1500.0, "GateFlood")
    light((3300, 620, 160), (1.0, 0.40, 0.12), 2600.0, 620.0, "GateBarrelFire")

    # Overhead cable spans: sagging diagonals stage depth across the corridor
    # (off-axis so no crossarm silhouette can read as a cross).
    mesh("SM_Erebus_CableSpan_A", (400, -60, 880), (0, 0, 78), (1.0, 1.0, 1.0), label="CableSpan_A")
    mesh("SM_Erebus_CableSpan_A", (2050, 80, 820), (0, 0, 102), (1.0, 1.0, 1.0), label="CableSpan_B")
    mesh("SM_Erebus_CableSpan_B", (1250, -900, 720), (0, -34, 62), (1.0, 1.0, 1.0), label="CableSpan_Torn")

    # Blast-bent street lamps along the route: city furniture that stages the
    # approach and breaks the empty mid-street band.
    mesh("SM_Erebus_StreetLamp_Bent_A", (250, -470, 0), (0, 6, 35), label="Lamp_A")
    mesh("SM_Erebus_StreetLamp_Bent_A", (1750, 430, 0), (0, -5, -155), (1.05, 1.05, 1.05), label="Lamp_B")
    mesh("SM_Erebus_StreetLamp_Bent_A", (2950, -420, 0), (0, 8, 60), (0.95, 0.95, 0.95), label="Lamp_C")

    # Gantry tower on the right flank, past the tower slab.
    mesh("SM_Erebus_GantryTower_A", (5600, 2100, 0), (0, 0, 4))

    # --- Phase 4.7 hero pieces: specific destroyed objects, not generic modules ---
    # Knocked-out tank guarding the road edge; barrel drooped, hull breached.
    mesh("SM_Erebus_TankHulk_A", (380, -730, 0), (0, 0, -155), (1.0, 1.0, 1.0), label="Hero_TankHulk")
    mesh("SM_Erebus_RubbleMedium_A", (240, -640, -4), (0, 0, 60), (0.9, 0.9, 0.9), label="TankDebris_A")
    # Crashed gunship nose-in past the bunker; debris trail rakes back toward the road.
    # z -70: the crushed-belly boolean leaves the airframe bbox floor at +72, so the
    # wreck must sink to sit in the mud instead of hovering.
    mesh("SM_Erebus_GunshipWreck_A", (2120, -520, -70), (0, 0, -140), (1.15, 1.15, 1.15), label="Hero_Gunship")
    for index, (dx, dy, s) in enumerate([(1820, -380, 0.8), (1650, -290, 0.6), (1950, -460, 0.7)]):
        mesh("SM_Erebus_Wreckage_B", (dx, dy, -4), (0, 0, index * 111), (s, s, s),
             label="GunshipDebris_%d" % index)
    # Field gun dug in behind the north sandbag line, still aimed down the corridor.
    mesh("SM_Erebus_ArtilleryGun_A", (-250, 820, 0), (0, 0, -22), (1.0, 1.0, 1.0), label="Hero_Artillery")
    mesh("SM_Erebus_SandbagRow_A", (-120, 780, 0), (0, 0, 80), (1.0, 1.0, 0.72), label="ArtillerySandbags")
    # Burned-out supply truck on the spawn flank.
    mesh("SM_Erebus_TruckWreck_A", (-1520, -620, 0), (0, 0, 24), (1.0, 1.0, 1.0), label="Hero_Truck")

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
    # Warm practical pools vs the cool overcast key (gate feedback round 4:
    # controlled warm/cool contrast, stronger focal guidance). Intensities up
    # ~50% so each fire carves a readable warm pocket at gameplay distance.
    light((-1120, -560, 240), (1.0, 0.42, 0.12), 7800.0, 1000.0, "BarrelFire")
    light((-650, -560, 110), (1.0, 0.42, 0.13), 14000.0, 1600.0, "CraterFire")
    light((1500, 560, 160), (1.0, 0.26, 0.07), 5000.0, 1000.0, "WreckFire")
    light((2160, 500, 350), (1.0, 0.48, 0.12), 1700.0, 560.0, "BunkerLamp")
    light((2620, -700, 220), (1.0, 0.33, 0.09), 5600.0, 760.0, "PipeFire")
    light((2800, -2540, 260), (1.0, 0.34, 0.09), 11000.0, 1800.0, "MonolithFire")
    light((2120, -520, 200), (1.0, 0.30, 0.08), 8000.0, 1050.0, "GunshipFire")
    light((380, -730, 180), (1.0, 0.40, 0.12), 2300.0, 470.0, "TankSmolder")
    light((1500, -3300, 400), (1.0, 0.34, 0.09), 16000.0, 3400.0, "VistaFire")


def build_background():
    """A complete ruined city in every direction (gate feedback round 3: 'the world
    must be complete'). Deterministic pseudo-random rings of ruin masses fill 360
    degrees around the route in three depth bands, with the forward lane kept open
    for the corridor read and the Cathedral. All far masses are cheap silhouettes:
    no shadows, fog does the shading."""

    def ring(band_index, radius_min, radius_max, step_deg, scale_min, scale_max, jitter_deg):
        index = 0
        angle = 0.0
        while angle < 360.0:
            seed = int(angle * 7.3) + band_index * 977
            yaw_jitter = ((seed * 13) % (2 * jitter_deg)) - jitter_deg
            bearing = angle + yaw_jitter * 0.4
            # Keep the forward corridor lane open: the vanishing point, the gate and
            # the Cathedral own bearings within ~9 degrees of +X.
            if abs(((bearing + 180.0) % 360.0) - 180.0) < 9.0:
                angle += step_deg
                continue
            radius = radius_min + ((seed * 37) % 100) / 100.0 * (radius_max - radius_min)
            import math
            cx = 1200 + radius * math.cos(math.radians(bearing))
            cy = radius * math.sin(math.radians(bearing))
            scale = scale_min + ((seed * 61) % 100) / 100.0 * (scale_max - scale_min)
            variant = "A" if (seed % 3) else "B"
            scale_z = scale * (0.8 + ((seed * 17) % 40) / 100.0)
            mesh("SM_Erebus_RuinBlock_" + variant, (cx, cy, 0), (0, 0, (seed * 29) % 360),
                 (scale, scale, scale_z),
                 label="City_%d_%d" % (band_index, index))
            if seed % 4 == 0:
                # Seat the coping on the block's actual (randomized) top, minus the
                # collapse cuts, so it never floats as a dark beam against the sky.
                block_h = (1900.0 if variant == "A" else 2700.0) * scale_z * 0.82
                mesh("SM_Erebus_RuinEdge_" + ("A" if seed % 2 else "B"),
                     (cx, cy, block_h), (0, 0, (seed * 29) % 360),
                     (1.1 * scale, 1.1 * scale, 1.4),
                     label="CityEdge_%d_%d" % (band_index, index))
            index += 1
            angle += step_deg

    # Near band: readable damaged blocks around the whole field, including behind spawn.
    ring(0, 6500, 9000, 15.0, 1.2, 2.0, 10)
    # Mid band: larger masses dissolving into haze.
    ring(1, 9500, 13500, 12.0, 1.8, 3.0, 12)
    # Far band: continuous city mass closing every horizon.
    ring(2, 14000, 22000, 9.0, 2.8, 4.4, 14)

    # Broken facade fragments mixed into the near band flanks for silhouette variety.
    for index, (bx, by, yaw, s) in enumerate([
            (6700, -3600, 168, 1.6), (7400, 3900, 12, 1.7), (-3600, -3400, 105, 1.5),
            (-4100, 2800, 75, 1.6), (3600, -4600, 150, 1.7), (3200, 4400, -18, 1.6),
            (-5200, -800, 95, 1.7), (-5000, 1400, 82, 1.5)]):
        mesh("SM_Erebus_Facade_Broken_A" if index % 2 else "SM_Erebus_StructureFrame_B",
             (bx, by, 0), (0, 0, yaw), (s, s, s), label="CityRuin_%d" % index)

    # Forward skyline: mid-rise ruin layers flanking the open corridor lane, so
    # the vista reads city-into-haze instead of an empty gap (bearings 10-20deg).
    for index, (bx, by, s, yaw) in enumerate([
            (9500, -2900, 1.7, 174), (12500, -3800, 2.2, 168), (16000, -3200, 2.6, 178),
            (10500, 3400, 1.8, -8), (14000, 4200, 2.4, -14), (18500, 3600, 2.9, -4),
            (21000, -4400, 3.2, 172), (24000, 5000, 3.4, -10)]):
        mesh("SM_Erebus_RuinBlock_A" if index % 2 else "SM_Erebus_RuinBlock_B",
             (bx, by, 0), (0, 0, yaw), (s, s, s * 0.9), label="FwdSkyline_%d" % index)

    # Cathedral (visual gate section 17): enormous, vertical, structurally unique,
    # largely black, pushed to ~320m down the corridor axis, center-right of the
    # comparison frame, where the fog stack shapes the fluted towers into the
    # reference's haze-veiled landmark. Tops reach ~25deg above the horizon.
    # Round-4 landmark push: bigger angular size + one more spire so the cluster
    # anchors the frame instead of being swallowed by midground massing.
    mesh("SM_Erebus_CathedralTower_A", (26500, 1500, 0), (0, 0, 20), (1.35, 1.35, 1.4), label="Cathedral_Main")
    mesh("SM_Erebus_CathedralTower_B", (30000, 3000, 0), (0, 0, -15), (1.1, 1.1, 1.2), label="Cathedral_Flank")
    mesh("SM_Erebus_CathedralTower_C", (24500, 500, 0), (0, 0, 45), (1.05, 1.05, 1.25), label="Cathedral_Fore")
    mesh("SM_Erebus_CathedralSpire_A", (28500, 4600, 0), (0, 0, 70), (0.9, 0.9, 1.0), label="Cathedral_Spire_A")
    mesh("SM_Erebus_CathedralSpire_B", (25600, 2300, 0), (0, 0, 130), (1.0, 1.0, 1.1), label="Cathedral_Spire_B")
    mesh("SM_Erebus_CathedralSpire_B", (27400, 600, 0), (0, 0, 200), (0.85, 0.85, 1.3), label="Cathedral_Spire_C")
    mesh("SM_Erebus_RuinBlock_B", (27500, 1500, 0), (0, 0, 8), (3.2, 3.2, 1.6), label="CathedralBase_A")
    mesh("SM_Erebus_RuinBlock_B", (29500, 3200, 0), (0, 0, -14), (3.0, 3.0, 1.4), label="CathedralBase_B")
    mesh("SM_Erebus_RuinBlock_B", (28500, 2300, 0), (0, 0, 24), (2.0, 2.0, 3.2), label="CathedralShoulder_A")


def build_vfx_and_decals():
    # Niagara lives in the runtime director (BuildErebusZoneEffects): components saved
    # into the streamed level do not render in packaged builds. Lights/decals stay here.
    light((6100, -1900, 300), (1.0, 0.36, 0.10), 9000.0, 3000.0, "DistantFire_S")
    light((8400, 2300, 300), (1.0, 0.32, 0.08), 9000.0, 3200.0, "DistantFire_N")

    decal("MI_Erebus_Decal_Scorch", (-1120, -560, 6), (0, -90, 0), (80, 190, 190), "Scorch_Barrel")
    decal("MI_Erebus_Decal_Scorch", (-350, -620, 6), (0, -90, 70), (100, 300, 300), "Scorch_Crater")
    decal("MI_Erebus_Decal_Scorch", (950, -800, 6), (0, -90, 30), (110, 320, 320), "Scorch_MonolithFire")
    decal("MI_Erebus_Decal_Scorch", (1500, 560, 6), (0, -90, -20), (90, 280, 280), "Scorch_Wreck")
    decal("MI_Erebus_Decal_Scorch", (2620, -700, 6), (0, -90, 45), (80, 240, 240), "Scorch_Pipe")
    decal("MI_Erebus_Decal_Scorch", (2120, -520, 6), (0, -90, -40), (120, 420, 420), "Scorch_Gunship")
    decal("MI_Erebus_Decal_Scorch", (380, -730, 6), (0, -90, 25), (90, 300, 300), "Scorch_Tank")
    decal("MI_Erebus_Decal_Scorch", (-1520, -620, 6), (0, -90, 24), (80, 260, 260), "Scorch_Truck")
    decal("MI_Erebus_Decal_Grime", (-2100, 0, 180), (0, 0, 0), (70, 300, 220), "Grime_RearWall")
    decal("MI_Erebus_Decal_Grime", (952, -822, 400), (0, 0, 6), (70, 380, 420), "Grime_Monolith_A")
    decal("MI_Erebus_Decal_Grime", (2148, 822, 420), (0, 0, 186), (70, 380, 420), "Grime_Monolith_B")
    decal("MI_Erebus_Decal_Scorch", (760, -600, 120), (0, 0, 8), (60, 240, 200), "Scorch_BlastWall")
    decal("MI_Erebus_Decal_Grime", (-150, -1320, 300), (0, 0, 176), (80, 520, 620), "Grime_FacadeBreak_S")
    decal("MI_Erebus_Decal_Grime", (550, 1330, 340), (0, 0, 2), (80, 520, 620), "Grime_Facade_N")
    decal("MI_Erebus_Decal_Scorch", (-450, 1220, 220), (0, 0, -4), (80, 420, 380), "Scorch_ServiceBay_N")

    # Round-4 surface-variety pass: soot streaks under openings, oil stains on
    # the road, burn shadows around every practical fire, wall grime on the
    # trench line — believable wear instead of uniform procedural concrete.
    for index, (gx, gy, gz, yaw, w, h) in enumerate([
            (-300, -1335, 700, 176, 260, 520), (450, -1335, 900, 176, 300, 640),
            (1200, -1335, 650, 176, 240, 480), (-150, 1315, 620, 2, 280, 560),
            (900, 1305, 780, 2, 300, 600), (2200, 1500, 700, -4, 260, 520)]):
        decal("MI_Erebus_Decal_Grime", (gx, gy, gz), (0, 0, yaw), (70, w, h),
              "SootStreak_%d" % index)
    for index, (ox, oy, s, yaw) in enumerate([
            (-300, -80, 2.2, 40), (800, 120, 1.8, 150), (1600, -120, 2.4, 260),
            (2500, 60, 1.9, 330), (-1000, 40, 1.6, 200)]):
        decal("MI_Erebus_Decal_Grime", (ox, oy, 6), (0, -90, yaw), (30, 200 * s, 170 * s),
              "OilStain_%d" % index)
    decal("MI_Erebus_Decal_Scorch", (3430, -720, 700), (0, 0, 0), (90, 300, 620), "Scorch_GateCol_S")
    decal("MI_Erebus_Decal_Grime", (3430, 720, 800), (0, 0, 0), (90, 280, 700), "Grime_GateCol_N")
    decal("MI_Erebus_Decal_Scorch", (-620, -1010, 220), (0, 0, 90), (70, 480, 340), "Scorch_TrenchS")
    decal("MI_Erebus_Decal_Grime", (1000, 945, 200), (0, 0, 270), (70, 520, 300), "Grime_TrenchN")
    decal("MI_Erebus_Decal_Scorch", (3300, 620, 6), (0, -90, 40), (70, 200, 200), "Scorch_GateBarrel")


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
