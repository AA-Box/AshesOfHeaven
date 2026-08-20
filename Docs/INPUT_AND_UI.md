# Input and responsive UI

Gameplay consumes device-independent Enhanced Input actions. The project keeps Blueprint mapping contexts for authored controls, while `UAHPlatformManagerSubsystem` creates a native fallback context with these names:

`IA_Move`, `IA_Look`, `IA_Fire`, `IA_ADS`, `IA_Reload`, `IA_Jump`, `IA_Crouch`, `IA_Sprint`, `IA_Melee`, `IA_Grenade`, `IA_Interact`, `IA_WeaponNext`, `IA_WeaponPrevious`, `IA_Pause`, `IA_VehicleAccelerate`, `IA_VehicleBrake`, and `IA_VehicleSteer`.

Desktop supports keyboard/mouse and Unreal-compatible controllers. Mobile uses a left virtual movement zone, a right look zone, and contextual action buttons. `UAHMobileControlsWidget` is the native two-thumb foundation; a project-specific Widget Blueprint can add fire, ADS, reload, interact, grenade, melee, weapon switch, and vehicle controls by forwarding `PressAction`/`ReleaseAction`.

Mobile profiles expose touch sensitivity, separate ADS sensitivity, control opacity/scale, hold/toggle policy, optional gyro, and subtle aim assist. When a controller is connected, the authored UI should hide or minimize touch controls and switch prompts; when touch resumes it should restore them.

All HUD roots should use a UMG `SafeZone` and call `UAHPlatformUIBlueprintLibrary::GetResponsiveHUDMetrics`. This accounts for viewport size, DPI, display cutouts, rounded corners, Dynamic Island/system gesture regions, and 16:9/16:10/19.5:9/20:9/tablet/ultrawide layouts without fixed pixel coordinates. Keep gameplay landscape on mobile.

Subtitles must expose enabled state, font scale, background opacity, speaker labels, and a safe-zone-aware position that does not overlap touch controls.
