#!/usr/bin/env python3

import importlib.util
import io
import tempfile
import unittest
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from types import SimpleNamespace


ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location("validate_pso", ROOT / "Scripts/Validate-PSO.py")
VALIDATE_PSO = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(VALIDATE_PSO)


class ValidatePSOTests(unittest.TestCase):
    def test_repository_config_and_build_scripts_are_integrated(self):
        self.assertEqual([], VALIDATE_PSO.validate_repository(ROOT, VALIDATE_PSO.PLATFORMS))

    def test_log_analysis_classifies_miss_too_late_and_hitch(self):
        with tempfile.TemporaryDirectory() as directory:
            log = Path(directory) / "capture.log"
            log.write_text(
                "PSO PRECACHING MISS:\n\tPSOPrecachingState:\t\tMissed\n"
                "PSO PRECACHING MISS:\n\tPSOPrecachingState:\t\tToo Late\n"
                "Runtime graphics PSO creation hitch (24.50 msec) for Resource Test in Pass BasePass\n",
                encoding="utf-8",
            )
            report = VALIDATE_PSO.analyze_evidence([log], [], 50.0)
            self.assertEqual(1, report["pso"]["misses"])
            self.assertEqual(1, report["pso"]["too_late"])
            self.assertEqual(1, report["pso"]["runtime_compile_hitches"])
            self.assertEqual(24.5, report["pso"]["max_runtime_compile_hitch_ms"])
            self.assertFalse(report["evidence"]["miss_classification_complete"])

    def test_csv_analysis_reports_frame_and_pso_metrics(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / "capture.csv"
            capture.write_text(
                "Metadata,Value\n"
                "FrameTime (ms),PSOPrecache/Miss,PSOPrecache/Untracked,PSOPrecache/TooLate,PSO/GraphicsPSOHitch,PSO/GraphicsPSOHitchTime\n"
                "16.6,0,0,0,0,0\n"
                "63.0,2,3,1,1,31.25\n",
                encoding="utf-8",
            )
            report = VALIDATE_PSO.analyze_evidence([], [capture], 50.0)
            self.assertEqual(2, report["pso"]["misses"])
            self.assertEqual(3, report["pso"]["untracked"])
            self.assertEqual(1, report["pso"]["too_late"])
            self.assertEqual(1, report["pso"]["runtime_compile_hitches"])
            self.assertEqual(63.0, report["frame_time"]["max_ms"])
            self.assertEqual(1, report["frame_time"]["spikes_over_threshold"])
            self.assertTrue(report["frame_time"]["threshold_exceeded"])
            self.assertTrue(report["evidence"]["miss_classification_complete"])

    def test_strict_analysis_rejects_csv_without_pso_counters(self):
        with tempfile.TemporaryDirectory() as directory:
            capture = Path(directory) / "capture.csv"
            capture.write_text("FrameTime (ms),GameThread\n16.6,8.0\n", encoding="utf-8")
            args = SimpleNamespace(
                report=None,
                strict=True,
                max_misses=0,
                max_untracked=0,
                max_too_late=0,
                max_hitches=0,
                max_frame_time_ms=50.0,
            )
            stdout = io.StringIO()
            stderr = io.StringIO()
            with redirect_stdout(stdout), redirect_stderr(stderr):
                result = VALIDATE_PSO.write_and_check_report(args, [], [capture])
            self.assertEqual(1, result)
            self.assertIn("requires PSOPrecache", stderr.getvalue())

    def test_mobile_package_requires_application_and_shader_payload(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            stage = root / "stage"
            archive = root / "archive"
            (stage / "Content/Paks").mkdir(parents=True)
            archive.mkdir()
            (stage / "Content/Paks/AshesOfHeaven-Android_ASTC.utoc").write_bytes(b"utoc")
            (archive / "AshesOfHeaven-arm64.apk").write_bytes(b"apk")
            args = SimpleNamespace(
                staged_root=stage,
                archive_root=archive,
                platform="android",
                project_root=ROOT,
                require_bundled_cache=False,
            )
            self.assertEqual(0, VALIDATE_PSO.cmd_package(args))


if __name__ == "__main__":
    unittest.main()
