# PSO and Shader Pipeline

This document defines the shader and pipeline-state-object (PSO) policy for Ashes of Heaven on Unreal Engine 5.8. It also records the pre-implementation audit so future changes can be compared with the original state.

## Pre-change audit

Recorded on 2026-08-24 before changing project PSO configuration.

### Engine and project

- `AshesOfHeaven.uproject` associates the project with Unreal Engine 5.8.
- The installed engine used for this audit is UE 5.8.1, changelist 56057345 (`++UE5+Release-5.8`).
- Both project targets use `BuildSettingsVersion.V7` and `EngineIncludeOrderVersion.Unreal5_8`.
- The project uses the mobile shading path with deferred mobile rendering. Mobile device profiles disable high-cost desktop features such as Nanite, Virtual Shadow Maps, Lumen, and ray tracing.

### Existing PSO state

Before this work, `Config/DefaultEngine.ini` contained only:

```ini
r.PSOPrecache.ProxyCreationStrategy=1
PSOPrecacheFallbackMaterial=None
```

There was no explicit project policy for `r.PSOPrecaching`, `ShaderPipelineCache`, validation, stable shader keys, pipeline-cache collection, or startup warmup. UE 5.8 itself defaults `r.PSOPrecaching` to enabled and defaults the proxy creation strategy to `1`, so automatic precaching was active implicitly rather than intentionally configured.

Existing packaged Mac output contained the normal Metal shader archives but no `.spc` or `.stable.upipelinecache` files. No generated PSO cache, PSO validation log, PSO trace, or PSO-specific CSV evidence was present in the repository or current build output. Consequently, the prior state has no quantitative baseline for PSO misses or PSO compilation hitches.

### Target RHIs

| Target | Pre-change RHI and shader format | Audit conclusion |
| --- | --- | --- |
| Windows | DirectX 12, `PCD3D_SM6` | Automatic precaching applies. This is the only target for which this project will optionally ship a collected stable `.spc` cache, because it can cover validated residual misses on the target driver/RHI workflow. |
| macOS Apple Silicon | Metal, `SF_METAL_SM6` | Use UE automatic precaching and the UE 5.8 Apple IoStore Metal shader archive. A Windows-recorded cache is not portable to Metal. |
| Android | ARM64 ASTC package; Vulkan ES 3.1 and OpenGL ES 3.1 shader formats are enabled by UE defaults | Use UE automatic/adaptive mobile precaching. A bundled cache is not enabled by default: Vulkan and GLES recordings are distinct, and UE documents the older Android bundled workflow as legacy for modern engines. |
| iOS | Metal ES 3.1 (`SF_METAL_ES3_1_IOS`) | Use UE automatic/adaptive mobile precaching and the Apple shader archive. Do not add a large device-startup PSO compile. |

The Android and iOS RHI settings were inherited from UE defaults rather than explicitly pinned in project platform configuration.

### Engine 5.8 behavior relevant to the design

- Automatic PSO precaching is enabled by default in UE 5.8.
- Component precaching and the proxy-delay strategy are supported. The older `r.PSOPrecache.ProxyCreationWhenPSOReady` switch used by the UE 5.6 reference is deprecated and is not copied.
- Validation mode `1` is lightweight and available in Shipping. Validation mode `2` provides detailed Development diagnostics and must be set early because the console variable is read-only after startup.
- UE 5.8 adapts precaching for mobile feature levels. In particular, it reduces global/default-material/Slate work and Niagara precache time on ES 3.1. The project must not globally override those reductions.
- Niagara compute PSO precaching is already supported by the engine. Stable-cache cooking also has special handling for Niagara compute PSOs.
- Stable cache input is RHI-specific. UE's cooker reads stable cache input from `Build/<Platform>/PipelineCaches` and emits the cooked `.stable.upipelinecache` under the packaged content pipeline-cache directory.
- UE 5.8 Apple targets use the IoStore Metal shader code archive by default. This is separate from the Windows `.spc` collection workflow.

### Existing build pipeline

The project already builds with:

- `Scripts/Build-Windows.ps1`
- `Scripts/Build-Mac.sh`
- `Scripts/Build-Android.sh`
- `Scripts/Build-iOS.sh`

All four scripts run `BuildCookRun`, but none previously validated shader/PSO configuration, inspected packaged PSO data, enabled strict PSO verification, or collected performance evidence. The existing `Scripts/Validate-CrossPlatform.sh` performed general static checks only.

### Reference-project findings

The ActionRoguelike UE 5.6 reference explicitly enables automatic precaching, validation, stable shader keys, and `ShaderPipelineCache`. It includes a hard-coded Windows batch file that expands a recorded `.rec.upipelinecache` plus `.shk` metadata into a DirectX stable `.spc` cache.

