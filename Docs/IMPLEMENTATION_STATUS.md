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

## Next

Human playthrough and platform evidence are the remaining Phase 3 acceptance work. Do not begin Chapter Two or Phase 4 until that evidence is recorded.
