# ASHES OF HEAVEN implementation status

## PHASE 3 — CHAPTER ONE GREYBOX — 2026-08-21

Chapter One is implemented as a playable greybox slice on top of the Phase 2 combat foundation. The chapter uses a persistent `GameInstance` state, ordered mission stages, dialogue sequences, checkpoint serialization, encounter persistence, a Manticore vehicle, a Veil Warden, terminal interaction, countdown state, debug routing, and a chapter-complete state.

No final art, animation, lighting, sound, or Chapter Two content was added.

### Chapter path

The runtime stage graph contains all required story beats, in order:

1. `OpeningBlack` — black opening and opening dialogue.
2. `ErebusOpening` — reach the defensive line.
3. `OpeningBattle` — hold the Erebus line.
4. `TransitStation` — enter the transit station.
5. `VeilRevelation` — Veil says “You came back.”
6. `OpenBattlefield` — cross the large battlefield.
7. `ManticoreSection` — enter and operate the Manticore.
8. `CathedralApproach` — reach the Cathedral approach.
9. `FailsafeOrder` — receive the order to destroy Erebus; the 08:42 countdown begins.
10. `CathedralInterior` — enter the Cathedral and meet Sael.
11. `SaelTransmission` — Sael’s transmission is presented.
12. `FailsafeTerminal` — inspect and confirm the terminal showing `11,407,231` projected civilian casualties.
13. `Escape` — escape the Cathedral.
14. `OtherLucian` — encounter the other Lucian.
15. `ErebusDestruction` — destruction sequence and transition.
16. `TenYearsLater` — ten-year transition.
17. `MayaScene` — Maya scene.
18. `NysaTransmission` — Nysa transmission.
19. `FleetDeparture` — fleet departure.
20. `StarsDisappearing` — disappearing stars.
21. `ChapterComplete` — `ASHES OF HEAVEN / CHAPTER ONE COMPLETE`.

The authored objective chain contains 17 objectives, from reaching the defensive line through the title reveal. Narrative-only stages remain in the stage graph without creating duplicate objective entries.

### Slice expectations

- Target playtime: approximately 20–30 minutes for a first complete run, depending on combat pace and dialogue timing. This is a design target, not an automated timing result.
- Systems included: Phase 2 FPS movement and combat, rifle/ADS/reload, grenades, pickups, melee, Veil enemies, human friendlies, objectives, checkpoints, death/restart, dialogue/subtitles, terminal confirmation, countdown, Manticore driving and mounted weapon, barricade destruction, Veil Warden navigation/fallback movement, debug chapter routing, and save-state restoration.
- Greybox presentation is intentionally functional. Runtime-generated blockout geometry and labels stand in for final level art.

## Verification results

### Fresh commandlet

The previous editor/hot-reload session was not reused. A fresh Unreal process ran:

```bash
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  AshesOfHeaven.uproject \
  -run=AHCombatVerificationCommandlet -unattended -nop4 -nosplash \
  -nullrhi -nosound -stdout -FullStdOutLogOutput
```

Result: **PASS** — 13 project checks, 0 failed checks, and `Success - 0 error(s), 0 warning(s)` in `Saved/Logs/Phase3-AHCombatVerificationCommandlet.log`.

The result includes the nine Phase 2 combat/acceptance checks plus:

- `AshesOfHeaven.Chapter.StageOrdering`
- `AshesOfHeaven.Chapter.ObjectiveChain`
- `AshesOfHeaven.Chapter.StateSerialization`
- `AshesOfHeaven.Chapter.CountdownAndNarrativeState`

### Unreal automation

The full project automation filter was run in a fresh commandlet process:

```bash
"$UE_ROOT/Engine/Binaries/Mac/UnrealEditor-Cmd" \
  AshesOfHeaven.uproject \
  -ExecCmds="Automation RunTests AshesOfHeaven;Quit" \
  -TestExit="Automation Test Queue Empty" -unattended -nop4 -nosplash \
  -nullrhi -nosound -stdout -FullStdOutLogOutput
```

Result: **PASS** — 13 tests found, all 13 completed with `Result={Success}`, and `**** TEST COMPLETE. EXIT CODE: 0 ****`. The captured run is `/tmp/ashes-phase3-automation.log` on the validation machine.

### Build and package

`./Scripts/Build-Mac.sh` passed on 2026-08-21. Development Editor and Mac Shipping compiled; cook, stage, pak, archive, and local codesign completed successfully. The packaged application is:

`Builds/macOS/AshesOfHeaven.app`

