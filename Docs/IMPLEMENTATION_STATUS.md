# ASHES OF HEAVEN implementation status

## PHASE 3 — CHAPTER ONE GREYBOX — 2026-08-21

Chapter One is implemented as a playable greybox slice on top of the Phase 2 combat foundation. The chapter uses a persistent `GameInstance` state, ordered mission stages, dialogue sequences, checkpoint serialization, encounter persistence, a Manticore vehicle, terminal interaction, countdown state, debug routing, and a chapter-complete state.

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

> **Superseded by Level One: FOR A WHILE.** Level One now has **12** objectives and ends at `ErebusDestruction`; the `TenYearsLater` through `StarsDisappearing` stages are retained only for save compatibility and are not reachable. The stage list and objective count in this Phase 3 record are the historical greybox shape. See `Docs/LEVEL1_FOR_A_WHILE.md` for the current contract.

### Slice expectations

- Target playtime: approximately 20–30 minutes for a first complete run, depending on combat pace and dialogue timing. This is a design target, not an automated timing result.
- Systems included: Phase 2 FPS movement and combat, rifle/ADS/reload, grenades, pickups, melee, Veil enemies, human friendlies, objectives, checkpoints, death/restart, dialogue/subtitles, terminal confirmation, countdown, Manticore driving and mounted weapon, barricade destruction, debug chapter routing, and save-state restoration.
- Greybox presentation is intentionally functional. Runtime-generated blockout geometry and labels stand in for final level art.

## Verification results

