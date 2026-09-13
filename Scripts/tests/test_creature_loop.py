"""Loop endpoint regression without launching Unreal."""
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import MagicMock, patch


class Vector:
    def __init__(self, x, y, z):
        self.x, self.y, self.z = x, y, z

    def __sub__(self, other):
        return Vector(self.x - other.x, self.y - other.y, self.z - other.z)


class Quat:
    def __init__(self, x, y, z, w):
        self.x, self.y, self.z, self.w = x, y, z, w


class CreatureLoopTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        unreal = MagicMock()
        unreal.Vector, unreal.Quat = Vector, Quat
        unreal.Paths.project_saved_dir.return_value = "/tmp"
        unreal.Paths.convert_relative_path_to_full.return_value = "/tmp"
        spec = importlib.util.spec_from_file_location(
            "teuthisan_loop_test", Path(__file__).parents[1] / "AuthorTeuthisanAnimations.py")
        cls.module = importlib.util.module_from_spec(spec)
        with patch.dict(sys.modules, {"unreal": unreal}):
            spec.loader.exec_module(cls.module)

    def test_tail_reaches_first_pose_exactly(self):
        poses = [{"root": (Vector(i, i * 2, i * 3), Quat(0, 0, 0.6, 0.8), Vector(1, 1, 1))}
                 for i in range(20)]
        poses[0]["root"] = (Vector(0, 0, 0), Quat(0, 0, 0, 1), Vector(1, 1, 1))
        self.module.close_loop(["root"], poses, 8)
        t, q, _ = poses[-1]["root"]
        self.assertEqual((t.x, t.y, t.z), (0, 0, 0))
        self.assertAlmostEqual(q.w, 1.0)
        self.assertAlmostEqual(q.z, 0.0)
        self.assertEqual(poses[5]["root"][0].x, 5)

    def test_blend_span_longer_than_clip(self):
        poses = [{"root": (Vector(i, 0, 0), Quat(0, 0, 0, 1), Vector(1, 1, 1))}
                 for i in range(4)]
        self.module.close_loop(["root"], poses, 24)
        self.assertEqual(poses[-1]["root"][0].x, 0)

    def test_two_frame_clip_closes(self):
        poses = [{"root": (Vector(i, 0, 0), Quat(0, 0, 0, 1), Vector(1, 1, 1))}
                 for i in range(2)]
        self.module.close_loop(["root"], poses, 24)
        self.assertEqual(poses[-1]["root"][0].x, 0)


if __name__ == "__main__":
    unittest.main()
