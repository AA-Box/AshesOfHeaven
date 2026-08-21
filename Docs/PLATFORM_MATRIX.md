# ASHES OF HEAVEN platform QA matrix

This matrix is evidence-gated. `UNTESTED` means the project has source/config support but no verified run on that target. `BLOCKED BY TOOLCHAIN` means validation cannot be performed on the current build machine. Do not change a cell to `PASS` without a dated device/build record.

| Feature | Windows | macOS | Android | iOS |
| --- | --- | --- | --- | --- |
| Launch | UNTESTED | PASS (packaged Shipping app launched with normal Metal RHI, visible standard window, no crash report; 2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |
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
| Packaging | BLOCKED BY TOOLCHAIN | PASS (UE 5.8 Apple Silicon Shipping archive and codesign verification; 2026-08-21) | BLOCKED BY TOOLCHAIN | BLOCKED BY TOOLCHAIN |

## Required evidence record

For each `PASS`, record:

- commit/build identifier and Unreal Engine version;
- device model, OS version, renderer/RHI, quality preset, and frame-rate mode;
- scenario tested and result (including save slot/checkpoint behavior);
- date and tester.

The Mac host currently has the UE 5.8 Apple Silicon toolchain. The source/editor build completed successfully and the packaged app passed `codesign --verify --deep --strict`; the launch smoke check used the normal Metal renderer rather than Null RHI. Windows, Android, and signed iOS artifacts require their corresponding external build/signing environments. Interactive combat, AI, touch, and full end-to-end checkpoint progression still need a human play session; they remain `UNTESTED` here until that evidence is recorded.
