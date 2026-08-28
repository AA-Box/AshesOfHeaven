import os
import sys

import unreal

"""Author L_Cathedral_Presentation: the Cathedral as a saved level of real kit meshes.

Replaces the procedural architectural scaffold that
AAHChapterOneDirector::BuildCathedralArtTarget builds out of scaled SM_AH_Cube fins and
frames. The primitive builder stays as the packaged-safety fallback and is skipped whenever
this level loads.

Coordinates are ANCHOR-LOCAL: local (0,0,0) is the CathedralApproach stage anchor, world
(14500, 0, 790) - the raised Cathedral walkway elevation, NOT the Erebus ground plane. The
zone spans the approach, the interior, the failsafe terminal chamber and the escape route
(world X 14000..20000 -> local -500..5500).

The Cathedral is Veil architecture: too large, too regular, and indifferent to the human
walkway threaded through it. Human kit pieces appear only as the expedition's own scaffolding,
at a scale that reads as temporary against the structure.
"""

sys.path.insert(0, os.path.join(unreal.Paths.project_dir(), "Scripts"))
from ah_zone_authoring import Zone  # noqa: E402

LEVEL_PATH = "/Game/Ashes/Environment/Cathedral/L_Cathedral_Presentation"
ZONE = Zone(LEVEL_PATH, "Cathedral")

# The gameplay route is a narrow walkway down Y=0. Veil structure starts well outside it so
# nothing authored here can crowd or visually block the path the player actually walks.
ROUTE_HALF_WIDTH = 300.0
VOID_Y = 1050.0


def build_walkway():
    """The human catwalk the player crosses, and the handrails that give it scale."""
    ZONE.row("SM_Erebus_Catwalk_A", (-400, 0, 0), (420, 0, 0), 14, scale=(1.05, 1.2, 1.0), label="Walkway")
    ZONE.row("SM_Erebus_CatwalkSupport_A", (-300, -ROUTE_HALF_WIDTH, -120), (840, 0, 0), 7, label="WalkwaySupport_S")
    ZONE.row("SM_Erebus_CatwalkSupport_A", (-300, ROUTE_HALF_WIDTH, -120), (840, 0, 0), 7, label="WalkwaySupport_N")
    ZONE.row("SM_Erebus_Barricade_A", (-350, -285, 0), (700, 0, 0), 8, scale=(1.0, 0.6, 0.8), label="Handrail_S")
    ZONE.row("SM_Erebus_Barricade_A", (-350, 285, 0), (700, 0, 0), 8, rotation=(0, 0, 180),
             scale=(1.0, 0.6, 0.8), label="Handrail_N")

    # The ramp up from the approach: the walkway climbs into the structure.
    ZONE.row("SM_Erebus_TowerSlab_A", (-2100, 0, -700), (240, 0, 84), 10,
             scale=(0.6, 1.5, 0.25), label="ApproachRamp")


