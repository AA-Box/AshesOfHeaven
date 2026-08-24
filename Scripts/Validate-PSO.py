#!/usr/bin/env python3
"""Validate, capture, analyze, and expand Ashes of Heaven PSO evidence.

Uses only the Python standard library so every build host can run it before
BuildCookRun. Strict analysis requires real packaged-game log or CSV evidence.
"""

from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Sequence


PLATFORMS = ("windows", "mac", "android", "ios")
ROOT = Path(__file__).resolve().parents[1]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="replace")


def require_tokens(root: Path, path: Path, tokens: Iterable[str], errors: list[str]) -> None:
    if not path.is_file():
        errors.append(f"missing required file: {path.relative_to(root)}")
        return
    text = read_text(path)
    for token in tokens:
        if token not in text:
            errors.append(f"{path.relative_to(root)} is missing: {token}")


def configured_startup_assets(default_game: Path) -> list[str]:
    return re.findall(r"^\+StartupAssets=(\S+)\s*$", read_text(default_game), re.MULTILINE)


def object_path_to_asset(root: Path, object_path: str) -> Path | None:
    package = object_path.split(".", 1)[0]
    if not package.startswith("/Game/"):
        return None
    return root / "Content" / f"{package.removeprefix('/Game/')}.uasset"


def validate_repository(root: Path, platforms: Sequence[str]) -> list[str]:
    errors: list[str] = []
    engine_ini = root / "Config/DefaultEngine.ini"
    game_ini = root / "Config/DefaultGame.ini"
    require_tokens(
        root,
        engine_ini,
        (
            "r.PSOPrecaching=1",
            "r.PSOPrecache.Components=1",
            "r.PSOPrecache.Resources=0",
            "r.PSOPrecache.Validation=1",
            "r.PSOPrecache.ProxyCreationStrategy=1",
            "r.ShaderPipelineCache.Enabled=1",
            "r.ShaderPipelineCache.StartupMode=2",
            "DefaultGraphicsRHI=DefaultGraphicsRHI_DX12",
            "+D3D12TargetedShaderFormats=PCD3D_SM6",
            "+TargetedRHIs=SF_METAL_SM6",
        ),
        errors,
    )
    require_tokens(
        root,
        game_ini,
        (
            "bUseIoStore=True",
            "bShareMaterialShaderCode=True",
            "bSharedMaterialNativeLibraries=True",
            "[/Script/AshesOfHeaven.AHShaderPipelineSettings]",
            "[ShaderPipelineCache.CacheFile]",
        ),
        errors,
    )

    startup_assets = configured_startup_assets(game_ini) if game_ini.is_file() else []
    if not startup_assets:
        errors.append("DefaultGame.ini has no shader warmup StartupAssets")
    for object_path in startup_assets:
        asset_path = object_path_to_asset(root, object_path)
        if asset_path is None:
            errors.append(f"startup preload is not a project asset: {object_path}")
        elif not asset_path.is_file():
            errors.append(f"startup preload target does not exist: {object_path}")

    coverage_markers = (
        "SKM_Rifle",
        "ShooterProjectile_Bullet",
        "ShooterProjectile_Grenade",
        "WBP_HUD_Root",
        "WBP_ManticoreHUD",
        "M_HumanArmor",
        "M_CathedralMatter",
        "M_AH_FireSprite",
        "M_AH_SmokeSoft",
        "NS_Erebus_FireSmall",
        "NS_Erebus_SmokeColumn",
    )
    for marker in coverage_markers:
        if not any(marker in value for value in startup_assets):
            errors.append(f"startup preload lacks representative coverage: {marker}")

    platform_expectations = {
        "windows": (
            root / "Config/Windows/WindowsEngine.ini",
            ("NeedsShaderStableKeys=True", "r.ShaderPipelineCache.ExcludePrecachePSO=1"),
        ),
        "mac": (root / "Config/Mac/MacEngine.ini", ("r.PSOPrecaching=1", "Metal")),
        "android": (
            root / "Config/Android/AndroidEngine.ini",
            ("bBuildForArm64=True", "bBuildForES31=True", "bSupportsVulkan=True", "bSupportsVulkanSM5=False"),
        ),
        "ios": (
            root / "Config/IOS/IOSEngine.ini",
            ("bSupportsMetal=True", "bSupportsMetalMobileSM5=False", "bSupportsMetalMobileSM6=False"),
        ),
    }
    build_scripts = {
        "windows": root / "Scripts/Build-Windows.ps1",
        "mac": root / "Scripts/Build-Mac.sh",
        "android": root / "Scripts/Build-Android.sh",
        "ios": root / "Scripts/Build-iOS.sh",
    }
    for platform in platforms:
        ini_path, tokens = platform_expectations[platform]
        require_tokens(root, ini_path, tokens, errors)
        require_tokens(root, build_scripts[platform], ("Validate-PSO.py", "package", "config"), errors)

    require_tokens(root, root / "Source/AshesOfHeaven/AshesOfHeaven.Build.cs", ('"RenderCore"',), errors)
    require_tokens(
        root,
        root / "Source/AshesOfHeaven/Performance/AHShaderPipelineWarmupSubsystem.cpp",
        ("RequestAsyncLoad", "BatchMode::Fast", "BatchMode::Background", "NumPrecompilesRemaining"),
        errors,
    )
    return errors


