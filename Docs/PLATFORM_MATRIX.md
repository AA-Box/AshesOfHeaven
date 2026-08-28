# ASHES OF HEAVEN platform QA matrix

This matrix is evidence-gated. `UNTESTED` means the project has source/config support but no verified interactive run on that target. `BLOCKED BY TOOLCHAIN` means validation cannot be performed on the current build machine. Do not change a cell to `PASS` without a dated device/build record.

## Current Phase 4 source target — 2026-08-22

The current source includes four real in-engine visual target areas and the stable preview launcher `Scripts/Run-Mac-ArtTarget.sh`. The visual target layer is non-colliding and preserves the Phase 3 gameplay/nav architecture. Current source-level targets are: Erebus battlefield, Transit Station, Cathedral interior, Present-day Lucian/Maya, M91 framing, faction-aware temporary combatant silhouettes, original Cathedral glyph scaffolding, and a scalable HUD.

The table below remains evidence-gated. The current Phase 4/4.1 machine evidence is recorded immediately below; the older Phase 3.2/3.3 records remain historical. Subjective visual match, screenshots, device performance, and human interactive play remain `UNTESTED` unless explicitly recorded.

## Current Phase 4 machine evidence — 2026-08-22

- Development Editor: **PASS** — UE 5.8 Mac arm64 Development compile/link succeeded.
- Fresh commandlet: **PASS** — 15/15 checks, 0 failed checks and 0 errors; two known HUD-test teardown warnings after the passing checks.
- Full automation: **PASS** — 16/16 project tests completed with `Result={Success}`, exit code 0; no project test failed.
- Development package: **PASS** — `Builds/macOS-Development/AshesOfHeaven.app`; deep strict codesign passed.
- Shipping package: **PASS** — `Builds/macOS/AshesOfHeaven.app`; deep strict codesign passed.
- Normal Metal launch: **PASS** for process-level 15-second smoke on both fresh packages, without `-nullrhi` or `-nosound`; both exited with controlled status 0 after the smoke window (2026-08-22).
- Audio initialization: **PASS** — Development packaged normal-renderer smoke without `-nosound` created the CoreAudio 48 kHz mixer, initialized the 14-cue runtime palette, and started the ambient bed; final authored soundscape remains open.
- Gameplay, interactive combat/death/restart/pickup/Manticore/checkpoint progression, screenshots, subjective art match, and target-device performance: **UNTESTED**.

