// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AHPlatformTypes.h"
#include "AHPlatformSettings.generated.h"

/** Central project settings for platform selection and safe runtime fallbacks. */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="Ashes of Heaven Platform Settings"))
class ASHESOFHEAVEN_API UAHPlatformSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAHPlatformSettings();

	static const UAHPlatformSettings* Get();

	UPROPERTY(config, EditAnywhere, Category="Quality")
	EAHQualityPreset DefaultQualityPreset = EAHQualityPreset::Auto;

	UPROPERTY(config, EditAnywhere, Category="Quality")
	EAHMobilePerformanceMode DefaultMobilePerformanceMode = EAHMobilePerformanceMode::Auto;

	UPROPERTY(config, EditAnywhere, Category="Quality")
	bool bEnableDynamicResolution = true;

	UPROPERTY(config, EditAnywhere, Category="Quality")
	bool bAllowDesktopCinematicOptions = true;

	UPROPERTY(config, EditAnywhere, Category="Input")
	bool bForceTouchControls = false;

	UPROPERTY(config, EditAnywhere, Category="Input")
	bool bEnableMobileAimAssist = true;

	UPROPERTY(config, EditAnywhere, Category="Input")
	bool bEnableGyroAiming = true;

	UPROPERTY(config, EditAnywhere, Category="Save")
	bool bSaveOnApplicationSuspend = true;

	UPROPERTY(config, EditAnywhere, Category="Save")
	FString DefaultSaveSlot = TEXT("AshesOfHeaven_Slot_0");

	UPROPERTY(config, EditAnywhere, Category="Debug")
	bool bForceMobileProfileInEditor = false;

	UPROPERTY(config, EditAnywhere, Category="Debug")
	bool bLogPlatformProfileAtStartup = true;
};