def build_veil_structure():
    """Columns, beams and vaulting. Regular, oversized, and not built for people."""
    # Colonnade: two ranks of heavy columns marching the length of the interior.
    for side, y in (("S", -VOID_Y), ("N", VOID_Y)):
        ZONE.row("SM_Erebus_ColumnHeavy_A", (-300, y, 0), (900, 0, 0), 7,
                 scale=(2.6, 2.6, 6.0), label="VeilColumn_" + side)
        ZONE.row("SM_Erebus_ColumnHeavy_A", (150, y * 1.9, 0), (1800, 0, 0), 4,
                 scale=(3.4, 3.4, 9.0), label="VeilColumnOuter_" + side)

    # Vaulting: beams spanning the nave, high enough to read as a ceiling without closing it.
    ZONE.row("SM_Erebus_BeamHeavy_A", (-200, 0, 1500), (900, 0, 0), 7,
             rotation=(0, 0, 90), scale=(2.0, 2.0, 2.2), label="VaultBeam")
    ZONE.row("SM_Erebus_Overhang_A", (250, 0, 2100), (1800, 0, 0), 4,
             scale=(2.2, 4.0, 1.6), label="Vault")
    ZONE.row("SM_Erebus_StructureFrame_B", (-200, 0, 2600), (1500, 0, 0), 4,
             rotation=(0, 0, 90), scale=(1.8, 3.0, 1.4), label="UpperFrame")

    # Fins: the vocabulary the fallback established, now as kit geometry.
    for index, x in enumerate([-300.0, 600.0, 1500.0, 2700.0, 3600.0, 4500.0]):
        ZONE.mesh("SM_Erebus_Facade_Heavy_A", (x, -VOID_Y - 400, 0), (0, 0, 2), (1.2, 1.2, 4.0),
                  "VeilFin_S_%02d" % index)
        ZONE.mesh("SM_Erebus_Facade_Heavy_B", (x, VOID_Y + 400, 0), (0, 0, 178), (1.2, 1.2, 4.0),
                  "VeilFin_N_%02d" % index)

    # Monoliths standing in the side voids: mass with no function the player can name.
    for index, (x, y, yaw, s) in enumerate([
            (900, -1750, 14, 1.3), (2100, 1800, -22, 1.5), (3400, -1900, 40, 1.2),
            (4700, 1700, 160, 1.6)]):
        ZONE.mesh("SM_Erebus_Monolith_A", (x, y, 0), (0, 0, yaw), (s, s, s * 1.4),
                  "VeilMonolith_%02d" % index)

    # The far end: towers and spires seen down the nave, the structure continuing past the level.
    ZONE.mesh("SM_Erebus_CathedralTower_A", (6400, 0, -400), (0, 0, 12), (1.6, 1.6, 1.8), label="Tower_Axial")
    ZONE.mesh("SM_Erebus_CathedralTower_B", (6900, -2600, -400), (0, 0, -30), (1.4, 1.4, 1.6), label="Tower_S")
    ZONE.mesh("SM_Erebus_CathedralTower_C", (7100, 2400, -400), (0, 0, 55), (1.5, 1.5, 1.5), label="Tower_N")
    ZONE.mesh("SM_Erebus_CathedralSpire_A", (7600, -1200, -400), (0, 0, 70), (1.2, 1.2, 1.4), label="Spire_S")
    ZONE.mesh("SM_Erebus_CathedralSpire_B", (7900, 1400, -400), (0, 0, 130), (1.3, 1.3, 1.5), label="Spire_N")


def build_terminal_chamber():
    """World X 17500-19800: the failsafe terminal room, the decision point of the level."""
    ZONE.row("SM_Erebus_GroundSlab_A", (3200, 0, -10), (420, 0, 0), 5, scale=(1.2, 2.4, 1.0), label="ChamberFloor")
    ZONE.mesh("SM_Erebus_Fortress_A", (3600, -900, 0), (0, 0, 20), (1.1, 1.1, 2.2), label="ChamberMass_S")
    ZONE.mesh("SM_Erebus_Fortress_B", (3600, 900, 0), (0, 0, -20), (1.1, 1.1, 2.2), label="ChamberMass_N")
    ZONE.mesh("SM_Erebus_StructureFrame_A", (3600, 0, 1400), scale=(1.6, 3.4, 1.8), label="ChamberFrame")

    # The expedition's own equipment around the terminal: human, temporary, small.
    ZONE.mesh("SM_Erebus_Scaffold_A", (3400, -280, 0), scale=(0.9, 0.9, 1.1), label="TerminalScaffold")
    ZONE.mesh("SM_Erebus_ServiceBay_A", (3620, -300, 0), (0, 0, 12), (0.85, 0.85, 0.85), label="TerminalBay")
    ZONE.mesh("SM_Erebus_PanelBank_A", (3620, -300, 165), (0, 0, 12), label="TerminalPanel")
    for index, (x, y, yaw) in enumerate([(3300, 340, 0), (3480, 400, 35), (3760, -380, -20)]):
        ZONE.mesh("SM_Erebus_Crate_A", (x, y, 0), (0, 0, yaw), (0.9, 0.9, 0.9), "ExpeditionCase_%02d" % index)
    ZONE.mesh("SM_Erebus_CrateOpen_A", (3900, 330, 0), (0, 0, 150), (0.9, 0.9, 0.9), label="ExpeditionCase_Open")
    ZONE.prop("BP_Human_ExpeditionLight", (3350, -360, 0), scale=(0.85, 0.85, 0.85), label="ExpeditionLight_A")
    ZONE.prop("BP_Human_ExpeditionLight", (3820, 360, 0), scale=(0.85, 0.85, 0.85), label="ExpeditionLight_B")
    # The lit glyph panels stay runtime (AAHChapterOneDirector::BuildCathedralGlyphs): the
    # BP_Cathedral_GlyphPanel blueprint is assembled from SM_AH_* debug primitives, and the
    # glyph needs a dynamic emissive material instance a saved level cannot carry.
    ZONE.mesh("SM_Erebus_TowerSlab_A", (3100, -250, 300), (0, 0, 0), (0.4, 1.2, 1.6), label="GlyphMount_Terminal")