| Feature | Windows | macOS | Android | iOS |
| --- | --- | --- | --- | --- |
| Launch | UNTESTED | PASS — fresh Development and Shipping packages stayed alive for 15-second normal Metal process smokes (2026-08-22) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Main menu | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| New game | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Save/load | UNTESTED | AUTOMATED — packaged completion write + relaunch verified by `Scripts/Run-LevelOneE2E.sh`; human load-from-menu UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| FPS controls | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Controller | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Touch input | N/A | N/A | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Combat | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| AI | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Grenades | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Manticore | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Dialogue | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Subtitles | UNTESTED | UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Checkpoints | UNTESTED | AUTOMATED — capture/restore incl. inventory, encounter and Manticore state (`CampaignE2E.DeathReloadRestoresRunState`); human death-in-combat UNTESTED | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Chapter completion | UNTESTED | AUTOMATED — twelve objectives to `ChapterComplete`, persisted and surviving a process restart (`CampaignE2E.*`, `Scripts/Run-LevelOneE2E.sh`) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Suspend/resume | N/A | N/A | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
| Packaging | BLOCKED BY TOOLCHAIN | PASS — fresh Development and Shipping cook/package completed; deep strict codesign passed (2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |

## Historical pre-fix Mac evidence record

- Date: 2026-08-21.
- Build: Development Editor plus Mac Shipping from `./Scripts/Build-Mac.sh`; package `Builds/macOS/AshesOfHeaven.app`; package counter `0.12`.
- Engine/toolchain: Unreal Engine 5.8 Apple Silicon toolchain on macOS.
- Renderer: normal packaged launch without `-nullrhi`; process-level Metal launch smoke check.
- Result: process remained alive for approximately 20 seconds and was stopped with a controlled interrupt; `codesign --verify --deep --strict --verbose=2` passed.
- Scope: this record proves compile/package/codesign/process launch only. It does not prove interactive combat, AI, touch/controller, checkpoint, death/restart, pickup, Manticore, dialogue, or Chapter completion behavior.

The campaign lifecycle now has automated evidence, and it is worth being exact about what that
does and does not cover.

Covered without a human: `AshesOfHeaven.LevelOne.CampaignE2E.*` plays all twelve objectives on
the real director, boards and fires the Manticore, inspects and confirms the failsafe terminal,
takes a mid-run checkpoint and restores health/armour/ammo/grenades/objective/encounter/Manticore
state after a simulated death, finishes the chapter, and — after destroying the world and the
game instance — boots a second session through the real `AAHChapterOneGameMode` restore path and
asserts Level One is still complete. `Scripts/Run-LevelOneE2E.sh` repeats the completion and
relaunch halves across a real process boundary in a packaged Development build.

How the automated half is enforced without a runner: the suite runs on a developer machine and
records `Docs/automation-evidence.json` against a hash of `Source`, `Config`, `Content`,
`Scripts` and the `.uproject`; `source-validation` re-checks that hash on a hosted runner on
every pull request. `automation-tests` and `macos-shipping` stay gated on
`vars.UNREAL_ENGINE_MAC_ROOT` and run only where a self-hosted UE5/macOS runner exists — which
is deliberately not this public repository.

Still `UNTESTED`: everything a person does. Movement and aiming, whether an encounter is
survivable or fair, difficulty, readability under motion, controller feel, subjective art match,
and device performance. The packaged run is driven by `-LevelOneAutoplay` because synthetic
keyboard and mouse input does not reach the packaged game on this platform; it proves the
progression and persistence contract, not that the level plays well. Windows, Android, and signed
iOS artifacts require their corresponding external build/signing environments.

## Current Phase 3.2 evidence record

- Date: 2026-08-21.
- Source target: `4aa373ad79a378f0f0daba3a449ff9df93752e14`, with the development-only telemetry and playtest launcher included in the Phase 3.2 publication.
- Fresh commandlet and automation: blocked before Unreal startup by macOS LaunchServices/HIServices XPC errors; no test result claimed.
- Development Editor: blocked by protected UnrealBuildTool trace/log paths and then a recursive UBA executor failure after safe path overrides.
- Mac Shipping: blocked by protected Unreal AutomationTool log/config/cache paths; no fresh app produced.
- Existing app codesign: passed, but the app is stale relative to the current source.
- Normal Metal launch: stale executable exited with abort status 134; no post-fix launch claim.
- Human interactive Run 1 and Run 2: `UNTESTED`.

## Phase 4.2 recertification — 2026-08-22

- Development Editor: **PASS** — UE 5.8 Mac arm64 build/link after the UMG, audio, material, VFX,
  terminal, vehicle-event, and validation changes.
- Fresh `AHCombatVerificationCommandlet`: **PASS** — 15 executed checks, 0 failed checks and 0
  errors, including `AshesOfHeaven.Presentation.AssetManifest`; final log
  `/tmp/ashes-phase42-commandlet-final4.log`.
- Full `Automation RunTests AshesOfHeaven`: **PASS** — 16 project tests completed with
  `Result={Success}`, exit code 0; final log `/tmp/ashes-phase42-automation-final3.log`.
- Asset generation: **PASS** — `Scripts/GeneratePhase42Assets.py` completed with no script error;
  all saved `.uasset` files are under `Content/Ashes`.
- Shipping and Development packaging, codesign, and normal Metal launch: **PASS** — final
  packages at `Builds/macOS-Development/AshesOfHeaven.app` and `Builds/macOS/AshesOfHeaven.app`;
  both deep strict codesign checks passed and both normal-renderer processes stayed alive for 15
  seconds without `-nullrhi` or `-nosound` (2026-08-22).
- Human review: **UNTESTED** — interactive UI readability, actual sound-design quality, objective
  clarity without HUD reliance, combat/death/restart/pickups/checkpoints/Manticore, final target
  visual match, localization, and mobile/Windows/iOS device behavior.

## Phase 4.3 presentation-quality recertification — 2026-08-22

The first fresh post-change Shipping smoke exposed a real Apple Metal startup failure: the process
returned 139 while `MTLCompilerService` aborted in `validateSerializedVertexDescriptor`. The Mac
platform profile now keeps the Lumen path but disables hardware ray tracing, with the guard applied
both in `DefaultDeviceProfiles.ini` and the runtime quality manager. Windows and mobile profiles are
unchanged. A fresh rebuild and recertification after that guard passed all gates below.

- Asset generation: **PASS** — `Scripts/GeneratePhase42Assets.py` completed in a fresh Unreal
  process with no Python warnings/errors, no missing WAV sources, no animation GUID ensures, and
  no MCP listener error. The only remaining generator notices are UE 5.8 MetaSound save notices
  stating that editor audio rendering is unavailable during unattended asset serialization.
  Log: `/tmp/ashes-phase43-assets-final3.log`.
- UI: **PASS foundation** — the generated reticle is authored UMG geometry (no `+`/symbol glyph
  path), damage feedback uses a rule plus text, the root is an actual `USafeZone`, and all three
  named animations contain serialized tracks and are bound to root child widgets.
- Audio routing: **PASS foundation** — combat events use five distinct project SoundWave/Cue/
  MetaSound sources; footsteps route through `SM_World`; Weapons, Ambience, Veil, Dialogue, Music,
  Vehicle, World, UI, and Master submix assets are present. The checked-in WAVs are offline-generated
  integration sources, not a claim of final recorded sound design or voice/music production.
- Materials: **PASS foundation** — Wear, Edge, Damage, Wetness, and MicroDetail parameters are
  connected into base-color, roughness, and normal branches; Niagara sprite usage is marked on the
  project material masters.
- Niagara: **PASS foundation** — project-owned emitters/systems have effect-specific sprite
  materials, facing/alignment/sort choices, deterministic seeds, fixed bounds, allocation budgets,
  persistent IDs where needed, and critical/normal importance settings.
- Environment props: **PASS ownership boundary** — runtime props load project-owned mesh assets and
  project-owned material masters. Those meshes are still replaceable starter geometry duplicated
  into the project namespace; final authored environment meshes remain an art-production task.
- Development Editor: **PASS** — UE 5.8 Mac arm64 compile/link succeeded. Log:
  `/tmp/ashes-phase43-editor-build-final4.stdout`.
- Commandlet: **PASS** — 15/15 checks, 0 failed checks, exit code 0. Log:
  `/tmp/ashes-phase43-commandlet-final4.log`.
- Automation: **PASS** — 16/16 AshesOfHeaven project tests completed with `Result={Success}`;
  `**** TEST COMPLETE. EXIT CODE: 0 ****`. Unreal's built-in `UnifiedErrorTest` emits expected
  error-log samples, and the HUD test leaves the previously known teardown warning; neither is a
  project test failure. Log: `/tmp/ashes-phase43-automation-final4.log`.
- Mac Development package: **PASS** — `BUILD SUCCESSFUL`, cook/stage/pak/archive completed in
  60.53 seconds; `Builds/macOS-Development/AshesOfHeaven.app`. Log:
  `/tmp/ashes-phase43-development-package-final5.log`.
- Mac Shipping package: **PASS** — `BUILD SUCCESSFUL`, cook/stage/pak/archive completed in 49.30
  seconds; `Builds/macOS/AshesOfHeaven.app`. Log:
  `/tmp/ashes-phase43-shipping-package-final5.log`.
- Deep strict codesign: **PASS** for both fresh packages; logs
  `/tmp/ashes-phase43-development-codesign-final2.log` and
  `/tmp/ashes-phase43-shipping-codesign-final2.log`.
- Normal renderer launch: **PASS** — both fresh packages ran for a 15-second process smoke without
  `-nullrhi` or `-nosound`, produced no crash/assertion/fatal lines, and exited with controlled
  status 0. Logs: `/tmp/ashes-phase43-development-normal-final5.log` and
  `/tmp/ashes-phase43-shipping-normal-final6.log`.
- Human validation: **UNTESTED** — no interactive playthrough is being claimed. Human review still
  specifically covers HUD readability and reference match, listening quality, objective clarity,
  combat/AI/movement/ADS/recoil, pickup/grenade/melee behavior, death/restart/checkpoint inventory
  restoration, Manticore enter/drive/fire/exit, dialogue timing, countdown, terminal progression,
  completion, final art quality, and real Windows/iOS/Android devices.

## Phase 4.4 runtime/state recertification — 2026-08-22

This is the latest evidence record for the current source. It supersedes the older Phase 4.2/4.3
machine counts above without deleting their historical records.

- Development Editor: **PASS** — UE 5.8 Mac arm64 compile/link; `/tmp/phase44-editor-build-r4.log`.
- Fresh `AHCombatVerificationCommandlet`: **PASS** — 20 checks, 0 failed checks, 0 errors;
  `/tmp/phase44-commandlet-r3.log`.
- Full `Automation RunTests AshesOfHeaven`: **PASS** — 21 project tests completed with
  `Result={Success}`, exit code 0; `/tmp/phase44-automation-r4.log`.
- Mac Development package: **PASS** — fresh `Builds/macOS-Development/AshesOfHeaven.app`, with
  automatic MCP-port selection exercised; `/tmp/phase44-development-package-script-auto.log`.
- Mac Shipping package: **PASS** — fresh `Builds/macOS/AshesOfHeaven.app`;
  `/tmp/phase44-shipping-package-final.log`.
- Deep strict codesign: **PASS** for both fresh packages; `/tmp/phase44-codesign-final2.log`.
- Normal Metal launch: **PASS** for process-level smokes on both fresh packages. They stayed alive
  for 18 seconds without `-nullrhi` or `-nosound` and exited with controlled status 0;
  `/tmp/phase44-development-normal-script-auto.log` and `/tmp/phase44-shipping-normal-final.log`.
- Fresh Development runtime integration: **PASS for smoke evidence** — normal
  `L_ChapterOne_Greybox` startup logged clean state (`OpeningBlack`, objective 0, completion false),
  authored presentation profile/material/fog/lighting/audio, 117 placed actors, and six VFX systems.
  No Phase 4.4 runtime errors remained after rebuilding; `/tmp/phase44-development-normal-script-auto.log`.
- Screenshot capture: **UNAVAILABLE** — window capture hung and full-screen capture returned
  `could not create image from display`; no visual approval is claimed.
- Interactive gameplay, combat/death/restart/pickups/checkpoint progression, Manticore behavior,
  Chapter completion, subjective HUD/audio/art review, and target-device performance: **UNTESTED**.

The connected Unreal MCP editor service owns port 8000. `Scripts/Build-Mac.sh` now selects a free
isolated MCP port for cook commandlets when necessary; this is packaging-environment handling and
does not change the game's runtime architecture.