Useful ideas retained here are automatic precaching, residual-miss capture, stable-key expansion for Windows, and querying remaining precompiles. The deprecated proxy switch, hard-coded UE 5.3/Windows paths, Windows-only batch assumptions, and unconditional detailed validation are not copied.

## Selected UE 5.8 policy

The production policy is layered:

1. Automatic PSO precaching is the primary mechanism on every platform.
2. A small asynchronous startup asset preload exposes common combat, HUD, enemy, Cathedral, projectile, and Niagara resources early without blocking mobile startup.
3. Pipeline compilation uses a fast batch while the startup preload is active, then returns to background batching.
4. Development capture uses detailed validation, Insights tracing, CSV profiling, and optional bound-PSO recording.
5. Windows DX12 SM6 may ship a stable bundled cache generated only from demonstrated residual misses. Other platforms do not consume that Windows artifact.

The remainder of this document is completed alongside the implementation and validation tooling.

## Implemented configuration

### Shared policy

`Config/DefaultEngine.ini` now explicitly enables component automatic precaching, lightweight production validation, and `ShaderPipelineCache`. Resource-only precaching remains disabled because UE 5.8 documents it as experimental. The project starts bundled-cache work in background mode; the bounded startup preload temporarily selects the fast batch and then restores background mode.

Detailed validation and bound-PSO logging are deliberately not production defaults. `Scripts/Validate-PSO.py run` sets validation mode `2` on the process command line before the read-only console variable is initialized and enables verbose `LogPSOHitching` output for a Development capture.

Packaging explicitly enables IoStore, shared material shader code, and native shader libraries. The runtime-loaded M91 rifle and template projectile directories are also explicit cook inputs.

### Startup asset preload

`UAHShaderPipelineWarmupSubsystem` requests one asynchronous high-priority load for 15 representative assets. The set covers:

- M91 mesh and material;
- bullet and grenade projectile Blueprints;
- root HUD and Manticore HUD;
- common human/enemy, Veil, Cathedral, fire, and smoke materials;
- Erebus fire, wreck-fire, smoke-column, and local-smoke Niagara systems.

The handle is retained for the game-instance lifetime so these small common resources remain ready. There is no synchronous asset load, busy wait, or “compile everything” loop. `AH.PSO.Status` and `UAHShaderPipelineWarmupSubsystem::GetNumPipelinePrecompilesRemaining` expose the remaining bundled work for diagnostics or a future loading-screen widget.

No authored shotgun asset exists in the current content tree or source definitions. The validation route therefore exercises the M91 plus bullet/grenade paths; shotgun coverage remains a required checklist item when that asset is added.

### Per-platform policy

| Platform | Production mechanism | Bundled-cache decision |
| --- | --- | --- |
| Windows DX12 SM6 | Automatic precaching, startup asset exposure, shared shader library, optional stable pipeline cache | `Config/Windows/WindowsEngine.ini` generates stable keys and excludes already-precacheable PSOs from a collected cache. A `PCD3D_SM6` `.spc` is shipped only after a validated Windows capture is expanded. |
| macOS Metal SM6 | Automatic precaching, startup asset exposure, UE 5.8 Metal IoStore shader archive | No Windows cache is copied. A separate Metal manual bundle is not enabled without measured residual misses. |
| Android ARM64, Vulkan ES 3.1 and GLES ES 3.1 | UE automatic/adaptive ES 3.1 precaching and normal platform shader/driver caches | No legacy Android bundle by default. Vulkan and GLES would require separate evidence and separate recordings. |
| iOS Metal ES 3.1 | UE automatic/adaptive mobile precaching and Apple shader archive | No large startup bundle; the target is pinned to Metal ES 3.1, with mobile SM5/SM6 disabled. |

The platform `.ini` files pin the audited RHI choices so a later engine upgrade cannot silently change the cook matrix. Re-audit these settings whenever the engine association changes.

## Build integration

Every existing build entry point performs two checks:

1. `Validate-PSO.py config --platform <target>` before `BuildCookRun` verifies UE 5.8 keys, RHI policy, startup asset paths, runtime integration, and the target script itself.
2. `Validate-PSO.py package` after staging verifies that the application artifact and a shader archive/IoStore container exist.

Windows additionally checks that any `.spc` source input produced a packaged `.stable.upipelinecache`. Set `AH_REQUIRE_PSO_CACHE=1` on a Windows Shipping build when the release is expected to contain the optional cache; the build fails if the input or cooked output is absent.

All scripts accept `CLIENT_CONFIG=Development` for diagnostic packages and default to `Shipping`. Store distribution remains Shipping-only.