def print_errors(errors: Sequence[str]) -> int:
    for error in errors:
        print(f"ERROR: {error}", file=sys.stderr)
    return 1 if errors else 0


def cmd_config(args: argparse.Namespace) -> int:
    platforms = PLATFORMS if args.all_platforms else (args.platform,)
    errors = validate_repository(args.project_root.resolve(), platforms)
    if errors:
        return print_errors(errors)
    print(f"PSO config validation passed for: {', '.join(platforms)}")
    cache_dir = args.project_root / "Build/Windows/PipelineCaches"
    caches = sorted(cache_dir.glob("*AshesOfHeaven_PCD3D_SM6.spc")) if cache_dir.is_dir() else []
    if "windows" in platforms:
        print(f"Windows stable cache: {caches[0] if caches else 'not present (optional until a validated capture is expanded)'}")
    return 0


def recursive_matches(roots: Sequence[Path], patterns: Sequence[str]) -> list[Path]:
    matches: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        for pattern in patterns:
            matches.extend(path for path in root.rglob(pattern) if path.is_file() or path.is_dir())
    return sorted(set(matches))


def manifests_contain(roots: Sequence[Path], needle: str) -> bool:
    for manifest in recursive_matches(roots, ("*Manifest*.txt",)):
        if manifest.is_file() and needle.lower() in read_text(manifest).lower():
            return True
    return False


def cmd_package(args: argparse.Namespace) -> int:
    roots = [path.resolve() for path in (args.staged_root, args.archive_root) if path]
    errors: list[str] = []
    if not roots or not any(path.exists() for path in roots):
        return print_errors(["no staged or archive root exists for package validation"])

    shader_payload = recursive_matches(roots, ("*.ushaderbytecode", "*.utoc", "*.metallib"))
    if not shader_payload:
        errors.append("packaged build has no shader archive or IoStore container evidence")
    artifact_patterns = {
        "windows": ("AshesOfHeaven.exe",),
        "mac": ("AshesOfHeaven.app",),
        "android": ("*.apk", "*.aab"),
        "ios": ("*.ipa", "AshesOfHeaven*.app"),
    }
    artifacts = recursive_matches(roots, artifact_patterns[args.platform])
    if not artifacts:
        errors.append(f"no {args.platform} packaged application artifact found")

    bundled_cache: list[Path] = []
    if args.platform == "windows":
        source_dir = args.project_root / "Build/Windows/PipelineCaches"
        source_caches = sorted(source_dir.glob("*AshesOfHeaven_PCD3D_SM6.spc")) if source_dir.is_dir() else []
        bundled_cache = recursive_matches(roots, ("*AshesOfHeaven_PCD3D_SM6*.upipelinecache", "*.stable.upipelinecache"))
        cache_in_manifest = manifests_contain(roots, ".stable.upipelinecache")
        if args.require_bundled_cache and not source_caches:
            errors.append("AH_REQUIRE_PSO_CACHE requested, but no Windows PCD3D_SM6 .spc input exists")
        if source_caches and not (bundled_cache or cache_in_manifest):
            errors.append("Windows .spc input exists, but the packaged stable pipeline cache is absent")

    if errors:
        return print_errors(errors)
    print(f"PSO package validation passed for {args.platform}: {artifacts[0]}")
    print(f"Shader payload evidence: {shader_payload[0]}")
    if args.platform == "windows":
        print(f"Bundled stable cache evidence: {bundled_cache[0] if bundled_cache else 'not configured'}")
    return 0


