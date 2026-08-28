import os
import sys

import unreal

"""Author L_Transit_Presentation: the transit station as a saved level of real kit meshes.

Replaces the runtime primitive station (AAHChapterOneDirector::BuildTransitStationArtTarget,
which builds the platform out of scaled SM_AH_Cube/SM_AH_Cylinder) with the manufactured
SM_Erebus_* modular kit. The primitive builder stays as the packaged-safety fallback and is
skipped whenever this level loads.

Coordinates are ANCHOR-LOCAL: local (0,0,0) is the TransitStation stage anchor, world
(3500, 0, -50), which is the gameplay collision floor top. The zone covers the transit
platform and the Veil revelation chamber beyond it (world X 2500..5600 -> local -1000..2100).
"""

sys.path.insert(0, os.path.join(unreal.Paths.project_dir(), "Scripts"))
from ah_zone_authoring import Zone  # noqa: E402

LEVEL_PATH = "/Game/Ashes/Environment/Transit/L_Transit_Presentation"
ZONE = Zone(LEVEL_PATH, "Transit")

# Half-width of the platform hall. The gameplay route runs down the middle at Y=0; the
# station walls stand outside the play envelope so nothing narrows the corridor visually.
HALL_Y = 780.0


def build_platform():
    """The floor plane the player reads as a station platform, plus its edges and drainage."""
    # Approach apron, world X 1800-2550. The Erebus zone's road spine stops around X=2000 and
    # the station hall starts at 2550, so without this the seam between the two authored zones
    # has collision floor under it and nothing visible on top - the player walks on air.
    # AshesOfHeaven.Chapter.Objective01PresentationAligned samples (2000, 0) for exactly this.
    for lane_y in (-430, 0, 430):
        ZONE.row("SM_Erebus_GroundSlab_A", (-1700, lane_y, 0), (380, 0, 0), 3,
                 scale=(1.05, 1.0, 1.0), label="Approach_%d" % lane_y)

    # Slabs run the length of the hall in two rows either side of the track bed.
    ZONE.row("SM_Erebus_GroundSlab_A", (-950, -430, 0), (420, 0, 0), 8, scale=(1.05, 1.0, 1.0), label="Platform_S")
    ZONE.row("SM_Erebus_GroundSlab_A", (-950, 430, 0), (420, 0, 0), 8, scale=(1.05, 1.0, 1.0), label="Platform_N")
    ZONE.row("SM_Erebus_Curb_A", (-950, -300, 10), (420, 0, 0), 8, label="PlatformEdge_S")
    ZONE.row("SM_Erebus_Curb_A", (-950, 300, 10), (420, 0, 0), 8, rotation=(0, 0, 180), label="PlatformEdge_N")

    # Track bed: recessed channel between the platforms, wet at the bottom.
    ZONE.row("SM_Erebus_DrainChannel_A", (-900, 0, -18), (380, 0, 0), 8, label="TrackBed")
    ZONE.row("SM_Erebus_Puddle_A", (-700, 0, -14), (620, 0, 0), 5, scale=(1.4, 1.2, 1.0), label="TrackPuddle")
    ZONE.mesh("SM_Erebus_PuddleSet_A", (-330, -420, 2), scale=(1.2, 1.2, 1.0), label="PlatformPuddle_S")
    ZONE.mesh("SM_Erebus_PuddleSet_A", (280, 440, 2), (0, 0, 130), (1.0, 1.0, 1.0), label="PlatformPuddle_N")

    # Broken floor where the ceiling has already come down: the station is not intact.
    ZONE.mesh("SM_Erebus_BrokenFloor_A", (620, -480, 4), (0, 0, 12), (1.3, 1.3, 1.0), label="FloorCollapse_S")
    ZONE.mesh("SM_Erebus_RubbleMedium_A", (660, -520, 20), (0, 0, 40), label="FloorRubble_A")
    ZONE.mesh("SM_Erebus_RubbleLarge_A", (1180, 520, 30), (0, 0, 200), (0.8, 0.8, 0.9), label="FloorRubble_B")
    ZONE.mesh("SM_Erebus_DebrisField_A", (150, -560, 6), (0, 0, 70), label="Debris_S")
    ZONE.mesh("SM_Erebus_DebrisField_B", (900, 480, 6), (0, 0, -50), label="Debris_N")