Generated recordings, shader stable-key files, Insights traces, and local build output are ignored. A deliberately generated Windows `.spc` is not globally ignored because it is the intentional Shipping input under `Build/Windows/PipelineCaches`; review its size and capture provenance before committing it.

## Capture and validation workflow

### 1. Build a Development package

On macOS:

```bash
CLIENT_CONFIG=Development Scripts/Build-Mac.sh
```

On Windows PowerShell:

```powershell
$env:CLIENT_CONFIG = "Development"
Scripts/Build-Windows.ps1
```

Use Development, not Shipping, for detailed mode-2 miss descriptions and verbose hitch logging.

### 2. Start a clean desktop capture

The runner assigns a unique `-userdir`, so the test starts without a previous user pipeline cache. It captures a `.utrace`, CSV categories `PSO` and `PSOPrecache`, verbose runtime PSO hitch lines, and a JSON summary.

Mac example:

```bash
python3 Scripts/Validate-PSO.py run \
  --platform mac \
  --package Builds/macOS-Development/AshesOfHeaven.app \
  --output Saved/PSOValidation/mac-cold \
  --report Saved/PSOValidation/mac-cold/report.json
```

Windows example, with residual bound-PSO collection enabled:

```powershell
py -3 Scripts/Validate-PSO.py run `
  --platform windows `
  --package Builds/Windows-Development `
  --output Saved/PSOValidation/windows-cold `
  --collect-pso `
  --report Saved/PSOValidation/windows-cold/report.json
```

At the end of a run, enter these console commands before exiting:

```text
r.PSOPrecache.DumpStats
AH.PSO.Status
```

Open the `.utrace` in Unreal Insights. Inspect Game Thread and Render Thread frame spikes, PSO compile scopes, and the event context surrounding each spike. The JSON/CSV count identifies whether a spike coincided with `GraphicsPSOHitch`, `ComputePSOHitch`, `Miss`, or `TooLate`; a trace is still required to distinguish PSO cost from streaming or gameplay cost.

### 3. Representative coverage checklist

Use one uninterrupted cold run and record a trace bookmark or timestamp for each item:

1. Boot through the front-end and open/close the main menu.
2. Play the Erebus opening until control and the authored presentation are visible.
3. Trigger the first firefight; aim and fire the M91. Exercise the shotgun here when an authored shotgun exists.
4. Throw and detonate a grenade; also observe bullet impacts.
5. Kill at least one Veil enemy and observe hit/death materials and effects. `KillAllEnemies` can force the death path in Development.
6. Stand near Erebus fire and smoke long enough to exercise Niagara fire, wreck fire, local smoke, and the smoke column.
7. Exercise HUD damage, objectives, pause/menu, and return-to-game. `AH.Debug.UI` and `AH.Debug.UI.Damage` provide Development diagnostics.
8. Exercise Manticore if available. `ChapterSpawnManticore` forces its actor/HUD path in Development; `ChapterTeleportCathedral` covers Cathedral materials.

Repeat the identical route in a second fresh-userdir capture after integrating a new stable cache. Compare counts and frame-time spikes; do not compare a warm driver-cache run with a cold baseline.

### 4. Strict local or CI analysis

Analyze logs/CSVs pulled from desktop or device tooling:

```bash
python3 Scripts/Validate-PSO.py analyze \
  --evidence-dir Saved/PSOValidation/mac-cold \
  --strict \
  --max-misses 0 \
  --max-untracked 0 \
  --max-too-late 0 \
  --max-hitches 0 \
  --max-frame-time-ms 50 \
  --report Saved/PSOValidation/mac-cold/strict-report.json
```

Strict mode fails if evidence is missing, the CSV contains no parsed frames or `PSOPrecache` counter columns, or any configured threshold is exceeded. Reports explicitly state whether miss classification is complete. A non-strict run always reports the available measurements without turning incomplete coverage into a false success.

For Android, build with `CLIENT_CONFIG=Development`, launch/install through the normal device pipeline, capture `adb logcat`, and pull the game CSV/profile directory before using `analyze`. Test Vulkan and GLES separately on devices that actually select each RHI. For iOS, use a Development package and collect the Xcode device console plus the app's CSV/profile container. Mobile collection does not enable a desktop `.spc` workflow.

## Optional Windows stable-cache collection

Only do this after the representative Windows run reports residual DX12 SM6 misses or first-use global PSOs:

1. Run the Windows Development capture with `--collect-pso` on representative DX12 machines.
2. Keep the resulting `.rec.upipelinecache` files grouped by project version and `PCD3D_SM6` RHI.
3. Run a Windows cook with stable keys enabled, locate the cook's `.shk` metadata under `Saved/Cooked/Windows`, and use the exact directory containing those files.
4. Expand the recordings on Windows:

```powershell
py -3 Scripts/Validate-PSO.py expand `
  --engine-root "C:\Program Files\Epic Games\UE_5.8" `
  --recorded-dir Saved\PSOValidation\windows-cold\CollectedPSOs `
  --stable-keys-dir Saved\Cooked\Windows\AshesOfHeaven\Metadata\PipelineCaches `
  --output Build\Windows\PipelineCaches\PSO_AshesOfHeaven_PCD3D_SM6.spc