def numeric(value: str) -> float | None:
    try:
        return float(value.strip())
    except (TypeError, ValueError):
        return None


def parse_ue_csv(path: Path, frame_threshold_ms: float = 50.0) -> dict[str, float | bool]:
    lines = read_text(path).splitlines()
    header_index = next((i for i, line in enumerate(lines) if "," in line and "frametime" in line.lower()), None)
    result: dict[str, float | bool] = {
        "misses": 0.0, "untracked": 0.0, "too_late": 0.0, "hitches": 0.0,
        "max_hitch_ms": 0.0, "max_frame_ms": 0.0, "frame_spikes": 0.0,
        "frames": 0.0, "has_pso_counters": False,
    }
    if header_index is None:
        return result
    for row in csv.DictReader(lines[header_index:]):
        result["frames"] += 1
        for name, value in row.items():
            if name is None or value is None:
                continue
            normalized = re.sub(r"[^a-z0-9]", "", name.lower())
            if "psoprecache" in normalized and normalized.endswith(("miss", "untracked", "toolate")):
                result["has_pso_counters"] = True
            number = numeric(value)
            if number is None:
                continue
            if (normalized.startswith("frametime") or normalized.endswith("frametime")) and "hitch" not in normalized:
                result["max_frame_ms"] = max(result["max_frame_ms"], number)
                if number > frame_threshold_ms:
                    result["frame_spikes"] += 1
            elif "psoprecache" in normalized and normalized.endswith("miss"):
                result["misses"] += number
            elif "psoprecache" in normalized and normalized.endswith("untracked"):
                result["untracked"] += number
            elif "psoprecache" in normalized and normalized.endswith("toolate"):
                result["too_late"] += number
            elif ("graphicspsohitch" in normalized or "computepsohitch" in normalized) and "time" not in normalized:
                result["hitches"] += number
            elif "graphicspsohitchtime" in normalized or "computepsohitchtime" in normalized:
                result["max_hitch_ms"] = max(result["max_hitch_ms"], number)
    return result


