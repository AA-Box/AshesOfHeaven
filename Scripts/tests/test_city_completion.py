#!/usr/bin/env python3
"""Geometry checks for Scripts/CompleteSoulCity.py.

    python3 Scripts/tests/test_city_completion.py

The script only runs inside the editor, so `unreal` is stubbed and the source is executed up
to - but not including - its `main()` call. What is under test is the pure geometry that
decides whether the district floats: the support test, the footprint refusal, the world box of
a turned building, and what counts as ground once this pass has laid its own roads.
"""

import os
import sys
import unittest
from types import SimpleNamespace
from unittest.mock import MagicMock

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCRIPT = os.path.join(ROOT, "CompleteSoulCity.py")
unreal = MagicMock()
sys.modules["unreal"] = unreal

SOURCE = open(SCRIPT, encoding="utf-8").read()
TAIL = "\ntry:\n    main()"
assert TAIL in SOURCE, "the module's entry point moved; this loader has to be updated"
CITY = {"__name__": "city_completion"}
exec(compile(SOURCE[:SOURCE.index(TAIL)], SCRIPT, "exec"), CITY)


def vec(x=0.0, y=0.0, z=0.0):
    return SimpleNamespace(x=x, y=y, z=z)


def hit(z):
    return SimpleNamespace(to_dict=lambda: {"blocking_hit": True, "location": vec(z=z)})


def actor_with(tags):
    return SimpleNamespace(get_editor_property=lambda _name: tags)


class Seating(unittest.TestCase):
    """A support sample that hits nothing is the worst case, not an exemption."""

    def setUp(self):
        CITY["CULLED"][0] = 0
        CITY["EAS"].reset_mock()
        self.terrain = SimpleNamespace(world=None)
        self.actor = SimpleNamespace(
            get_actor_bounds=lambda _simple: (vec(0.0, 0.0, 500.0), vec(400.0, 400.0, 500.0)))

    def test_void_under_a_corner_is_destroyed(self):
        unreal.SystemLibrary.line_trace_single.return_value = None
        self.assertIsNone(CITY["ensure_seated"](self.terrain, self.actor))
        self.assertEqual(CITY["CULLED"][0], 1)
        CITY["EAS"].destroy_actor.assert_called_once_with(self.actor)

    def test_ground_just_under_it_survives(self):
        unreal.SystemLibrary.line_trace_single.return_value = hit(-20.0)
        self.assertIs(CITY["ensure_seated"](self.terrain, self.actor), self.actor)
        self.assertEqual(CITY["CULLED"][0], 0)

    def test_air_under_it_is_destroyed(self):
        unreal.SystemLibrary.line_trace_single.return_value = hit(-5000.0)
        self.assertIsNone(CITY["ensure_seated"](self.terrain, self.actor))
        self.assertEqual(CITY["CULLED"][0], 1)


