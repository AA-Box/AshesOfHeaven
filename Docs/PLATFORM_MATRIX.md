# ASHES OF HEAVEN platform QA matrix

This matrix is evidence-gated. `UNTESTED` means the project has source/config support but no verified interactive run on that target. `BLOCKED BY TOOLCHAIN` means validation cannot be performed on the current build machine. Do not change a cell to `PASS` without a dated device/build record.

| Feature | Windows | macOS | Android | iOS |
| --- | --- | --- | --- | --- |
| Launch | UNTESTED | PASS — packaged process stayed alive under normal Metal launch smoke check (2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
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
| Packaging | BLOCKED BY TOOLCHAIN | PASS — UE 5.8 Apple Silicon Shipping archive, local codesign verification (2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |

## Mac evidence record

- Date: 2026-08-21.
- Build: Development Editor plus Mac Shipping from `./Scripts/Build-Mac.sh`; package `Builds/macOS/AshesOfHeaven.app`; package counter `0.12`.
- Engine/toolchain: Unreal Engine 5.8 Apple Silicon toolchain on macOS.
- Renderer: normal packaged launch without `-nullrhi`; process-level Metal launch smoke check.
- Result: process remained alive for approximately 20 seconds and was stopped with a controlled interrupt; `codesign --verify --deep --strict --verbose=2` passed.
- Scope: this record proves compile/package/codesign/process launch only. It does not prove interactive combat, AI, touch/controller, checkpoint, death/restart, pickup, Manticore, dialogue, or Chapter completion behavior.

Interactive combat and full end-to-end checkpoint progression still require a human play session. Windows, Android, and signed iOS artifacts require their corresponding external build/signing environments.