```

The command refuses an incorrectly named output and refuses to overwrite an existing cache without `--force`. Rebuild Shipping with `AH_REQUIRE_PSO_CACHE=1`, then repeat the cold representative run and compare evidence. Never reuse this cache for Metal, Vulkan, or GLES.

## Automated validation

`Scripts/Validate-CrossPlatform.sh` now runs shell syntax checks, the all-platform PSO config/asset/build validation, and `Scripts/tests/test_validate_pso.py`. The tests cover repository integration plus parsing of real UE-style miss, too-late, hitch, and frame-time evidence. They are configuration/build-tool tests, not simulated rendering tests.

## Current evidence

As of the implementation on 2026-08-24:

- All four platform configuration and asset-path checks pass.
- Python parser tests and shell syntax checks pass.
- The package validator finds the application and `AshesOfHeaven-Mac.utoc` in the previously staged Mac build. That package predates this implementation, so it validates the inspection path only and is not runtime performance evidence.
- No generated stable cache is committed; Windows reports the optional cache as not present.
- The new settings and warmup translation units compile successfully with UE 5.8.1 for Mac arm64. The full target remains blocked by unrelated pre-existing worktree errors, including the enemy validation commandlet and a corrupted/project-specific projectile header; earlier attempts also exposed pooling, encounter, and world-state errors.
- A live UE 5.8.1 MacEditor MCP probe verified the active policy: `r.PSOPrecaching=1`, component precaching enabled, resource precaching disabled, proxy creation strategy `1`, `r.ShaderPipelineCache.Enabled=1`, startup mode `2`, and Mac `ExcludePrecachePSO=0`. The Editor build did not register `r.PSOPrecache.Validation`, so this probe cannot classify PSO misses or too-late precaches.
- The Asset Registry loaded all 15 configured warmup assets. PIE entered `/Game/ChapterOne/L_ChapterOne_Greybox` at the Erebus opening battle, opened the front-end/HUD path, reported 174 presentation assets with no missing presentation assets or failed mesh/VFX loads, and completed the bounded preload for 15 assets. The logged `pipeline precompiles remaining=0` is the pipeline-cache queue state at that instant, not proof that every automatic PSO was ready.
- PIE startup took 0.462 seconds. On the first rendered frames, the process-wide hitch counter reached 150 graphics and 0 compute PSO creation hitches, with 0 reported as precached. Eighteen verbose hitch records ranged from 25.13 ms to 77.10 ms and primarily identified Bloom, Local Exposure, Lumen card capture, and non-Nanite Virtual Shadow Map work. Each record had precache status `Unknown`; they are evidence of residual editor hitches, not a valid miss count.
- This was not a clean packaged benchmark: the same editor process had earlier Zen DDC `507 Insufficient Storage` failures, no CSV or Unreal Insights trace was captured, the counter is process-wide, and the firefight/grenade/death/Manticore route was not driven. An updated Development package still cannot be produced from the current unrelated dirty-worktree compile failures. Packaged PSO misses and representative frame-time spikes therefore remain **not measured**, not zero.

Before release sign-off, restart with a writable DDC and enough disk space, build an updated Development package, and run the complete representative route through `Validate-PSO.py run`. Archive its JSON, CSV, trace, and log evidence, then repeat the run warm to distinguish unavoidable driver-cache cold work from repeatable misses. Do not add a Mac, Vulkan, GLES, or Metal bundle solely from this Editor probe.

Update this section (or attach the generated JSON reports to the release evidence) after each target's representative capture.

## UE 5.8 references

- [PSO precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine)
- [Manually creating bundled PSO caches](https://dev.epicgames.com/documentation/en-us/unreal-engine/manually-creating-bundled-pso-caches-in-unreal-engine)
- [Android bundled PSO cache legacy workflow](https://dev.epicgames.com/documentation/en-us/unreal-engine/creating-bundled-pso-caches-for-android-in-unreal-engine)
- [Apple shader library selection](https://dev.epicgames.com/documentation/en-us/unreal-engine/shader-library-selection-for-apple-in-unreal-engine)