def build_hall():
    """Walls, columns and the roof structure that make the space read as enclosed."""
    for side, y, yaw in (("S", -HALL_Y, 0), ("N", HALL_Y, 180)):
        ZONE.row("SM_Erebus_IndustrialWall_A", (-950, y, 0), (430, 0, 0), 8,
                 rotation=(0, 0, yaw), scale=(1.0, 1.0, 1.15), label="HallWall_" + side)
        ZONE.row("SM_Erebus_IndustrialColumn_A", (-820, y * 0.82, 0), (560, 0, 0), 6,
                 scale=(1.0, 1.0, 1.25), label="HallColumn_" + side)
        ZONE.row("SM_Erebus_PanelBank_A", (-640, y * 0.94, 210), (900, 0, 0), 4,
                 rotation=(0, 0, yaw), label="WallPanel_" + side)
        ZONE.row("SM_Erebus_VentBank_A", (-300, y * 0.94, 430), (1150, 0, 0), 3,
                 rotation=(0, 0, yaw), label="Vent_" + side)

    # Roof: beams across the hall, with service pipes and cable runs slung beneath them.
    ZONE.row("SM_Erebus_BeamHeavy_A", (-880, 0, 620), (480, 0, 0), 7,
             rotation=(0, 0, 90), scale=(1.0, 1.0, 0.9), label="RoofBeam")
    ZONE.row("SM_Erebus_Pipe_Large_A", (-950, -250, 560), (900, 0, 0), 4,
             rotation=(0, 0, 0), label="RoofPipe_S")
    ZONE.row("SM_Erebus_Pipe_Large_A", (-950, 250, 585), (900, 0, 0), 4, label="RoofPipe_N")
    ZONE.mesh("SM_Erebus_Pipe_Elbow_A", (760, -250, 560), (0, 0, 90), label="RoofPipe_Elbow")
    ZONE.row("SM_Erebus_CableSpan_A", (-800, 0, 520), (760, 0, 0), 5, label="RoofCable")
    ZONE.row("SM_Erebus_PipeSupport_A", (-820, -250, 470), (860, 0, 0), 4, label="PipeSupport_S")

    # Service catwalk above the north platform: readable height and a place light comes from.
    ZONE.row("SM_Erebus_Catwalk_A", (-500, 620, 380), (400, 0, 0), 5, label="Catwalk")
    ZONE.row("SM_Erebus_CatwalkSupport_A", (-460, 620, 180), (800, 0, 0), 3, label="CatwalkSupport")


def build_entrance():
    """The station mouth on the route, where the objective sends the player."""
    ZONE.mesh("SM_Erebus_CheckpointGate_A", (0, 0, 0), scale=(1.25, 1.35, 1.3), label="StationGate")
    ZONE.mesh("SM_Erebus_IndustrialDoor_A", (-40, -300, 0), scale=(1.0, 1.0, 1.15), label="StationDoor_S")
    ZONE.mesh("SM_Erebus_IndustrialDoor_A", (-40, 300, 0), (0, 0, 180), (1.0, 1.0, 1.15), label="StationDoor_N")
    ZONE.mesh("SM_Erebus_StructureFrame_A", (0, 0, 500), scale=(1.4, 1.5, 1.0), label="GateFrame")
    ZONE.mesh("SM_Erebus_Overhang_A", (-120, 0, 700), scale=(1.2, 1.6, 1.0), label="GateCanopy")

    # Sign frames and their panels carry the authored signage the art manifest calls out.
    # The frame, post and panel are kit meshes rather than BP_Transit_Sign, which is built
    # from SM_AH_* debug primitives; the text itself stays a runtime label
    # (AAHChapterOneDirector::BuildTransitSignage) because TextRender does not survive a
    # headless level save reliably.
    ZONE.mesh("SM_Erebus_SignFrame_A", (-5, -645, 750), label="Sign_StationName")
    ZONE.mesh("SM_Erebus_SignFrame_A", (-5, 645, 720), (0, 0, 180), (0.8, 0.8, 0.8), label="Sign_Evacuation")
    ZONE.mesh("SM_Erebus_PanelBank_A", (-5, -645, 700), scale=(1.1, 1.6, 1.1), label="Sign_Panel_Station")
    ZONE.mesh("SM_Erebus_PanelBank_A", (-5, 645, 690), (0, 0, 180), (0.9, 1.3, 0.9), label="Sign_Panel_Evac")
    ZONE.mesh("SM_Erebus_UtilityPole_A", (0, -760, 0), scale=(1.0, 1.0, 1.2), label="Sign_Post")


