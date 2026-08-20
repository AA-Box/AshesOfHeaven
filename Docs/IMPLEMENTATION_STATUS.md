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
- Mac editor compile, Mac Shipping build/cook/stage/archive, local codesign verification, and packaged Null RHI boot/tick verification completed on 2026-08-21.

## IN PROGRESS

- Art, animation, authored cover geometry, navigation-volume tuning, and final audio/VFX assets are still prototype quality. The runtime fallback blockout keeps the slice playable while those assets are produced.
- Interactive combat progression and mobile device playtest evidence still need to be recorded in the platform matrix.
- Standard Unreal automation tests are present in `Source/AshesOfHeaven/Tests/AHCombatTests.cpp`. A direct editor verification commandlet is also present, but the currently open editor session owns an older hot-reloaded module, so this session could not execute the freshly renamed commandlet body without restarting the editor.

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

- Restart the editor, run the fresh `AHCombatVerificationCommandlet`, and record its seven subsystem checks.
- Play the slice from `ReachDefensivePosition` through `ReachExtraction`, verify objective transitions, enemy persistence, checkpoint reload, death/restart, pickups, melee, grenades, and mission completion.
- Replace runtime blockout meshes with authored Erebus cover/lighting, add NavMeshBoundsVolume, and bind final animation/audio/VFX assets.
- Record Mac interactive evidence, then repeat the matrix on Windows, Android, and iOS when their toolchains and devices are available.