> **Superseded.** The 13-check/13-test results in this section are the original Phase 3
> record and are kept as history. The authoritative current results are in
> [Phase 3.3](#phase-33--machine-recertification--2026-08-21), which recorded 14/14 on both
> suites against a freshly packaged build.

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
- The terminal interaction is staged as inspect then confirm, and the completion route advances through escape, destruction, the ten-year transition, Maya, Nysa, the fleet, disappearing stars, and the chapter title.

### Phase 3.1 — objective HUD delegate fix

The first fresh-runtime regression pass found a real defect in the objective presentation boundary: `AAHCombatPlayerController::HandleObjectiveChanged` and `HandleMissionComplete` were used as dynamic delegate targets without `UFUNCTION()` reflection. The handlers are now reflected, objective binding is isolated in `BindObjectiveEvents`, and the HUD exposes read-only presentation state for verification.

Regression coverage now exercises the live controller/HUD path, dynamic objective and mission-complete delegate targets, one-step `ObjectiveDebug` progression, objective text/index synchronization, and the reachable HUD completion state. All other `AddDynamic` targets in `Source/AshesOfHeaven` were audited and have reflected handlers.

UHT regeneration succeeded for `AshesOfHeavenEditor` and wrote the new delegate receiver reflection output. The changed Development Editor objects compiled successfully with the generated Unreal response files and linked successfully into the editor module.

**Superseded by Phase 3.3.** The blockers described in the rest of this section were specific
to the execution environment in use at the time; none of them reproduced on the normal macOS
session, where every command below ran to completion. Retained as history.

The post-fix commandlet and automation rerun were **BLOCKED in that execution environment**, not marked as passing: a fresh `UnrealEditor-Cmd` process emitted only macOS LaunchServices connection errors and did not reach Unreal startup logging before the bounded run was stopped. The standard UnrealBuildTool build path is separately blocked before compilation because this environment denies rotation of the external UnrealBuildTool trace-backup file under `~/Library/Application Support/Epic/UnrealBuildTool`. Therefore the earlier 13-check/13-test Phase 3 results above remain historical pre-fix evidence; no 14-check/14-test result is claimed here.

Development Editor rebuild through `Build.sh`, Mac Shipping cook/package, and normal-renderer packaged launch were not re-certified after this source change because of that environment block. The previous successful package and normal-renderer process smoke result remains recorded above, but it is not presented as validation of this unbuilt post-fix revision.

### Phase 3.2 — post-fix recertification record — 2026-08-21

**Superseded by Phase 3.3.** Every item recorded below as BLOCKED or NOT CERTIFIED has since
been executed successfully from a normal macOS session. The expected source recorded here,
`4aa373ad`, is no longer the current target. Retained as history.

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

### Phase 3.3 — machine recertification — 2026-08-21

Source: `f5499f72b773049d6da04e1e9a8df20d91880e87` on `main`. Every command below was run from a
normal macOS session against UE 5.8 at `/Users/Shared/Epic Games/UE_5.8`, Xcode 26.1.1. None of the
execution-environment blockers recorded in Phase 3.1 and 3.2 reproduced.

#### Results

| Gate | Result |
| --- | --- |
| `./Scripts/Build-Mac.sh` | **PASS** — `BUILD SUCCESSFUL`, ExitCode=0 |
| `AHCombatVerificationCommandlet` | **PASS** — 14 executed, 14 PASS, 0 FAIL |
| `Automation RunTests AshesOfHeaven` | **PASS** — 14 started, 14 completed, 14 `Result={Success}`, 0 failures, `TEST COMPLETE. EXIT CODE: 0` |
| Fresh Mac Shipping package | **PASS** — `Builds/macOS/AshesOfHeaven.app`, package version counter `0.27` |
| `codesign --verify --deep --strict` | **PASS** — `valid on disk`, `satisfies its Designated Requirement` |
| Development playtest package | **PASS** — `Builds/macOS-Development/AshesOfHeaven.app` |
| `./Scripts/Run-Mac-Playtest.sh` | **PASS** — launches a normal-renderer window, loads `L_ChapterOne_Greybox`, writes the playtest log |

The 14 results are executed test results, not a static inventory. Both suites are counted from
per-test markers: `: PASS` lines for the commandlet, `Test Completed. Result={Success}` for
automation.

#### Defects found and fixed during recertification

The gates did not pass as they stood. Seven defects were found and fixed:

1. **Cook failed on a full disk.** The first packaging run died `ExitCode=25` with roughly 300
   `Zen: Insufficient Storage (507)` errors at 1.7 GiB free. Not a code defect; recorded because it
   presents as an opaque cook failure.
2. **The package shipped stale cooked content.** UAT's `-archive` copies
   `Binaries/Mac/<Target>.app`, whose `Contents/UE` can still hold cooked data from an earlier run,
   and `Build-Mac.sh` used `ditto`, which merges rather than replaces. A freshly built executable was
   being bonded to paks from a previous build, and the stale files survived every rebuild. The script
   now sources the app from `Saved/StagedBuilds` and replaces the destination.
3. **A Shipping package cannot write a playtest log.** The Phase 3.2 telemetry is guarded by
   `#if !UE_BUILD_SHIPPING`, and an installed engine ships Core with `NO_LOGGING` for Shipping, so
   there is no log sink. `bUseLoggingInShipping` is not a way out: UBT rejects it on a shared build
   environment, and forcing it with `bOverrideBuildEnvironment` recompiles only the project module
   while Core stays prebuilt. `Build-Mac.sh` now takes `CLIENT_CONFIG` (default `Shipping`) and the
   playtest launcher defaults to the Development package. A packaged build also ignores both `-log`
   and `-abslog` and emits everything on stdout, so the launcher tees the stream.
4. **The greybox rendered black.** The map is generated empty and the director builds all geometry
   at runtime, but nothing created a light. With static lighting off and Lumen on, a scene with no
   lights renders black. The director now spawns a directional sun, a sky atmosphere and a
   real-time-capture sky light.
5. **The player fell out of the level.** Every ground-level actor sits at Z≈120, but the floor
   spanned X 5000..24000 while the player start is at X=-1400. The player spawned over open space and
   fell, and because falling through empty sky looks identical to standing still it read as frozen
   input. The floor now covers X -2000..31000 and Y ±2000.
6. **The weapon was unusable.** `WeaponMesh` was never tagged as a first-person primitive, so it drew
   at world scale and filled the view; `SKM_Rifle` is authored along Y and the hand socket used to
   supply the quarter turn that aims it down X; and `FireShot` replaced the relative rotation with a
   bare kick rotator, snapping the weapon side-on with every shot.
7. **Soldiers were invisible while their weapons were not.** Combatants have no skeletal mesh, so a
   soldier rendered as nothing while the weapon attached to them still drew. Meshless combatants now
   get a block body.

Two log-quality defects were fixed alongside these: roughly 42,000 `GetSocketInfoByName` warnings per
run, from attachments to sockets on a meshless component re-resolving every frame, and a navmesh
rebuilt through the navigation system's recovery path on every launch. A 60-second run now produces a
101 KB log with 0 errors, against 11 MB before.

#### Playtest observability

`Scripts/Run-Mac-Playtest.sh` writes a timestamped log under `Saved/PlaytestLogs` containing the
`[Phase3.2]` markers. The chapter-stage marker now also records the player's world location, without
which a fall out of the level is invisible in a playtest log.

#### Remaining human work

Human Run 1 (normal completion) and Run 2 (inventory → checkpoint → death/restart → encounters →
Manticore → completion) remain **UNTESTED**. No interactive playthrough is claimed. Phase 4 and
Chapter Two remain out of scope until both runs are recorded against a freshly packaged build.

## PHASE 4 — APPROVED VISUAL TARGET IMPLEMENTATION — 2026-08-21

Phase 4 implements four real runtime art-target areas on top of the Phase 3 gameplay layout. It does not begin Chapter Two, rewrite the gameplay framework, or mark the Phase 3 human playthrough as complete.

### Approved references found

All four approved reference files are present under `References/ArtTargets/` and are recorded in [ART_TARGETS.md](ART_TARGETS.md). They were inspected before implementation. They remain reference images; no offline render or generated image is presented as gameplay evidence.

### In-engine implementation

- **Erebus:** layered fortification, defensive wall, wreck/pipe scaffolding, distant ruin silhouettes, warm fire/emergency lights, cold daylight, fog, and low-cost dust. The Cathedral remains a large distant landmark.
- **Transit:** platform/rail/door-frame language, overhead service pipes, benches, cases, control equipment, authored `NORTH LINE / PLATFORM 02` and `CIVIL DEFENSE / EVACUATION ROUTE` signage, amber/red practicals, dust, and an environment-led route frame.
- **Cathedral:** monolithic fins, nested void/frame vocabulary, suspended masses, a human expedition walkway and equipment for scale, cold light/fog, and original procedural glyph families.
- **Present-day Lucian/Maya:** separated industrial composition with table/platform, cold key, restrained warm practical, dark negative space, and legally present UE mannequin display proxies. Final character work is explicitly not claimed.
- **M91:** reduced first-person occupancy, stable camera hold/recoil frame, first-person primitive tagging, a restrained capacitor/power-architecture scaffold, and dark first-person gauntlet proxies.
- **Human/Veil:** faction-specific temporary materials, a visible human body proxy, and a matte Veil silhouette. Final authored soldier meshes/animation remain open.
- **HUD:** scalable panels and safe margins, legible 1280×720 baseline, objective update prominence/settle behavior, health/armor/ammo/reserve/grenades, interaction, hit/damage feedback, countdown, vehicle state, and completion presentation.

### Architecture and safety

Art-target shapes and display characters are non-colliding and do not affect navigation. The Phase 3 gameplay floor, cover, triggers, checkpoints, encounter actors, Manticore route, and saved nav architecture remain authoritative. Runtime preview is selectable with `Scripts/Run-Mac-ArtTarget.sh` and `-ArtTarget=Erebus|Transit|Cathedral|LucianMaya|M91`.

Material mappings, glyph rules, palette, lighting, VFX limits, provenance, performance budgets, and external-art gaps are canonicalized in [ART_DIRECTION.md](ART_DIRECTION.md). Target-by-target comparison and human review gates are in [ART_TARGETS.md](ART_TARGETS.md).

### Phase 4 validation state

The Phase 4 source and runtime target layer completed a fresh machine validation pass on 2026-08-21 from the current working tree. These are machine/build gates only; they do not claim subjective visual approval or a human playthrough.

#### Fresh machine results — 2026-08-21

| Gate | Exact result |
| --- | --- |
| Development Editor | **PASS** — `RunUBT.sh AshesOfHeavenEditor Mac Development -Architecture=arm64 ...`; exit 0, UHT/compile/link succeeded. Log: `/tmp/ashes-phase4-editor-build-r6.log` |
| `AHCombatVerificationCommandlet` | **PASS** — 14 checks executed, 0 failed checks; `AshesOfHeaven combat commandlet: 14 tests, 0 failed checks, PASS`; 0 errors and 1 known HUD-test cleanup warning. Log: `/tmp/ashes-phase4-commandlet-r3.log` |
| `Automation RunTests AshesOfHeaven` | **PASS** — 15 tests discovered, 15 started, 15 completed with `Result={Success}`, `**** TEST COMPLETE. EXIT CODE: 0 ****`; art manifest included. Log: `/tmp/ashes-phase4-automation-r2.log` |
| Development cook/package | **PASS** — `BuildCookRun time: 60.05 s`, `BUILD SUCCESSFUL`, ExitCode=0; `Builds/macOS-Development/AshesOfHeaven.app`. Log: `/tmp/ashes-phase4-development-package-r2.log` |
| Development codesign | **PASS** — `codesign --verify --deep --strict --verbose=2`; valid on disk and designated requirement satisfied. |
| Development normal renderer | **PASS** — direct packaged executable stayed alive for 15 seconds with Metal/no `-nullrhi`, reached `L_ChapterOne_Greybox`, then was stopped with a controlled termination. Log: `/tmp/ashes-phase4-development-launch-r2.log` |
| Art-target activation | **PASS** — fresh Development normal-renderer smokes for `Erebus`, `Transit`, `Cathedral`, `LucianMaya`, and `M91` each stayed alive for 8 seconds, logged the matching `[Phase4][ArtTarget] activated=` marker, and showed no fatal/assertion/SIG marker. Logs: `/tmp/ashes-phase4-arttarget-{Erebus,Transit,Cathedral,LucianMaya,M91}-r2.log` |
| Mac Shipping cook/package | **PASS** — `BuildCookRun time: 51.10 s`, `BUILD SUCCESSFUL`, ExitCode=0; package counter `0.30`; `Builds/macOS/AshesOfHeaven.app`. Log: `/tmp/ashes-phase4-shipping-package-r2.log` |
| Mac Shipping codesign | **PASS** — `codesign --verify --deep --strict --verbose=2`; valid on disk and designated requirement satisfied. |
| Mac Shipping normal renderer | **PASS** — `AshesOfHeaven-Mac-Shipping` stayed alive for 15 seconds with Metal/no `-nullrhi`, then was stopped with a controlled termination. No new crash report was produced. |

No actual runtime screenshots were captured in this execution environment. The stable viewpoints are real in-engine positions and can be reviewed with `Scripts/Run-Mac-ArtTarget.sh`; no offline render or fabricated screenshot is claimed. Real device FPS, draw-call, memory, thermal, and mobile measurements were not performed. Subjective target match, final art approval, and the two Phase 3 interactive human runs remain human-gated.

## PHASE 4.1 — AUDIO AND REFERENCE-DRIVEN TACTICAL UI — 2026-08-21 (superseded by Phase 4.2)

The first Phase 4 pass exposed two concrete completeness gaps: the gameplay event sound properties
were mostly unassigned, and the HUD still read as a generic greybox overlay rather than belonging to
the approved industrial/Veil visual language. Phase 4.1 closes the runtime feedback gap and replaces
the HUD treatment without changing the chapter graph, combat rules, collision, navigation, or
cross-platform architecture.

### Audio implementation

- Added `UAHAudioSubsystem` in `Source/AshesOfHeaven/Gameplay/Audio/` as a world-owned runtime audio
  palette. It initializes on every playable world and is safe in packaged builds.
- The rifle uses the packaged project asset
  `/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02` for its default shot sound.
- Null or unassigned Blueprint sound slots now fall back to generated PCM cues for shot, reload, dry
  fire, impact, melee, armor, hurt, death, grenade, pickup, footstep, objective, dialogue, and a low mechanical
  ambient bed. Existing authored assets continue to win whenever a slot is assigned.
- Audio hooks are attached at the actual gameplay events: weapon fire/reload/empty/impact, combatant
  damage/death, melee, grenade explosion, objective changes, dialogue lines, and world start.
- The generated palette is an intentional Phase 4.1 integration layer, not a claim of final sound
  design. Authored weapon layers, footsteps, enemy vocalizations, environmental ambience, music,
  voice acting, mix, and platform-specific loudness still require production audio work.

### HUD implementation

- Replaced the old blue-grey block HUD with a dark near-black tactical treatment: thin bracket frames,
  steel/bone typography, restrained amber objective accents, cyan armor, red danger states, and sparse
  lines rather than large opaque panels.
- The objective strip now reads as `MISSION // OBJECTIVE`, shows progress, uses a short amber settle
  pulse, and remains readable at a 1280x720 baseline with safe margins.
- Health/armor, vitals, magazine/reserve, grenades, damage direction, hit markers, interaction,
  countdown, Manticore hull/speed, terminal intel, dialogue, and chapter completion all use the same
  tactical frame language. The magazine value is intentionally dominant and the weapon name/status
  hierarchy is secondary.
- HUD sizing is based on the smaller of width/height scale, clamped for 1280x720 through 2560x1440,
  so text does not collapse into the old tiny corner labels on the baseline target.
- The environment references remain the art direction source: Erebus uses war-worn steel/fire,
  Transit uses amber/red infrastructure, Cathedral uses black mass/cold light, and Lucian/Maya uses
  severe cinematic negative space. The HUD translates that shared language; the PNGs are not literal
  HUD layout sheets.

### Phase 4.1 verification — 2026-08-21

| Gate | Exact result |
| --- | --- |
| Development Editor | **PASS** — `AshesOfHeavenEditor Mac Development`, UE 5.8 Mac arm64 UHT/compile/link, exit 0; audio subsystem, pickup/footstep hooks, and HUD compiled. |
| `AHCombatVerificationCommandlet` | **PASS** — 14/14 checks, 0 failed checks, 0 errors; one known HUD-test cleanup warning; fresh Editor module logged `cues=14`. Log: `/tmp/ashes-phase41-commandlet-final-52678.log`. |
| `Automation RunTests AshesOfHeaven` | **PASS** — 15/15 tests completed with `Result={Success}`, exit code 0; fresh Editor module logged `cues=14`. Log: `/tmp/ashes-phase41-automation-final-52743.log`. |
| Mac Development cook/package | **PASS** — `BUILD SUCCESSFUL`, `BuildCookRun time: 60.38 s`, exit code 0; `Builds/macOS-Development/AshesOfHeaven.app`. |
| Mac Shipping cook/package | **PASS** — `BUILD SUCCESSFUL`, `BuildCookRun time: 47.55 s`, exit code 0; `Builds/macOS/AshesOfHeaven.app`. |
| Development normal renderer + audio | **PASS** — final packaged process stayed alive for 12 seconds without `-nullrhi` or `-nosound`; CoreAudio created the 48 kHz mixer, the 14-cue runtime palette initialized, and the ambient bed started. Captured output: `/tmp/ashes-phase41-development-final-audio-53794.stdout`. |
| Shipping normal renderer | **PASS** — final packaged `AshesOfHeaven-Mac-Shipping` stayed alive for 15 seconds without `-nullrhi` or `-nosound`; no crash occurred. Captured stdout was empty, as expected for this Shipping target: `/tmp/ashes-phase41-shipping-final-audio-53870.stdout`. |

These are machine/build and runtime initialization results. They do not claim a human playthrough,
subjective reference match, final authored audio, or final art approval.

## Known issues and scope boundaries

- Phase 4.1 now supplies a runtime audio integration layer and reference-driven tactical HUD, but
  authored environment meshes, production materials, final VFX/audio, and production character art
  remain open gaps. See `Docs/ART_TARGETS.md`; visual match is not claimed without human review.
- The presentation generator now saves project-authored Niagara emitter/system pairs, including
  `NS_AshField`, `NS_EmberDrift`, `NS_ImpactSparks`, `NS_FireSmall`, `NS_FireLarge`,
  `NS_SmokeColumn`, `NS_DustSheet`, and `NS_CathedralMotes`. These are integration-quality VFX
  assets, not a claim of final authored fire, smoke, embers, or Cathedral atmosphere.
- Combatants still use temporary faction-aware body proxies unless production meshes are supplied.
- Lucian and Maya are interim UE mannequin display proxies, not final character art and not a
  likeness claim.
- The current Chapter One map is one logical map with runtime-built sections; the stage/state architecture is ready for future World Partition or level-streaming splits.
- The authored map now carries saved navigation data: a `NavMeshBoundsVolume` and a
  `RecastNavMesh-Default` set to `RuntimeGeneration = Dynamic`. Dynamic is required rather than
  cosmetic, because the greybox geometry is spawned at runtime and the navmesh genuinely has to
  rebuild then; what changed is that this is now the configured path instead of the navigation
  system's recovery path, which previously logged a warning on every launch.
- Unreal may report unavailable Win64, Android, or Linux SDK warnings on this Mac host; Mac validation succeeded.
- Windows, Android, iOS, controller, touch, suspend/resume, performance/thermal, and signed-device validation require their respective toolchains or hardware.

## Human validation still required

Codex did not claim a human interactive playthrough. A human tester must run the packaged app twice and record the result:

1. Complete the slice normally, judging controls, gun feel, AI, progression, checkpoints, Manticore operation, dialogue, and whether anything softlocks.
2. Collect ammo and grenades, reach checkpoint 2 or 3, die, respawn, verify inventory/objectives/enemies, and finish the mission.

The human tester should ignore remaining prototype geometry, animation, lighting, and the Phase 4.1
placeholder audio palette when judging Phase 3 acceptance. These interactive combat/death/restart/
pickup/Manticore/checkpoint behaviors remain `UNTESTED` in the platform matrix until that playthrough
is performed.

### Validation environment note — 2026-08-21

An interactive launch attempt was made with the packaged Shipping app through macOS LaunchServices. The process created an 800×632 game window, but it initially opened on an offscreen display. After moving it to the primary display, CoreGraphics confirmed the window while macOS screen capture still returned `could not create image from display`. Without an observable viewport, no honest gameplay input/feel/objective/completion result can be recorded from that environment. The later direct normal-renderer machine smokes above prove launch only, not gameplay.

The later Phase 3.1 commandlet attempt was also stopped without a result after the macOS service startup failure described above. This is an execution-environment limitation, not interactive gameplay evidence.

## Next

Phase 4 stops at the four representative visual targets. Do not begin Chapter Two or propagate the
target language across the whole 20–30 minute chapter until the user has reviewed the actual
in-engine viewpoints. The remaining work is human visual approval, external environment/material/
character/VFX/audio authoring, real target-device profiling, and the previously recorded Phase 3
interactive Run 1/Run 2 acceptance.

## PHASE 4.2 — Unreal-native presentation recertification — 2026-08-22

Phase 4.2 is implemented as a presentation pipeline only. Gameplay systems, the Chapter One
stage/objective graph, checkpoints, encounter logic, and navigation architecture were preserved.
Chapter Two/Phase 5 was not started.

### Implemented

- Production HUD now enters through `AAHCombatHUD` but renders through saved UMG
  `/Game/Ashes/UI/HUD/WBP_HUD_Root`; the old Canvas `DrawHUD` path is an empty compatibility
  override, not the production presentation path.
- `UAHHUDRootWidget` consumes a saved authored UMG hierarchy through safe-zone-aware anchors and binds event-driven objective,
  health, armor, ammo, grenades, interaction, dialogue, countdown, damage, completion, and
  Manticore state. It does not poll gameplay in Tick.
- Saved UMG assets own real child hierarchies for root, objective, player status, weapon status, crosshair,
  interaction, damage, countdown, dialogue, terminal intel, Manticore, chapter title, and terminal
  world presentation.
- `UAHAudioPaletteData` and `UAHAudioSettings` provide semantic audio resolution. Imported project
  WAVs, graph-based SoundCues, serialized MetaSound sources, attenuation, concurrency, and world/UI
  submix assets are saved below `Content/Ashes/Audio`; the default palette resolves MetaSounds.
- Runtime PCM generation and engine template substitution are removed. Missing authored events log
  and skip instead of synthesizing a tone.
- The M91 resolves the project-local `MS_M91_Fire` source and no longer loads the grenade-launcher
  template family.
- Stage changes cross-fade the authored Erebus, Transit, Cathedral, and Manticore environment
  events. Development debug commands cover the requested UI/audio/material paths.
- Saved material masters/instances, authored Niagara emitter/system pairs, reusable Blueprint
  presentation props, and presentation data assets exist under `Content/Ashes`. Materials contain
  wear/grime/edge/damage/wetness/microdetail branches; VFX no longer duplicates engine templates.
  The director consumes project assets first with explicit greybox fallback paths.
- The terminal owns a world-space `WBP_TerminalWorld` widget component. Manticore and countdown
  publish low-rate/event-driven presentation updates.

### Machine verification — fresh Unreal processes

Commands were run against UE 5.8.1 at `/Users/Shared/Epic Games/UE_5.8` from the current checkout.

| Check | Result |
| --- | --- |
| `AshesOfHeavenEditor Mac Development` | **PASS** — compiled and linked successfully after the final Niagara path and manifest fixes |
| `Scripts/GeneratePhase42Assets.py` | **PASS** — commandlet authoring completed without script errors; normal-editor rerun serialized MetaSound graphs and saved assets under `Content/Ashes` |
| `AHCombatVerificationCommandlet` | **PASS** — 15 tests, 0 failed checks, 0 errors; final log `/tmp/ashes-phase42-commandlet-final4.log` |
| `Automation RunTests AshesOfHeaven` | **PASS** — 16 project tests, all `Result={Success}`, exit code 0; final log `/tmp/ashes-phase42-automation-final3.log` |
| Asset Registry presentation manifest | **PASS** — required UMG/audio/material/Niagara assets loaded |

The automation harness emitted no project failures. The commandlet emitted two known test-world
teardown warnings (`AHObjectiveHUDTestWorld`) after its 15 passing checks; the MCP listener was
isolated to port 8001 for this run because the normal editor owns port 8000. Neither affected the
result.

### Build/package status for this revision

Development Editor, Mac Development and Mac Shipping cook/package, deep strict codesign, and
normal Metal launch checks are complete for this revision. Both fresh archived processes stayed
alive for 15 seconds without `-nullrhi` or `-nosound` and exited with controlled status 0. The
Development package was rebuilt after the material-manifest test was corrected for the UE 5.8
runtime API. The package runs emitted only known toolchain noise: the UE MetalShaderConverter
include warning and the MCP licensing warning; no project build, cook, or test failure remained.

Exact final runtime logs:

- `Saved/Logs/Phase42-AssetGeneration-Final.log`
- `Saved/Logs/Phase42-AHCombatVerificationCommandlet.log`
- `Saved/Logs/Phase42-Automation.log`
- `Saved/Logs/Phase42-Development-NormalLaunch-Final.log`
- `Saved/Logs/Phase42-Shipping-NormalLaunch.log`
- `/tmp/ashes-phase42-commandlet-final4.log`
- `/tmp/ashes-phase42-automation-final3.log`

Final packages (2026-08-22, package counter `0.43`):

- `Builds/macOS-Development/AshesOfHeaven.app`
- `Builds/macOS/AshesOfHeaven.app`

### Explicit human review still required

No human approval is being faked. The following remain open:

- actual HUD readability and visual match at 1280×720, ultrawide, and mobile safe zones;
- sound-design listening review: M91 identity, material impacts, footsteps, Erebus/Transit/Cathedral
  ambience, Veil language, Manticore machinery, dialogue, mix, tails, and silence;
- normal interactive combat, objective clarity without relying on debug text, enemy/friendly AI,
  movement/ADS/recoil/reload, grenades/melee/pickups, death/restart/checkpoint inventory restore,
  Manticore enter/drive/fire/exit, dialogue timing, countdown, terminal progression, and Chapter
  One completion;
- final visual comparison against the four approved targets, character art/animation, lighting,
  fog, fire/smoke/VFX quality, Windows/iOS/Android packaging, and real-device performance.

The next authorized step is human review of the fresh packages. Do not start Phase 5 or Chapter
Two from this record.

## PHASE 4.3 — Presentation quality recertification — 2026-08-22

This pass addresses the concrete gaps identified after the Phase 4.2 Unreal-native pipeline:
stale glyph reticles, no-op UMG animation declarations, missing safe-zone root, collapsed combat
audio routing, footsteps on the wrong route, disconnected material controls, identical Niagara
scaffolds, and runtime dependence on `/Engine/BasicShapes`. Gameplay, Chapter One progression,
checkpoint logic, navigation, and cross-platform boundaries were preserved. Phase 5/Chapter Two
was not started.

The first fresh post-change Shipping smoke exposed a real Apple Metal startup failure: the process
returned 139 while `MTLCompilerService` aborted in `validateSerializedVertexDescriptor`. The Mac
profile now keeps the Lumen path but disables hardware ray tracing, with the guard applied in both
`Config/DefaultDeviceProfiles.ini` and the runtime quality manager. Windows and mobile profiles are
unchanged. The fresh rebuild and recertification below are after that fix.

### Implemented

- `WBP_Crosshair` now uses authored `UBorder` geometry for the core, four spread arms, and hit
  state. The old `+` TextBlock remains only as a collapsed compatibility binding and contains no
  visible glyph. Hit and damage feedback use restrained authored rules and state text rather than
  symbol characters.
- `WBP_HUD_Root` is rooted in a real `USafeZone`. `ObjectiveRevealAnimation`,
  `DamagePulseAnimation`, and `CountdownUrgencyAnimation` are serialized `UWidgetAnimation` assets
  with `RenderOpacity` tracks bound to their root child widgets; runtime calls now resolve real
  animations instead of silently doing nothing.
- Audio now imports every checked-in WAV before dependent assets are created. `Combat.Melee`,
  `Combat.Hurt`, `Combat.Armor`, `Combat.Death`, and `Combat.Grenade` each resolve a distinct
  project source. Footsteps use world attenuation/concurrency and `SM_World`. The mix owns
  `SM_Master`, `SM_World`, `SM_Weapons`, `SM_Ambience`, `SM_Veil`, `SM_Dialogue`, `SM_Music`,
  `SM_Vehicle`, and `SM_UI`. No runtime PCM synthesizer or engine grenade-launcher template is used.
- Material masters connect Wear, Edge, Damage, Wetness, and MicroDetail controls into their
  base-color, roughness, and normal graphs. Project material masters are also explicitly marked
  for Niagara sprites.
- Niagara emitters and systems remain project-owned and now receive effect-specific renderer
  material, facing/alignment/sort, deterministic seed, fixed-bound, allocation, persistent-ID,
  and importance configuration.
- Presentation props load project-owned mesh assets and project-owned material masters at runtime.
  The copied meshes are an explicit starter kit boundary, not a claim that final environment art
  has been authored.

### Machine verification — fresh Unreal processes

| Gate | Exact result |
| --- | --- |
| `Scripts/GeneratePhase42Assets.py` | **PASS** — no Python warnings/errors, missing WAV sources, animation GUID ensures, or listener errors; only known UE 5.8 non-rendering-editor MetaSound save notices. Log: `/tmp/ashes-phase43-assets-final3.log` |
| Development Editor | **PASS** — UE 5.8 Mac arm64 compile/link, exit code 0. Log: `/tmp/ashes-phase43-editor-build-final4.stdout` |
| `AHCombatVerificationCommandlet` | **PASS** — 15/15 checks, 0 failed checks, exit code 0. Log: `/tmp/ashes-phase43-commandlet-final4.log` |
| `Automation RunTests AshesOfHeaven` | **PASS** — 16/16 project tests `Result={Success}`, `**** TEST COMPLETE. EXIT CODE: 0 ****`. Log: `/tmp/ashes-phase43-automation-final4.log` |
| Mac Development cook/package | **PASS** — `BUILD SUCCESSFUL`, 60.53 s; `Builds/macOS-Development/AshesOfHeaven.app`. Log: `/tmp/ashes-phase43-development-package-final5.log` |
| Mac Shipping cook/package | **PASS** — `BUILD SUCCESSFUL`, 49.30 s; `Builds/macOS/AshesOfHeaven.app`. Log: `/tmp/ashes-phase43-shipping-package-final5.log` |
| Deep strict codesign | **PASS** — both fresh packages valid on disk and satisfy their designated requirements. Logs: `/tmp/ashes-phase43-development-codesign-final2.log`, `/tmp/ashes-phase43-shipping-codesign-final2.log` |
| Normal Metal launch | **PASS** — both fresh packages ran 15 s without `-nullrhi`/`-nosound`, produced no crash/assertion/fatal lines, and exited status 0. Logs: `/tmp/ashes-phase43-development-normal-final5.log`, `/tmp/ashes-phase43-shipping-normal-final6.log` |

Automation emitted two expected categories of engine noise: Unreal's built-in `UnifiedErrorTest`
logs sample error messages by design, and the objective-HUD test leaves the known temporary-world
teardown warning after its passing result. There were no project test failures.

### Human validation still required

Codex cannot honestly replace the requested interactive playthrough. A human must still run the
fresh `Builds/macOS/AshesOfHeaven.app` and judge controls, gun feel, ADS/recoil/reload, enemy and
friendly AI, objective clarity, pacing, Manticore operation, countdown, checkpoint transitions,
dialogue timing, collision/pathfinding, softlocks, crashes, pickups, melee, grenades, death/restart,
inventory restoration, and Chapter One completion. The final sound-design listen, visual reference
match, authored environment/character/animation quality, localization, and real Windows/iOS/
Android/device performance are also not claimed by these machine checks.

Do not start Phase 5 or Chapter Two from this record.

## PHASE 4.4.1 — Runtime presentation recovery — 2026-08-22

This focused pass addresses the defects visible in the current packaged first-playable view. It
keeps the existing cross-platform architecture and uses project-owned Unreal assets/materials;
it does not begin Phase 5 or Chapter Two.

### Defects fixed

- Removed the camera-attached placeholder gauntlet slabs that were dominating the view. The
  first-person weapon is now a smaller, lower/right local presentation, and the player weapon is
  hidden while the opening presentation is active.
- Added an authored full-screen black opening curtain. Opening dialogue owns the screen until the
  sequence completes; objective, status, weapon, crosshair, interaction, damage, countdown,
  vehicle, and chapter-title gameplay presentation are suppressed during that interval. Movement
  and look input are also locked for the opening sequence.
- Reworked the HUD asset layout for safe-zone/anchor-based responsive placement: compact objective
  reveal, compact player status, minimal weapon/ammo readout, restrained crosshair, transient hit/
  damage indicators, and a smaller dialogue treatment. The permanent weapon-name dossier and
  `OBJECTIVE 01` metadata are removed from the normal gameplay presentation.
- Wired runtime crosshair state to ADS, spread, interaction target, and vehicle context. Hit and
  damage feedback now reveal authored rules/indicators briefly and then clear themselves.
- Replaced remaining runtime prototype material references in combat character presentation with
  `/Game/Ashes/Materials/M_VeilObsidian` and `/Game/Ashes/Materials/M_HumanMetal`, while keeping
  collision/navigation actors intact. The presentation visibility debug toggle remains available
  for development verification.
- Removed the last runtime `/Engine/BasicShapes` and `/Game/LevelPrototyping` loads from the
  playable slice constructors. Runtime block, sphere, cylinder, vehicle, weapon, and Erebus
  geometry now resolve the checked-in `/Game/Ashes/Presentation/Meshes/SM_AH_*` assets and the
  authored human-metal material family.

### Machine verification

| Gate | Exact result |
| --- | --- |
| Development Editor | **PASS** — Mac arm64 compile/link, exit code 0; `/tmp/phase441-editor-build-final.log` |
| `AHCombatVerificationCommandlet` | **PASS** — 20/20 checks, 0 failed checks, exit code 0; `/tmp/phase441-commandlet-final.stdout` |
| `Automation RunTests AshesOfHeaven` | **PASS** — 21/21 project tests started and completed with `Result={Success}`, `**** TEST COMPLETE. EXIT CODE: 0 ****`; `/tmp/phase441-automation-final.log` |
| Mac Development cook/package | **PASS** — `BUILD SUCCESSFUL`; `Builds/macOS-Development/AshesOfHeaven.app`; `/tmp/phase441-development-package-final.log` |
| Mac Shipping cook/package | **PASS** — `BUILD SUCCESSFUL`; `Builds/macOS/AshesOfHeaven.app`; `/tmp/phase441-shipping-package-final.log` |
| Deep strict codesign | **PASS** — final Development and Shipping packages verified; `/tmp/phase441-development-codesign-final.log`, `/tmp/phase441-shipping-codesign-final.log` |
| Normal Metal launch | **PASS** — final Development and Shipping apps remained alive for 12 seconds without `-nullrhi` or `-nosound` and exited cleanly after controlled shutdown; `/tmp/phase441-development-normal-final.log`, `/tmp/phase441-shipping-normal-final.log` |

The automation output includes Unreal's intentional `UnifiedErrorTest` sample error messages and
the known temporary HUD test-world teardown warnings after successful tests. They are engine/test
harness noise, not project test failures. The commandlet run was isolated to MCP port 18080 because
the connected Unreal Editor owns port 8000.

### Human validation still required

No normal-runtime screenshot was captured: macOS display capture returned `could not create image
from display`. Therefore this record does not claim visual approval. A human must inspect the
actual packaged app for the opening curtain/dialogue, absence of giant black geometry, first
playable Erebus composition, M91 framing, crosshair and hit/damage states, HUD readability at the
target resolutions, and reference-image match. Human playtesting is also still required for
movement/mouse feel, shooting/ADS/recoil/reload, enemy and friendly AI, objective clarity/pacing,
pickups/melee/grenades, death/restart inventory restoration, checkpoints, Manticore operation,
dialogue/countdown/terminal progression, awkward routes, collision/pathfinding, softlocks, crashes,
and Chapter One completion. This pass does not claim final sound design, final art quality, or
cross-platform device validation.

Do not start Phase 5 or Chapter Two from this record.

## PHASE 4.4 — Runtime art integration and Chapter state correctness — 2026-08-22

This pass fixes the normal packaged Chapter One path, not only the ArtTarget launcher. Phase 5 and
Chapter Two were not started.

### Defects reproduced and fixed

- A stale save could restore an early stage with a later objective/checkpoint. Chapter state now has
  one canonical normalization path, a save version, bounded objective indices, stage/objective
  mapping, map validation, and a development-only `-freshchapter`/`-resetprogress` clean start.
- Objective completion now persists the next objective index. `StarsDisappearing` no longer jumps
  directly to completion; the final title objective is the only route to `ChapterComplete`.
- The HUD explicitly initializes transient widgets hidden, and controller/HUD completion delegates
  refuse to display completion before the canonical final stage. This fixes the Objective 06 / chapter
  completion overlap at the state and delegate boundaries instead of hiding it cosmetically.
- Checkpoint restore normalizes chapter/objective state before restoring objectives and rejects a
  checkpoint saved for another map. Inventory, grenade, encounter, vehicle, and narrative state stay
  on the same restore path.
- Normal `L_ChapterOne_Greybox` startup now consumes project-owned presentation meshes/materials,
  environment profiles, Blueprint props, Niagara systems, and stage-mapped audio. Greybox collision
  and navigation actors remain present but their visible prototype meshes/labels are hidden. Two
  remaining runtime LevelPrototyping mesh references were replaced with project-owned meshes after
  the first packaged smoke exposed their load errors.
- The five combat semantic events now resolve five distinct project Sound Cue sources rather than
  reusing the M91 impact source for melee, hurt, armor, death, and grenade.
- `Scripts/Build-Mac.sh` automatically selects an isolated MCP cook port when the editor owns 8000;
  this prevents the connected editor service from turning a successful cook into a commandlet error.

### Machine verification

| Gate | Result |
| --- | --- |
| Development Editor | **PASS** — Mac arm64 compile/link, exit code 0; `/tmp/phase44-editor-build-r4.log` |
| `AHCombatVerificationCommandlet` | **PASS** — 20 checks, 0 failed checks, 0 errors; `/tmp/phase44-commandlet-r3.log` |
| `Automation RunTests AshesOfHeaven` | **PASS** — 21 project tests completed with `Result={Success}`, exit code 0; `/tmp/phase44-automation-r4.log` |
| Mac Development cook/package | **PASS** — `BUILD SUCCESSFUL`, fresh app at `Builds/macOS-Development/AshesOfHeaven.app`, with automatic MCP-port selection exercised; `/tmp/phase44-development-package-script-auto.log` |
| Mac Shipping cook/package | **PASS** — `BUILD SUCCESSFUL`, fresh app at `Builds/macOS/AshesOfHeaven.app`; `/tmp/phase44-shipping-package-final.log` |
| Deep strict codesign | **PASS** — both fresh packages valid on disk and satisfy their designated requirements; `/tmp/phase44-codesign-final2.log` |
| Normal Metal launch | **PASS** — both fresh packages stayed alive for 18 seconds without `-nullrhi` or `-nosound`, then exited via controlled status 0; `/tmp/phase44-development-normal-script-auto.log`, `/tmp/phase44-shipping-normal-final.log` |
| Normal Development runtime presentation | **PASS for integration smoke** — `L_ChapterOne_Greybox`, `OpeningBlack`, `ErebusOpening`, authored material family, fog/sky/lighting, `SC_Erebus_Ambience`, 117 placed presentation actors, and six VFX systems were logged with no Phase 4.4 errors; `/tmp/phase44-development-normal-script-auto.log` |

The first post-package presentation smoke did find and fix two missing LevelPrototyping mesh loads;
the final package was rebuilt and the final runtime smoke was rerun after that fix. Remaining log
noise is engine/toolchain noise: the MetalShaderConverter include warning and the MCP licensing
warning. The commandlet also retains the known objective-HUD test-world teardown warning after its
passing checks; it is not a project test failure.

No normal-runtime screenshot was captured. Window capture hung in this environment and full-screen
`screencapture` returned `could not create image from display`; no screenshot or visual approval is
being claimed.

### Human validation still required

Machine checks do not replace the two requested interactive runs. Human review is still required for
movement/mouse feel, shooting/ADS/recoil/reload, enemy and friendly AI, objective clarity and pacing,
pickup/grenade/melee behavior, death/restart inventory restoration, checkpoint transitions, Manticore
enter/drive/fire/exit, dialogue timing, countdown, terminal progression, awkward routes, collision,
softlocks, crashes, and reaching Chapter Complete. It also remains required for subjective HUD and
reference-image match, listening quality, final authored environment/character/animation quality,
Windows/iOS/Android validation, and target-device performance. Geometry, lighting, placeholder
animation, VFX, and sound remain prototype/scaffold quality where the earlier art-target sections say
so; this pass does not claim AAA visual approval.

Do not start Phase 5 or Chapter Two from this record.

## Phase 4.8 — Erebus visual gate iteration 2 (2026-08-23)

The Phase 4.6 packaged result was rejected by human review as a dark dressed
blockout. This pass iterated Erebus ONLY (Transit/Cathedral/Lucian-Maya
prohibited and untouched):

- Gen-3 architecture: SM_Erebus_Fortress_A (34m seamed fortress slab),
  SM_Erebus_Fortress_B (elevated block on legs), SM_Erebus_TowerSlab_A,
  SM_Erebus_CheckpointGate_A, SM_Erebus_BannerDrape_A/B,
  SM_Erebus_DebrisField_A/B; deeper facade roofs/units, real Cathedral fluting.
- Scale correction: Cathedral cluster moved from 82m (a sky-sealing wall) to
  265-335m anchor-local; skyline rings pushed out; floating ring copings seated.
- Tonal re-anchor: bright storm sky (MI_Erebus_StormCloud on the engine cloud
  material), backlit sun 32 @ pitch -15/yaw -150 with shadows, RayleighScale
  2.2/multiscatter 4, ground-hugging fog (0.022 / falloff 0.30 / inscattering
  0.55), skylight 0.55-0.75, AE bias -0.7, min 0.03, max 3.0.
- Materials: MI ground family darkened + wetness balanced, Concrete_Light
  lifted for band contrast, mud/asphalt tiling densified.
- Niagara: readable fires (pool-shaped), calmer embers, heavy smoke columns;
  legacy transit overhead wires removed; fires repositioned onto sources
  visible from the review pose.
- Validation: editor automation 27/27, commandlet 21/21.

Status: awaiting HUMAN visual review of Saved/Phase48Evidence packaged
captures. NOT marked approved.

## LEVEL ONE — FOR A WHILE — 2026-08-25

The Chapter One campaign boundary moved: Level One ends when Erebus is destroyed and Nysa
transmits `Lucian.`. `AHChapterStateConstants::ObjectiveCount` is 12 and the chapter save version
is 7. Everything above this line describing 17 objectives, 21 reachable stages, or the ten-years-later
epilogue is the historical greybox shape, not current behaviour.

Current contract, verification and known gaps: `Docs/LEVEL1_FOR_A_WHILE.md`.