def build_evacuation_traces():
    """What people left behind. A composed handful, not a prop carpet."""
    # Benches from kit geometry: a curb slab on short supports. BP_Transit_Bench is built
    # from SM_AH_Cube/SM_AH_Cylinder and would put debug primitives back on the platform.
    for label, x, y, yaw in (("Bench_S", -250, -380, 90), ("Bench_N", 280, 380, -90)):
        ZONE.mesh("SM_Erebus_Curb_A", (x, y, 95), (0, 0, yaw), (1.1, 1.0, 0.7), label)
        ZONE.mesh("SM_Erebus_Crate_A", (x - 70, y, 0), (0, 0, yaw), (0.45, 0.45, 0.85), label + "_LegA")
        ZONE.mesh("SM_Erebus_Crate_A", (x + 70, y, 0), (0, 0, yaw), (0.45, 0.45, 0.85), label + "_LegB")
    for index, (x, y, yaw, scale) in enumerate([
            (-350, -410, 18, 1.0), (-230, -450, -12, 0.85), (-180, -390, 55, 0.9),
            (420, 470, 130, 0.95), (760, 430, -35, 0.8)]):
        ZONE.mesh("SM_Erebus_Crate_A", (x, y, 0), (0, 0, yaw), (scale, scale, scale),
                  "Case_%02d" % index)
    ZONE.mesh("SM_Erebus_CrateOpen_A", (-120, -460, 0), (0, 0, -25), label="Case_Ransacked")
    ZONE.mesh("SM_Erebus_CrateOpen_A", (560, 500, 0), (0, 0, 145), (0.9, 0.9, 0.9), label="Case_Spilled")
    for index, (x, y, yaw) in enumerate([(-60, -520, 0), (330, 520, 40), (980, -470, 210)]):
        ZONE.mesh("SM_Erebus_Barrel_A", (x, y, 0), (0, 0, yaw), label="Barrel_%02d" % index)

    # Control desk the fallback built out of two boxes.
    ZONE.mesh("SM_Erebus_ServiceBay_A", (-60, -250, 0), scale=(0.9, 0.9, 0.9), label="ControlDesk")
    ZONE.mesh("SM_Erebus_PanelBank_A", (-60, -250, 160), label="ControlPanel")
    ZONE.mesh("SM_Erebus_ServiceBay_Destroyed_A", (1250, 300, 0), (0, 0, 165), label="ControlDesk_Wrecked")

    # Barricades: the platform was a defence line before it was abandoned.
    ZONE.mesh("SM_Erebus_Barricade_A", (170, -560, 0), (0, 0, 8), label="Barricade_S")
    ZONE.mesh("SM_Erebus_SandbagRow_A", (250, 560, 0), (0, 0, 184), label="Sandbags_N")