def build_escape_route():
    """World X 20000+: the way out, already coming apart as Erebus is fired on."""
    ZONE.row("SM_Erebus_Catwalk_A", (5600, 0, 0), (420, 0, 0), 6, scale=(1.05, 1.2, 1.0), label="EscapeWalkway")
    ZONE.mesh("SM_Erebus_BrokenFloor_A", (6300, 0, 4), (0, 0, 8), (1.6, 1.6, 1.0), label="EscapeCollapse")
    ZONE.mesh("SM_Erebus_RuinEdge_A", (6800, -520, 0), (0, 0, 26), (1.4, 1.4, 1.6), label="EscapeRuin_S")
    ZONE.mesh("SM_Erebus_RuinEdge_B", (7000, 540, 0), (0, 0, -34), (1.3, 1.3, 1.5), label="EscapeRuin_N")
    for index, (x, y, yaw) in enumerate([(5900, -320, 20), (6500, 300, 140), (7100, -280, 260)]):
        ZONE.mesh("SM_Erebus_RubbleLarge_A", (x, y, 0), (0, 0, yaw), (0.9, 0.9, 0.9),
                  "EscapeRubble_%02d" % index)
    ZONE.prop("BP_Cathedral_Fin", (5800, -700, 90), (0, 0, -3), (2.8, 2.8, 2.8), label="EscapeFin_S")
    ZONE.prop("BP_Cathedral_Fin", (6600, 700, 210), (0, 180, 4), (2.4, 2.4, 2.4), label="EscapeFin_N")


def build_lighting_and_wear():
    """Cold Veil light from above, warm human light on the walkway, wear where people have been."""
    for index, x in enumerate([-100.0, 900.0, 1900.0, 2900.0, 3900.0, 4900.0]):
        ZONE.light((x, 0, 900), (0.42, 0.56, 1.0), 700.0, 1400.0, "VeilLight_%02d" % index)
    for index, x in enumerate([300.0, 1500.0, 2700.0, 3600.0, 5200.0]):
        ZONE.light((x, -280, 210), (0.98, 0.72, 0.42), 260.0, 520.0, "ExpeditionLamp_%02d" % index)
    ZONE.light((6400, 0, 600), (0.72, 0.82, 1.0), 900.0, 2200.0, "AxialGlow")

    for index, (x, y, yaw) in enumerate([
            (1500, -255, 0), (3100, -255, 0), (3600, -250, 0), (4500, 255, 180)]):
        ZONE.decal("MI_Erebus_Decal_Grime", (x, y, 400), (0, 0, yaw), (60, 300, 620),
                   "VeilStreak_%02d" % index)
    for index, (x, y, s) in enumerate([(300, 0, 1.6), (1500, -60, 1.4), (2900, 80, 1.8), (3600, 0, 2.0)]):
        ZONE.decal("MI_Erebus_Decal_Grime", (x, y, 6), (0, -90, 40 * index), (30, 180 * s, 150 * s),
                   "WalkwayWear_%02d" % index)
    for index, (x, y, s) in enumerate([(6300, 0, 2.4), (6800, -520, 1.8), (7000, 540, 1.6)]):
        ZONE.decal("MI_Erebus_Decal_Scorch", (x, y, 6), (0, -90, 60 * index), (70, 220 * s, 200 * s),
                   "EscapeScorch_%02d" % index)


def run():
    ZONE.open_clean_level()
    build_walkway()
    build_veil_structure()
    build_terminal_chamber()
    build_escape_route()
    build_lighting_and_wear()
    ZONE.save()


run()