def analyze_evidence(log_paths: Sequence[Path], csv_paths: Sequence[Path], frame_threshold_ms: float) -> dict[str, object]:
    log_text = "\n".join(read_text(path) for path in log_paths if path.is_file())
    states: list[str] = []
    for block in re.split(r"(?=PSO PRECACHING MISS:)", log_text, flags=re.IGNORECASE):
        if not re.match(r"PSO PRECACHING MISS:", block, flags=re.IGNORECASE):
            continue
        match = re.search(r"PSOPrecachingState:\s*([^\r\n]+)", block, flags=re.IGNORECASE)
        states.append(match.group(1).strip().lower() if match else "unknown")
    log_missed = sum(state.startswith("miss") or state == "unknown" for state in states)
    log_untracked = sum(state.startswith("untracked") for state in states)
    log_too_late = sum("too late" in state or "toolate" in state for state in states)
    hitch_values = [float(value) for value in re.findall(
        r"Runtime (?:graphics|compute) PSO creation hitch \(([0-9.]+) msec\)", log_text, re.IGNORECASE
    )]

    csv_totals: dict[str, float | bool] = {
        "misses": 0.0, "untracked": 0.0, "too_late": 0.0, "hitches": 0.0,
        "max_hitch_ms": 0.0, "max_frame_ms": 0.0, "frame_spikes": 0.0,
        "frames": 0.0, "has_pso_counters": False,
    }
    for path in csv_paths:
        if not path.is_file():
            continue
        parsed = parse_ue_csv(path, frame_threshold_ms)
        for key in ("misses", "untracked", "too_late", "hitches", "frame_spikes", "frames"):
            csv_totals[key] += parsed[key]
        for key in ("max_hitch_ms", "max_frame_ms"):
            csv_totals[key] = max(csv_totals[key], parsed[key])
        csv_totals["has_pso_counters"] = bool(csv_totals["has_pso_counters"]) or bool(parsed["has_pso_counters"])

    return {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "evidence": {
            "logs": [str(path) for path in log_paths if path.is_file()],
            "csv": [str(path) for path in csv_paths if path.is_file()],
            "csv_frames": round(csv_totals["frames"]),
            "log_validation_events": len(states),
            "csv_pso_counters_present": bool(csv_totals["has_pso_counters"]),
            "miss_classification_complete": bool(csv_totals["has_pso_counters"]),
        },
        "pso": {
            "misses": max(log_missed, round(csv_totals["misses"])),
            "untracked": max(log_untracked, round(csv_totals["untracked"])),
            "too_late": max(log_too_late, round(csv_totals["too_late"])),
            "runtime_compile_hitches": max(len(hitch_values), round(csv_totals["hitches"])),
            "max_runtime_compile_hitch_ms": round(max(max(hitch_values, default=0.0), csv_totals["max_hitch_ms"]), 3),
        },
        "frame_time": {
            "max_ms": round(csv_totals["max_frame_ms"], 3),
            "spikes_over_threshold": round(csv_totals["frame_spikes"]),
            "threshold_ms": frame_threshold_ms,
            "threshold_exceeded": csv_totals["max_frame_ms"] > frame_threshold_ms,
        },
    }


def collect_paths(explicit: Sequence[Path], roots: Sequence[Path], suffix: str) -> list[Path]:
    found = [path.resolve() for path in explicit]
    for root in roots:
        if root.exists():
            found.extend(path.resolve() for path in root.rglob(f"*{suffix}") if path.is_file())
    return sorted(set(found))


def write_and_check_report(args: argparse.Namespace, log_paths: Sequence[Path], csv_paths: Sequence[Path]) -> int:
    report = analyze_evidence(log_paths, csv_paths, args.max_frame_time_ms)
    if args.report:
        output = args.report.resolve()
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))
    evidence = report["evidence"]
    pso = report["pso"]
    failures: list[str] = []
    if args.strict and not evidence["csv"]:
        failures.append("strict mode requires real CSV evidence for frame-time and PSO counters")
    if args.strict and evidence["csv"] and not evidence["csv_frames"]:
        failures.append("strict mode requires parsed frame-time samples in CSV evidence")
    if args.strict and evidence["csv"] and not evidence["csv_pso_counters_present"]:
        failures.append("strict mode requires PSOPrecache miss/untracked/too-late counter columns")
    if args.strict and pso["misses"] > args.max_misses:
        failures.append(f"PSO misses {pso['misses']} exceed {args.max_misses}")
    if args.strict and pso["untracked"] > args.max_untracked:
        failures.append(f"untracked PSOs {pso['untracked']} exceed {args.max_untracked}")
    if args.strict and pso["too_late"] > args.max_too_late:
        failures.append(f"too-late PSOs {pso['too_late']} exceed {args.max_too_late}")
    if args.strict and pso["runtime_compile_hitches"] > args.max_hitches:
        failures.append(f"PSO compile hitches {pso['runtime_compile_hitches']} exceed {args.max_hitches}")
    if args.strict and report["frame_time"]["threshold_exceeded"]:
        failures.append(f"max frame time exceeds {args.max_frame_time_ms} ms")
    return print_errors(failures)


def cmd_analyze(args: argparse.Namespace) -> int:
    roots = [path.resolve() for path in args.evidence_dir]
    return write_and_check_report(args, collect_paths(args.log, roots, ".log"), collect_paths(args.csv, roots, ".csv"))