The final archive has package version counter `0.12`. `codesign --verify --deep --strict --verbose=2` passed with `valid on disk` and `satisfies its Designated Requirement`.

The final packaged executable was launched without `-nullrhi`:

```bash
./Builds/macOS/AshesOfHeaven.app/Contents/MacOS/AshesOfHeaven-Mac-Shipping \
  -windowed -ResX=1024 -ResY=576 -nosound
```

Result: **PASS for process-level normal-renderer launch** — the process remained alive for approximately 20 seconds and was stopped with a controlled interrupt. This is a launch smoke check only; it is not a human playthrough or a claim that the slice was interactively completed.

### Static/runtime guard coverage

- The stage graph and objective tests cover the complete Chapter One path and reachable completion state.
- Encounter activation prunes invalid, destroyed, and dead actors; friendlies are not encounter enemies.
- Checkpoint state includes chapter stage, objective index, narrative/section/encounter history, countdown/failsafe flags, player inventory, and Manticore state.
- Encounter and pickup ownership is director-scoped; restarting reloads the world rather than duplicating the prior world’s actors.
- Ammo and grenade state are serialized through the existing checkpoint/player save path.
- Veil Warden movement uses a navigation query and a bounded direct fallback when projection is unavailable.
- The terminal interaction is staged as inspect then confirm, and the completion route advances through escape, destruction, the ten-year transition, Maya, Nysa, the fleet, disappearing stars, and the chapter title.

### Phase 3.1 — objective HUD delegate fix

The first fresh-runtime regression pass found a real defect in the objective presentation boundary: `AAHCombatPlayerController::HandleObjectiveChanged` and `HandleMissionComplete` were used as dynamic delegate targets without `UFUNCTION()` reflection. The handlers are now reflected, objective binding is isolated in `BindObjectiveEvents`, and the HUD exposes read-only presentation state for verification.

Regression coverage now exercises the live controller/HUD path, dynamic objective and mission-complete delegate targets, one-step `ObjectiveDebug` progression, objective text/index synchronization, and the reachable HUD completion state. All other `AddDynamic` targets in `Source/AshesOfHeaven` were audited and have reflected handlers.

UHT regeneration succeeded for `AshesOfHeavenEditor` and wrote the new delegate receiver reflection output. The changed Development Editor objects compiled successfully with the generated Unreal response files and linked successfully into the editor module.

The post-fix commandlet and automation rerun are **BLOCKED in the current execution environment**, not marked as passing: a fresh `UnrealEditor-Cmd` process emitted only macOS LaunchServices connection errors and did not reach Unreal startup logging before the bounded run was stopped. The standard UnrealBuildTool build path is separately blocked before compilation because this environment denies rotation of the external UnrealBuildTool trace-backup file under `~/Library/Application Support/Epic/UnrealBuildTool`. Therefore the earlier 13-check/13-test Phase 3 results above remain historical pre-fix evidence; no 14-check/14-test result is claimed here.

Development Editor rebuild through `Build.sh`, Mac Shipping cook/package, and normal-renderer packaged launch were not re-certified after this source change because of that environment block. The previous successful package and normal-renderer process smoke result remains recorded above, but it is not presented as validation of this unbuilt post-fix revision.

### Phase 3.2 — post-fix recertification record — 2026-08-21

The expected post-fix source is `4aa373ad79a378f0f0daba3a449ff9df93752e14` (`fix(combat): reflect objective HUD handlers`). The working tree adds development-only `[Phase3.2]` telemetry and `Scripts/Run-Mac-Playtest.sh`; no gameplay feature or Phase 4/Chapter Two content was added.