def build_revelation_chamber():
    """World X 4200-5600: the deeper hall where the Veil revelation plays out."""
    ZONE.row("SM_Erebus_GroundSlab_A", (1150, -430, 0), (420, 0, 0), 4, label="DeepFloor_S")
    ZONE.row("SM_Erebus_GroundSlab_A", (1150, 430, 0), (420, 0, 0), 4, label="DeepFloor_N")
    for side, y, yaw in (("S", -HALL_Y, 0), ("N", HALL_Y, 180)):
        ZONE.row("SM_Erebus_IndustrialWall_A", (1200, y, 0), (430, 0, 0), 4,
                 rotation=(0, 0, yaw), scale=(1.0, 1.0, 1.3), label="DeepWall_" + side)
    ZONE.row("SM_Erebus_ColumnHeavy_A", (1300, -560, 0), (700, 0, 0), 3, scale=(0.9, 0.9, 1.4), label="DeepColumn_S")
    ZONE.row("SM_Erebus_ColumnHeavy_A", (1300, 560, 0), (700, 0, 0), 3, scale=(0.9, 0.9, 1.4), label="DeepColumn_N")
    ZONE.mesh("SM_Erebus_Facade_Broken_A", (2050, 0, 0), (0, 0, 90), (1.5, 1.5, 1.4), label="DeepEndWall")
    ZONE.mesh("SM_Erebus_Monolith_A", (1700, -640, 0), (0, 0, 22), (0.7, 0.7, 0.8), label="VeilForm_S")
    ZONE.mesh("SM_Erebus_Monolith_A", (1900, 660, 0), (0, 0, -18), (0.6, 0.6, 0.7), label="VeilForm_N")


def build_lighting_and_wear():
    """Practical lights, then the surface wear that keeps the concrete from reading uniform."""
    for index, (x, y, z) in enumerate([(-820, -560, 430), (-200, 560, 430), (420, -560, 430),
                                       (1040, 560, 430), (1660, -560, 430)]):
        ZONE.mesh("SM_Erebus_WorkLight_A", (x, y, z), label="WorkLight_%02d" % index)

    # Amber platform practicals, one failing red emergency lamp, cold spill from the deep hall.
    ZONE.light((-200, -500, 590), (1.0, 0.48, 0.16), 620.0, 700.0, "PlatformLamp_S")
    ZONE.light((320, 500, 510), (0.95, 0.08, 0.03), 360.0, 520.0, "EmergencyLamp_N")
    ZONE.light((-820, -540, 470), (1.0, 0.52, 0.20), 420.0, 620.0, "PlatformLamp_W")
    ZONE.light((1000, 520, 470), (1.0, 0.44, 0.14), 380.0, 600.0, "PlatformLamp_E")
    ZONE.light((1800, 0, 420), (0.36, 0.50, 1.0), 520.0, 900.0, "VeilSpill")
    ZONE.light((0, 0, 760), (0.90, 0.62, 0.30), 300.0, 900.0, "GateLamp")

    for index, (x, y, yaw, w, h) in enumerate([
            (-700, -770, 0, 300, 520), (-100, 770, 180, 280, 480),
            (500, -770, 0, 320, 560), (1100, 770, 180, 260, 460)]):
        ZONE.decal("MI_Erebus_Decal_Grime", (x, y, 300), (0, 0, yaw), (70, w, h), "WallGrime_%02d" % index)
    for index, (x, y, yaw, s) in enumerate([
            (620, -480, 40, 2.4), (1180, 520, 200, 2.0), (170, -560, 12, 1.6),
            (-60, -520, 90, 1.4), (980, -470, 260, 1.8)]):
        ZONE.decal("MI_Erebus_Decal_Scorch", (x, y, 6), (0, -90, yaw), (70, 200 * s, 180 * s),
                   "Scorch_%02d" % index)
    for index, (x, y, s) in enumerate([(-400, -60, 2.0), (300, 80, 1.7), (900, -100, 2.2)]):
        ZONE.decal("MI_Erebus_Decal_Grime", (x, y, 6), (0, -90, 30 * index), (30, 190 * s, 160 * s),
                   "OilStain_%02d" % index)


def run():
    ZONE.open_clean_level()
    build_platform()
    build_hall()
    build_entrance()
    build_evacuation_traces()
    build_revelation_chamber()
    build_lighting_and_wear()
    ZONE.save()


run()