def find_executable(package: Path, platform: str) -> Path | None:
    if package.is_file():
        return package
    if platform == "mac":
        matches = sorted(path for path in package.rglob("Contents/MacOS/AshesOfHeaven*") if path.is_file())
    else:
        matches = sorted(path for path in package.rglob("AshesOfHeaven*.exe") if path.is_file())
    return matches[0] if matches else None


def cmd_run(args: argparse.Namespace) -> int:
    executable = find_executable(args.package.resolve(), args.platform)
    if executable is None:
        print(f"ERROR: could not find packaged executable under {args.package}", file=sys.stderr)
        return 2
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    user_dir = output / "User"
    user_dir.mkdir(parents=True, exist_ok=True)
    log_path = output / "AshesOfHeaven-PSO.log"
    trace_path = output / "AshesOfHeaven-PSO.utrace"
    command = [
        str(executable), "-log", "-stdout", "-FullStdOutLogOutput", "-unattended", f"-userdir={user_dir}",
        "-ini:Engine:[SystemSettings]:r.PSOPrecache.Validation=2",
        "-ini:Engine:[SystemSettings]:r.ShaderPipelineCache.LogPSO=1",
        "-LogCmds=LogPSOHitching Verbose,LogEngine Log",
        "-trace=default,cpu,frame,bookmark", f"-tracefile={trace_path}", "-tracefiletrunc",
        "-csvCategories=PSO,PSOPrecache", f"-csvCaptureFrames={args.capture_frames}", "-csvGpuStats",
    ]
    if args.collect_pso:
        command.append("-logPSO")
    if args.map:
        command.append(args.map)
    command.extend(args.game_arg)
    print("Launching with UE 5.8 detailed PSO validation. Complete the documented checklist, then exit normally.")
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    timed_out = False
    try:
        output_text, _ = process.communicate(timeout=args.duration if args.duration > 0 else None)
    except subprocess.TimeoutExpired:
        timed_out = True
        process.terminate()
        try:
            output_text, _ = process.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            process.kill()
            output_text, _ = process.communicate()
    log_path.write_text(output_text or "", encoding="utf-8")
    if output_text:
        print(output_text, end="" if output_text.endswith("\n") else "\n")
    if process.returncode not in (0, -15) and not timed_out:
        print(f"ERROR: packaged game exited with code {process.returncode}", file=sys.stderr)
        return process.returncode or 1
    recorded_files = sorted(path for path in user_dir.rglob("*.rec.upipelinecache") if path.is_file())
    if recorded_files:
        collected_dir = output / "CollectedPSOs"
        collected_dir.mkdir(parents=True, exist_ok=True)
        for recorded_file in recorded_files:
            shutil.copy2(recorded_file, collected_dir / recorded_file.name)
        print(f"Collected {len(recorded_files)} bound PSO recording(s) under {collected_dir}")
    csvs = sorted(path for path in output.rglob("*.csv") if path.is_file())
    return write_and_check_report(args, [log_path], csvs)


