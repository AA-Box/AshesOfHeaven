// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AHPlatformTypes.generated.h"

/** The platform families that share the Ashes of Heaven gameplay code. */
UENUM(BlueprintType)
enum class EAHPlatformFamily : uint8
{
	Unknown,
	Windows,
	Mac,
	Android,
	IOS
};

/** Player-facing quality presets. Auto resolves once per device and can be overridden by the user. */
UENUM(BlueprintType)
enum class EAHQualityPreset : uint8
{
	Auto,
	Low,
	Medium,
	High,
	Ultra,
	Mobile
};

/** Mobile quality modes intentionally map to stable frame-time targets. */
UENUM(BlueprintType)
enum class EAHMobilePerformanceMode : uint8
{
	Quality,
	Balanced,
	Performance,
	Auto
};

/** The small set of touch actions shared by native and Blueprint mobile controls. */
UENUM(BlueprintType)
enum class EAHMobileTouchAction : uint8
{
	Fire,
	ADS,
	Jump,
	Crouch,
	Reload,
	Interact,
	Grenade,
	Melee,
	Sprint,
	WeaponNext,
	WeaponPrevious,
	VehicleAccelerate,
	VehicleBrake,
	VehicleExit,
	VehicleSwitchSeat
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHPlatformCapabilities
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Platform")
	EAHPlatformFamily PlatformFamily = EAHPlatformFamily::Unknown;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Platform")
	FString PlatformName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Platform")
	bool bIsMobile = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Platform")
	bool bIsDesktop = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rendering")
	bool bSupportsNanite = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rendering")
	bool bSupportsLumen = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rendering")
	bool bSupportsVirtualShadowMaps = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Rendering")
	bool bSupportsDynamicResolution = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input")
	bool bSupportsTouch = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input")
	bool bSupportsGyro = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Input")
	bool bSupportsExternalController = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Platform")
	bool bSupportsPlatformSaveLocation = true;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHGraphicsProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	EAHQualityPreset Preset = EAHQualityPreset::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics", meta=(ClampMin=25.0, ClampMax=200.0))
	float RenderScale = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	int32 TextureQuality = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	int32 ShadowQuality = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	int32 EffectsQuality = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	int32 PostProcessQuality = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	int32 FoliageQuality = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics", meta=(ClampMin=0.0, ClampMax=20000.0))
	float ShadowDistance = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bUseDynamicResolution = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bUseLumen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bUseNanite = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bUseVirtualShadowMaps = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bMotionBlur = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bFilmGrain = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Graphics")
	bool bCameraShake = true;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHPerformanceProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	float TargetFrameRate = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	float MinimumStableFrameRate = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 MaxActiveCombatants = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 MaxMidDistanceActors = 48;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 MaxDistantSimulationActors = 96;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	float MidDistanceTickInterval = 0.10f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 MaxPersistentVFX = 64;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 MaxDynamicLights = 16;

	/** Per-combatant body fill lights. One unshadowed point light per body fits a desktop budget,
	 * but the mobile profile affords MaxDynamicLights (4) for the whole scene against 8-16
	 * combatants, so mobile bodies read off the sun and skylight instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	bool bCharacterFillLights = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 MaxProjectilePoolSize = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Performance")
	int32 ThermalMitigationAfterMinutes = 10;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHInputProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bUsesTouchControls = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bSupportsGyroAiming = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bSupportsExternalController = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bAimAssistEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin=0.1, ClampMax=5.0))
	float TouchSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin=0.1, ClampMax=5.0))
	float ADSSensitivity = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input", meta=(ClampMin=0.0, ClampMax=1.0))
	float TouchControlOpacity = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	float TouchControlScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bHoldToAim = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	bool bHoldToCrouch = true;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHDeviceProfile
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Device")
	FString DeviceProfileName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Device")
	FString DeviceMake;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Device")
	FString DeviceModel;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Device")
	int32 ApproximateMemoryGB = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Device")
	bool bHighEnd = false;
};