class Footprint(unittest.TestCase):
    """Steep samples have a budget; samples over nothing do not."""

    def ground(self, missing=(), steep=(), exact_missing=(), exact_z=None):
        terrain = CITY["Terrain"](None)
        half = 2000.0

        def key_of(x, y):
            return (round((x - 5000.0) / half), round((y - 5000.0) / half))

        def at(x, y):
            key = key_of(x, y)
            if key in missing:
                return None
            return (0.0, 0.5 if key in steep else 1.0)

        def exact(x, y):
            key = key_of(x, y)
            if key in missing or key in exact_missing:
                return None
            return (exact_z or {}).get(key, 0.0)

        terrain.at = at
        terrain.exact = exact
        return terrain, half

    def test_flat_ground_is_accepted(self):
        terrain, half = self.ground()
        self.assertIsNotNone(terrain.footprint(5000.0, 5000.0, half, half, 0.0))

    def test_three_steep_samples_still_build(self):
        terrain, half = self.ground(steep={(-1, -1), (-1, 0), (-1, 1)})
        self.assertIsNotNone(terrain.footprint(5000.0, 5000.0, half, half, 0.0))

    def test_one_edge_over_void_is_refused(self):
        terrain, half = self.ground(missing={(-1, -1), (-1, 0), (-1, 1)})
        why = {}
        self.assertIsNone(terrain.footprint(5000.0, 5000.0, half, half, 0.0, why))
        self.assertEqual(why, {"void": 1})

    def test_a_single_corner_over_void_is_refused(self):
        terrain, half = self.ground(missing={(1, 1)})
        self.assertIsNone(terrain.footprint(5000.0, 5000.0, half, half, 0.0))

    def test_a_corner_the_lattice_rounded_onto_ground_is_refused(self):
        # at() snaps to a 5 m cell, so it reports ground for a corner that is really over the
        # drop. The uncached corner pass is the one that has to catch it.
        terrain, half = self.ground(exact_missing={(1, 1)})
        why = {}
        self.assertIsNone(terrain.footprint(5000.0, 5000.0, half, half, 0.0, why))
        self.assertEqual(why, {"void": 1})

    def test_a_corner_below_the_lattice_deepens_the_podium(self):
        # The true corner is 4 m under what the lattice reported; low has to follow it down or
        # the plinth built to the lattice value hangs over that corner.
        terrain, half = self.ground(exact_z={(1, 1): -400.0})
        low, base, relief = terrain.footprint(5000.0, 5000.0, half, half, 0.0)
        self.assertEqual(low, -400.0)
        self.assertEqual(base, 0.0)
        self.assertEqual(relief, 400.0)


class TurnedBox(unittest.TestCase):
    """The overlap test is axis-aligned, so a turned building needs its world box."""

    def test_axis_aligned_is_unchanged(self):
        self.assertEqual(CITY["world_half"](9200.0, 1400.0, 0.0), (9200.0, 1400.0))

    def test_quarter_turn_swaps(self):
        box = CITY["world_half"](9200.0, 1400.0, 90.0)
        self.assertAlmostEqual(box[0], 1400.0, places=3)
        self.assertAlmostEqual(box[1], 9200.0, places=3)

    def test_diagonal_grows_both_axes(self):
        box = CITY["world_half"](9200.0, 1400.0, 45.0)
        self.assertAlmostEqual(box[0], 7495.0, delta=5.0)
        self.assertAlmostEqual(box[1], 7495.0, delta=5.0)

    def test_the_overlap_the_local_box_missed(self):
        # A 92 x 14 m row house at 45 deg, and a neighbour 40 m away on the diagonal.
        spots = [(4000.0, 4000.0, 700.0, 700.0)]
        self.assertTrue(CITY["clear_of"](0.0, 0.0, 0.0, spots, 9200.0, 1400.0))
        box = CITY["world_half"](9200.0, 1400.0, 45.0)
        self.assertFalse(CITY["clear_of"](0.0, 0.0, 0.0, spots, box[0], box[1]))


class Ground(unittest.TestCase):
    """Our own roads are not ground: the fences run after they are laid."""

    def trace(self, actors):
        unreal.SystemLibrary.line_trace_multi.return_value = [
            SimpleNamespace(to_dict=lambda a=a: {"hit_actor": a}) for a in actors]

    def test_authored_geometry_is_ground(self):
        self.trace([actor_with(["SomeAuthoredThing"])])
        self.assertTrue(CITY["solid_ground"](None, 0.0, 0.0))

    def test_our_own_road_over_the_void_is_not_ground(self):
        self.trace([actor_with([CITY["TAG"]])])
        self.assertFalse(CITY["solid_ground"](None, 0.0, 0.0))

    def test_nothing_at_all_is_not_ground(self):
        self.trace([])
        self.assertFalse(CITY["solid_ground"](None, 0.0, 0.0))


if __name__ == "__main__":
    unittest.main(verbosity=2)
