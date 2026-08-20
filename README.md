# Ashes of Heaven

Cross-platform military science-fiction FPS built with Unreal Engine 5.8.

The current proving ground is Erebus, a compact combat vertical slice: first-person movement, sprint/crouch/mantle traversal, the M91 Revenant rifle, ADS/reload/melee/grenades, health and armor, Veil Pilgrim enemies, human allies, objective-driven encounters, checkpoints, death/restart, HUD feedback, audio/VFX hooks, and mobile touch paths. The project also includes shared platform services for Windows, macOS, Android, and iOS, including lifecycle handling, runtime quality settings, save support, and platform-aware input/UI behavior.

## Project layout

- `Source/AshesOfHeaven/` — gameplay and shared platform systems
- `Content/Combat/L_Erebus_CombatPrototype.umap` — authored entry map for the Erebus slice
- `Config/` — cross-platform device profiles, scalability, and project settings
- `Build/` — platform packaging resources and entitlements
- `Scripts/` — Windows, macOS, Android, iOS, and validation scripts
- `Docs/` — platform matrix, performance budgets, controls, streaming, and build notes

## Requirements

- Unreal Engine 5.8.0
- Platform SDKs and signing credentials for the targets you intend to package

## Build and validation

Run the platform-specific script from the `Scripts/` directory, or use `Scripts/Validate-CrossPlatform.sh` for a source/configuration validation pass. On macOS, `Scripts/Build-Mac.sh` creates a signed-for-local-run Shipping archive at `Builds/macOS/AshesOfHeaven.app`. See [`Docs/BUILD_AND_INSTALL.md`](Docs/BUILD_AND_INSTALL.md) for setup details, [`Docs/IMPLEMENTATION_STATUS.md`](Docs/IMPLEMENTATION_STATUS.md) for the Phase 2 handoff, and [`Docs/PLATFORM_MATRIX.md`](Docs/PLATFORM_MATRIX.md) for target coverage.

The editor startup map and packaged game default map are both `L_Erebus_CombatPrototype`. The combat slice also exposes development-only console commands such as `AIDebug`, `ObjectiveDebug`, `ReloadCheckpoint`, `RestartEncounter`, `KillAllEnemies`, `SpawnPilgrim`, and `TeleportToEncounter`.

Generated Unreal output and packaged builds are intentionally excluded from version control. Keep signing material local and provide it through the platform toolchain when packaging.