def cmd_expand(args: argparse.Namespace) -> int:
    recorded = args.recorded_dir.resolve()
    stable_keys = args.stable_keys_dir.resolve()
    errors = []
    if not list(recorded.glob("*.rec.upipelinecache")):
        errors.append(f"no .rec.upipelinecache files under {recorded}")
    if not list(stable_keys.glob("*.shk")):
        errors.append(f"no .shk stable-key files under {stable_keys}")
    if errors:
        return print_errors(errors)
    output = args.output.resolve()
    if "AshesOfHeaven_PCD3D_SM6" not in output.name or output.suffix.lower() != ".spc":
        print("ERROR: Windows output name must contain AshesOfHeaven_PCD3D_SM6 and end in .spc", file=sys.stderr)
        return 2
    if output.exists() and not args.force:
        print(f"ERROR: {output} exists; pass --force to intentionally replace it", file=sys.stderr)
        return 2
    editor = args.editor_cmd.resolve() if args.editor_cmd else args.engine_root.resolve() / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
    if not editor.is_file():
        print(f"ERROR: UnrealEditor-Cmd.exe not found at {editor}", file=sys.stderr)
        return 2
    output.parent.mkdir(parents=True, exist_ok=True)
    command = [
        str(editor), str(args.project_file.resolve()), "-run=ShaderPipelineCacheTools", "expand",
        str(recorded / "*.rec.upipelinecache"), str(stable_keys / "*.shk"), str(output), "-unattended", "-nop4",
    ]
    print("Expanding validated Windows DX12 SM6 recordings into a stable cache...")
    completed = subprocess.run(command, check=False)
    if completed.returncode != 0 or not output.is_file():
        print(f"ERROR: ShaderPipelineCacheTools failed with code {completed.returncode}", file=sys.stderr)
        return completed.returncode or 1
    print(f"Stable cache written: {output} ({output.stat().st_size} bytes)")
    return 0


def add_threshold_arguments(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--strict", action="store_true")
    parser.add_argument("--max-misses", type=int, default=0)
    parser.add_argument("--max-untracked", type=int, default=0)
    parser.add_argument("--max-too-late", type=int, default=0)
    parser.add_argument("--max-hitches", type=int, default=0)
    parser.add_argument("--max-frame-time-ms", type=float, default=50.0)
    parser.add_argument("--report", type=Path)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    config = subparsers.add_parser("config", help="Validate config, assets, and build integration.")
    config.add_argument("--project-root", type=Path, default=ROOT)
    group = config.add_mutually_exclusive_group(required=True)
    group.add_argument("--platform", choices=PLATFORMS)
    group.add_argument("--all-platforms", action="store_true")
    config.set_defaults(func=cmd_config)
    package = subparsers.add_parser("package", help="Validate packaged shader/cache data.")
    package.add_argument("--project-root", type=Path, default=ROOT)
    package.add_argument("--platform", choices=PLATFORMS, required=True)
    package.add_argument("--staged-root", type=Path)
    package.add_argument("--archive-root", type=Path)
    package.add_argument("--require-bundled-cache", action="store_true")
    package.set_defaults(func=cmd_package)
    analyze = subparsers.add_parser("analyze", help="Analyze real logs and CSVs.")
    analyze.add_argument("--evidence-dir", type=Path, action="append", default=[])
    analyze.add_argument("--log", type=Path, action="append", default=[])
    analyze.add_argument("--csv", type=Path, action="append", default=[])
    add_threshold_arguments(analyze)
    analyze.set_defaults(func=cmd_analyze)
    run = subparsers.add_parser("run", help="Launch a packaged desktop capture.")
    run.add_argument("--platform", choices=("windows", "mac"), required=True)
    run.add_argument("--package", type=Path, required=True)
    run.add_argument("--output", type=Path, default=ROOT / "Saved/PSOValidation" / datetime.now().strftime("%Y%m%d-%H%M%S"))
    run.add_argument("--map")
    run.add_argument("--capture-frames", type=int, default=36000)
    run.add_argument("--duration", type=int, default=0)
    run.add_argument("--collect-pso", action="store_true")
    run.add_argument("--game-arg", action="append", default=[])
    add_threshold_arguments(run)
    run.set_defaults(func=cmd_run)
    expand = subparsers.add_parser("expand", help="Build optional Windows PCD3D_SM6 stable cache.")
    expand.add_argument("--engine-root", type=Path, required=True)
    expand.add_argument("--editor-cmd", type=Path)
    expand.add_argument("--project-file", type=Path, default=ROOT / "AshesOfHeaven.uproject")
    expand.add_argument("--recorded-dir", type=Path, required=True)
    expand.add_argument("--stable-keys-dir", type=Path, required=True)
    expand.add_argument("--output", type=Path, default=ROOT / "Build/Windows/PipelineCaches/PSO_AshesOfHeaven_PCD3D_SM6.spc")
    expand.add_argument("--force", action="store_true")
    expand.set_defaults(func=cmd_expand)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
