# Build and install

All commands run from the project root. Set `UE_ROOT` to the installed Unreal Engine 5.8 root when it is not in the script default location. Build outputs are written below `Builds/` and are not source-controlled.

## Windows x64

Requirements: Windows 10/11, Visual Studio/toolchain supported by UE 5.8, and an installed Unreal Engine.

```powershell
$env:UE_ROOT = 'C:\Program Files\Epic Games\UE_5.8'
& .\Scripts\Build-Windows.ps1
```

The Shipping archive contains `AshesOfHeaven.exe` and its packaged files. Run the executable from the archived Windows directory; Unreal Editor is not required. An installer can wrap that archive using the studio's approved installer system without changing the game package.

## macOS Apple Silicon

Requirements: macOS on Apple Silicon, Xcode command-line tools, and UE 5.8. Intel macOS should only be enabled after the installed UE/toolchain is verified for it.

```bash
export UE_ROOT='/Users/Shared/Epic Games/UE_5.8'
./Scripts/Build-Mac.sh
```

The archive contains `AshesOfHeaven.app`. For distribution, sign and notarize outside source control, for example:

```bash
codesign --deep --force --options runtime --sign "$MAC_CODESIGN_IDENTITY" Builds/macOS/AshesOfHeaven.app
xcrun notarytool submit Builds/macOS/AshesOfHeaven.app.zip --keychain-profile "$MAC_NOTARY_PROFILE" --wait
```

Create a DMG only after signing/notarization succeeds. Never put certificates, private keys, or notarization credentials in this repository.

## Android ARM64

Requirements: Android SDK/NDK versions selected by the installed UE 5.8 Android toolchain, `adb`, and `ANDROID_HOME` or `ANDROID_SDK_ROOT`.

```bash
export UE_ROOT='/Users/Shared/Epic Games/UE_5.8'
export ANDROID_SDK_ROOT="$ANDROID_HOME"
./Scripts/Build-Android.sh
adb install -r Builds/Android/**/AshesOfHeaven-arm64.apk
```

The normal run produces an ARM64 testing APK. For a store/distribution run, configure the secure Android keystore in the Unreal project/build environment and set `AH_ANDROID_AAB=1` and `ANDROID_KEYSTORE`; the distribution archive is staged under `Builds/Android/AAB`. Keystore passwords and store credentials must be injected by CI secrets, never committed.

## iOS/iPadOS

Requirements: macOS, Xcode, UE 5.8 iOS toolchain, a valid Apple signing identity, provisioning profile, and team configuration for a signed IPA.

```bash
export UE_ROOT='/Users/Shared/Epic Games/UE_5.8'
./Scripts/Build-iOS.sh
```

For a signed distribution build, provide the external environment and rerun:

```bash
export AH_IOS_DISTRIBUTION=1
export AH_IOS_TEAM_ID='YOUR_TEAM_ID'
export AH_IOS_PROVISIONING_PROFILE='YOUR_PROFILE_NAME'
./Scripts/Build-iOS.sh
```

Install development builds through Xcode or `devicectl`; distribute signed builds through TestFlight/App Store. The current project intentionally does not contain Apple certificates, private keys, profiles, or team secrets.

## Validation and CI entry point

```bash
./Scripts/Validate-CrossPlatform.sh
```

An Unreal-equipped runner should invoke the platform script for its host platform, then run automated tests and archive the output. CI must keep Windows, macOS, Android, and iOS jobs separate because Unreal/SDK/signing toolchains are not interchangeable.