Static inventory confirms 14 `AHCombatVerificationCommandlet::BeginTest` checks and 14 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` registrations, including `AshesOfHeaven.Chapter.ObjectiveHUDDelegate`. This is an inventory check only; it is not a substitute for executing the tests.

#### Fresh verification attempts

- **UHT: PASS.** `AshesOfHeavenEditor` processed successfully for Mac arm64, with 0 generated files written. Log: `/tmp/ashes-phase32-uht-r2.log`.
- **Commandlet: BLOCKED BY EXECUTION ENVIRONMENT.** A fresh `UnrealEditor-Cmd` invocation was bounded at 180 seconds. It emitted only macOS LaunchServices/HIServices XPC connection errors, produced no Unreal startup or test markers, and was stopped without a result. Log: `/tmp/ashes-phase32-commandlet.log`.
- **Automation: BLOCKED BY EXECUTION ENVIRONMENT.** A fresh `UnrealEditor-Cmd` automation invocation was bounded at 180 seconds and exited via the timeout signal (`142`). It emitted the same LaunchServices/HIServices errors and executed no discovered tests. Log: `/tmp/ashes-phase32-automation.log`.
- **Process cleanup: NOT VERIFIABLE.** `ps` and `pgrep` were denied by the managed macOS execution environment. No broad process kill was performed.

#### Build and package attempts

- **Development Editor: BLOCKED BY EXECUTION ENVIRONMENT/TOOLCHAIN.** The normal build first failed while rotating `Trace-backup-*.uba` under `~/Library/Application Support/Epic/UnrealBuildTool`. A safe session/log override reached real Clang compilation, then UE 5.8 UBA stopped with `UBA executor is not expected to be invoked from a recursive UBT call`. Log: `/tmp/ashes-phase32-editor-build-r3.log`.
- **Changed-object compile fallback: PASS, 11/11.** The 11 touched gameplay translation units compiled with the existing Unreal response files and Apple Clang. This is useful source-level evidence but is not a Development Editor build.
- **Mac Shipping cook/package: BLOCKED BY EXECUTION ENVIRONMENT.** The canonical script was blocked deleting/creating external Unreal AutomationTool log/config paths under `~/Library/Logs/Unreal Engine` and `~/Library/Application Support/Epic/UnrealEngine`. Redirecting UAT logs still hit the protected `XmlConfigCache-...bin` path. No fresh post-fix package was produced.
- **Codesign: PASS for the existing app only.** `codesign --verify --deep --strict --verbose=2 Builds/macOS/AshesOfHeaven.app` passed, but that app predates the current source changes and is therefore not fresh post-fix evidence.
- **Normal Metal launch: NOT CERTIFIED.** The stale existing executable was tried directly without `-nullrhi`; it exited immediately with abort status 134 and no stdout/log output in this environment. Because no fresh package could be produced, no post-fix launch result is claimed.

#### Playtest observability and remaining human work

Development builds now emit bounded `[Phase3.2]` logs for objective transitions, stage/dialogue/countdown transitions, checkpoints, inventory/ammo/grenades, encounters, Manticore state, death/restart, and completion. `Scripts/Run-Mac-Playtest.sh` launches the packaged app in a normal window without Null RHI and writes a timestamped log under `Saved/PlaytestLogs`.

Codex did not fake interactive validation. Human Run 1 (normal completion) and Run 2 (inventory → checkpoint → death/restart → encounters → Manticore → completion) remain **UNTESTED**. Phase 4 and Chapter Two remain explicitly out of scope until those two human runs are recorded on a freshly packaged build.

## Known issues and scope boundaries

- Geometry, animation, lighting, sound, and VFX are greybox/prototype quality by design.
- The current Chapter One map is one logical map with runtime-built sections; the stage/state architecture is ready for future World Partition or level-streaming splits.
- The authored map does not yet contain a hand-authored navigation mesh asset. The runtime nav bounds plus Warden query/fallback keep the prototype path valid.
- Unreal may report unavailable Win64, Android, or Linux SDK warnings on this Mac host; Mac validation succeeded.
- Windows, Android, iOS, controller, touch, suspend/resume, performance/thermal, and signed-device validation require their respective toolchains or hardware.

## Human validation still required

Codex did not claim a human interactive playthrough. A human tester must run the packaged app twice and record the result:

1. Complete the slice normally, judging controls, gun feel, AI, progression, checkpoints, Manticore operation, dialogue, and whether anything softlocks.
2. Collect ammo and grenades, reach checkpoint 2 or 3, die, respawn, verify inventory/objectives/enemies, and finish the mission.

The human tester should ignore ugly geometry, placeholder animation, lighting, and sound when judging Phase 3 acceptance. These interactive combat/death/restart/pickup/Manticore/checkpoint behaviors remain `UNTESTED` in the platform matrix until that playthrough is performed.

### Validation environment note — 2026-08-21

An interactive launch attempt was made with the packaged Shipping app through macOS LaunchServices. The process created an 800×632 game window, but it initially opened on an offscreen display. After moving it to the primary display, CoreGraphics confirmed the window while macOS screen capture still returned `could not create image from display`. Without an observable viewport, no honest gameplay input/feel/objective/completion result can be recorded from this environment. The app was stopped cleanly; no crash or reproducible gameplay defect was observed.

The later Phase 3.1 commandlet attempt was also stopped without a result after the macOS service startup failure described above. This is an execution-environment limitation, not interactive gameplay evidence.

## Next

Human playthrough and platform evidence are the remaining Phase 3 acceptance work. Do not begin Chapter Two or Phase 4 until that evidence is recorded.
