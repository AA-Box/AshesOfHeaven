# Ashes of Heaven

Cross-platform horror shooter built with Unreal Engine 5.8.0.

The project includes a shared platform layer for Windows, macOS, Android, and iOS, with mobile touch controls, lifecycle handling, runtime quality settings, save support, and platform-aware input/UI behavior.

## Project layout

- `Source/AshesOfHeaven/` — gameplay and shared platform systems
- `Config/` — cross-platform device profiles, scalability, and project settings
- `Build/` — platform packaging resources and entitlements
- `Scripts/` — Windows, macOS, Android, iOS, and validation scripts
- `Docs/` — platform matrix, performance budgets, controls, streaming, and build notes

## Requirements

- Unreal Engine 5.8.0
- Platform SDKs and signing credentials for the targets you intend to package

## Build and validation

Run the platform-specific script from the `Scripts/` directory, or use `Scripts/Validate-CrossPlatform.sh` for a source/configuration validation pass. See [`Docs/BUILD_AND_INSTALL.md`](Docs/BUILD_AND_INSTALL.md) for setup details and [`Docs/PLATFORM_MATRIX.md`](Docs/PLATFORM_MATRIX.md) for target coverage.

Generated Unreal output and packaged builds are intentionally excluded from version control. Keep signing material local and provide it through the platform toolchain when packaging.
