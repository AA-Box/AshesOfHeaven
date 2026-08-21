# ASHES OF HEAVEN platform QA matrix

This matrix is evidence-gated. `UNTESTED` means the project has source/config support but no verified interactive run on that target. `BLOCKED BY TOOLCHAIN` means validation cannot be performed on the current build machine. Do not change a cell to `PASS` without a dated device/build record.

## Current Phase 4 source target — 2026-08-21

The current source includes four real in-engine visual target areas and the stable preview launcher `Scripts/Run-Mac-ArtTarget.sh`. The visual target layer is non-colliding and preserves the Phase 3 gameplay/nav architecture. Current source-level targets are: Erebus battlefield, Transit Station, Cathedral interior, Present-day Lucian/Maya, M91 framing, faction-aware temporary combatant silhouettes, original Cathedral glyph scaffolding, and a scalable HUD.

The table below remains evidence-gated. The current Phase 4/4.1 machine evidence is recorded immediately below; the older Phase 3.2/3.3 records remain historical. Subjective visual match, screenshots, device performance, and human interactive play remain `UNTESTED` unless explicitly recorded.

## Current Phase 4 machine evidence — 2026-08-21

- Development Editor: **PASS** — UE 5.8 Mac arm64 Development compile/link succeeded.
- Fresh commandlet: **PASS** — 14/14 checks, 0 failed checks, 0 errors, 1 known HUD-test cleanup warning.
- Full automation: **PASS** — 15/15 project tests completed with `Result={Success}`, exit code 0.
- Development package: **PASS** — `Builds/macOS-Development/AshesOfHeaven.app`; deep strict codesign passed.
- Shipping package: **PASS** — `Builds/macOS/AshesOfHeaven.app`; deep strict codesign passed.
- Normal Metal launch: **PASS** for process-level 15-second smoke on both packages, without `-nullrhi`; all five Development art-target activations (`Erebus`, `Transit`, `Cathedral`, `LucianMaya`, `M91`) also stayed alive for 8 seconds and logged their matching target marker.
- Audio initialization: **PASS** — Development packaged normal-renderer smoke without `-nosound` created the CoreAudio 48 kHz mixer, initialized the 14-cue runtime palette, and started the ambient bed; final authored soundscape remains open.
- Gameplay, interactive combat/death/restart/pickup/Manticore/checkpoint progression, screenshots, subjective art match, and target-device performance: **UNTESTED**.

| Feature | Windows | macOS | Android | iOS |
| --- | --- | --- | --- | --- |
| Launch | UNTESTED | PASS — fresh Development and Shipping packages stayed alive for 15-second normal Metal process smokes (2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Main menu | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| New game | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Save/load | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| FPS controls | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Controller | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Touch input | N/A | N/A | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Combat | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| AI | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Grenades | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Manticore | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Dialogue | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Subtitles | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Checkpoints | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Chapter completion | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Suspend/resume | N/A | N/A | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Packaging | BLOCKED BY TOOLCHAIN | PASS — fresh Development and Shipping cook/package completed; deep strict codesign passed (2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |

## Historical pre-fix Mac evidence record

- Date: 2026-08-21.
- Build: Development Editor plus Mac Shipping from `./Scripts/Build-Mac.sh`; package `Builds/macOS/AshesOfHeaven.app`; package counter `0.12`.
- Engine/toolchain: Unreal Engine 5.8 Apple Silicon toolchain on macOS.
- Renderer: normal packaged launch without `-nullrhi`; process-level Metal launch smoke check.
- Result: process remained alive for approximately 20 seconds and was stopped with a controlled interrupt; `codesign --verify --deep --strict --verbose=2` passed.
- Scope: this record proves compile/package/codesign/process launch only. It does not prove interactive combat, AI, touch/controller, checkpoint, death/restart, pickup, Manticore, dialogue, or Chapter completion behavior.

Interactive combat and full end-to-end checkpoint progression still require a human play session. Windows, Android, and signed iOS artifacts require their corresponding external build/signing environments.

## Current Phase 3.2 evidence record

- Date: 2026-08-21.
- Source target: `4aa373ad79a378f0f0daba3a449ff9df93752e14`, with the development-only telemetry and playtest launcher included in the Phase 3.2 publication.
- Fresh commandlet and automation: blocked before Unreal startup by macOS LaunchServices/HIServices XPC errors; no test result claimed.
- Development Editor: blocked by protected UnrealBuildTool trace/log paths and then a recursive UBA executor failure after safe path overrides.
- Mac Shipping: blocked by protected Unreal AutomationTool log/config/cache paths; no fresh app produced.
- Existing app codesign: passed, but the app is stale relative to the current source.
- Normal Metal launch: stale executable exited with abort status 134; no post-fix launch claim.
- Human interactive Run 1 and Run 2: `UNTESTED`.
