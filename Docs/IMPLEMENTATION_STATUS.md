# Phase 2 implementation status

## COMPLETE

- Erebus combat prototype map is the editor and packaged-game entry point.
- Reusable player combat foundation: movement, sprint, crouch, mantle probe, health, armor, death, damage direction, god mode, and respawn flow.
- M91 Revenant automatic hitscan rifle with magazine/reserve ammo, reload transfer, ADS, recoil, hit confirmation, headshots, falloff, muzzle/impact/tracer hooks, and empty/reload audio hooks.
- Melee trace, knockback, grenade inventory, projectile grenades with fuse/radial falloff/impulse, and pickup interaction.
- Veil Pilgrim combatants, human soldier friendlies, faction hostility rules, sight perception, target selection, cover-aware movement hooks, fallback movement, repositioning, investigate/last-known behavior, and grenade reaction.
- Five-objective flow, objective zones, three authored encounters, three checkpoints, checkpoint serialization/restoration, death reload, encounter persistence, mission completion, and restart/debug commands.
- Canvas HUD with health/armor, ammo, grenades, objective, interaction prompt, crosshair, hit/headshot feedback, armor-break feedback, directional damage indicator, low-health overlay, and completion state.
- Shared mobile action paths for fire, ADS, reload, grenade, melee, interact, jump, sprint, crouch, and weapon cycling.
- Mac Development Editor compile, Mac Shipping build/cook/stage/archive, local codesign verification, and a normal Metal-renderer packaged launch smoke check completed on 2026-08-21.

## PHASE 2.1 ACCEPTANCE VERIFICATION — 2026-08-21

- Fresh-process `AHCombatVerificationCommandlet`: 9 tests, 0 failed checks, PASS. The checks covered health/damage, armor/regen, ammo/reload, grenades, the ordered five-objective chain, objective-history restore, checkpoint serialization, encounter configuration, and faction hostility. The old editor process was closed before this run; the commandlet used the absolute project path and `-nullrhi` for deterministic non-interactive verification.
- Fresh-process `Automation RunTests AshesOfHeaven.Combat`: 9 tests found, 9 completed with `Result={Success}`, 0 failures, `**** TEST COMPLETE. EXIT CODE: 0 ****`.
- `./Scripts/Build-Mac.sh`: Development Editor and Mac Shipping targets compiled; cook, stage, pak, archive, and local codesign completed successfully. Archive: `Builds/macOS/AshesOfHeaven.app`. `codesign --verify --deep --strict` passed. The packaged app launched normally without `-nullrhi`, remained alive for 1m24s, exposed a visible standard window, and the process sample confirmed `FMetalRHI`/Metal viewport activity. No crash report was created during the smoke run.
- Acceptance guards verified in code/tests: all five objectives can complete and reach mission completion; objective and encounter state plus ammo/grenade state survive checkpoint serialization/restore; destroyed or invalid active encounter actors are pruned so they cannot strand an encounter; the director owns one-per-world encounter/pickup spawning and restart is a world reload; only Veil Pilgrims are encounter enemies, so friendly soldiers cannot block encounter completion; Veil AI has navigation queries plus a movement fallback path.
- No major feature work, Chapter One work, or Phase 3 work was started.

## IN PROGRESS

- Art, animation, authored cover geometry, navigation-volume tuning, and final audio/VFX assets are still prototype quality. The runtime fallback blockout keeps the slice playable while those assets are produced.
- Interactive combat progression and mobile device playtest evidence still need to be recorded in the platform matrix.
- Standard Unreal automation tests are present in `Source/AshesOfHeaven/Tests/AHCombatTests.cpp`, and the direct verification commandlet was rerun successfully from a fresh process after the prior editor session was closed.

## KNOWN ISSUES

- The runtime blockout constructs geometry and encounters from `AHErebusCombatSliceDirector`; it is intentionally a proving-ground scaffold, not final environmental art.
- No final first-person animation set, character animation set, authored cover kit, or bespoke audio bank is included yet; all systems expose hooks for them.
- Unreal emits the installed-machine warnings for unavailable Win64/Android/Linux SDKs during validation. These do not affect the successful Mac build.
- The blank authored map has no hand-authored navigation mesh asset; AI includes navigation queries plus a movement fallback until the final nav volume is authored.

## BLOCKED BY EXTERNAL TOOLCHAIN

- Windows packaging requires a Windows build environment and Windows SDK.
- Android packaging requires the Android SDK/NDK and signing configuration; the current host reports Android SDK `r27c` unavailable.
- iOS device packaging requires the target Apple signing/team provisioning path and a device run.
- Full controller, touch, suspend/resume, performance, and thermal validation require the target hardware.

## NEXT

- Play the slice from `ReachDefensivePosition` through `ReachExtraction`, verify objective transitions, enemy persistence, checkpoint reload, death/restart, pickup behavior, melee, grenades, and mission completion.
- Replace runtime blockout meshes with authored Erebus cover/lighting, add NavMeshBoundsVolume, and bind final animation/audio/VFX assets.
- Record Mac interactive evidence, then repeat the matrix on Windows, Android, and iOS when their toolchains and devices are available.
