// Copyright Epic Games, Inc. All Rights Reserved.


#include "AshesOfHeavenPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputMappingContext.h"
#include "AshesOfHeavenCameraManager.h"
#include "Blueprint/UserWidget.h"
#include "AshesOfHeaven.h"
#include "Widgets/Input/SVirtualJoystick.h"
#include "Platform/AHMobileControlsWidget.h"
#include "Platform/AHPlatformManagerSubsystem.h"

AAshesOfHeavenPlayerController::AAshesOfHeavenPlayerController()
{
	// set the player camera manager class
	PlayerCameraManagerClass = AAshesOfHeavenCameraManager::StaticClass();
}

void AAshesOfHeavenPlayerController::BeginPlay()
{
	Super::BeginPlay();

	if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
	{
		PlatformManager->OnApplicationSuspended.AddUObject(this, &AAshesOfHeavenPlayerController::HandleApplicationSuspended);
		PlatformManager->OnApplicationResumed.AddUObject(this, &AAshesOfHeavenPlayerController::HandleApplicationResumed);
	}

	
	// only spawn touch controls on local player controllers
	if (IsLocalPlayerController() && ShouldUseTouchControls())
	{
		// spawn the mobile controls widget
		TSubclassOf<UUserWidget> ControlsClass = MobileControlsWidgetClass;
		if (!ControlsClass)
		{
			ControlsClass = UAHMobileControlsWidget::StaticClass();
		}
		MobileControlsWidget = CreateWidget<UUserWidget>(this, ControlsClass);

		if (MobileControlsWidget)
		{
			// add the controls to the player screen
			MobileControlsWidget->AddToPlayerScreen(0);

		} else {

			UE_LOG(LogAshesOfHeaven, Error, TEXT("Could not spawn mobile controls widget."));

		}

	}
}

void AAshesOfHeavenPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
	{
		PlatformManager->OnApplicationSuspended.RemoveAll(this);
		PlatformManager->OnApplicationResumed.RemoveAll(this);
	}

	Super::EndPlay(EndPlayReason);
}

void AAshesOfHeavenPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// only add IMCs for local player controllers
	if (IsLocalPlayerController())
	{
		// Add Input Mapping Context
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
			{
				if (UInputMappingContext* RuntimeContext = PlatformManager->GetRuntimeInputMappingContext())
				{
					Subsystem->AddMappingContext(RuntimeContext, 100);
				}
			}

			for (UInputMappingContext* CurrentContext : DefaultMappingContexts)
			{
				Subsystem->AddMappingContext(CurrentContext, 0);
			}

			// only add these IMCs if we're not using mobile touch input
			if (!ShouldUseTouchControls())
			{
				for (UInputMappingContext* CurrentContext : MobileExcludedMappingContexts)
				{
					Subsystem->AddMappingContext(CurrentContext, 0);
				}
			}
		}
	}
	
}

bool AAshesOfHeavenPlayerController::ShouldUseTouchControls() const
{
	if (const UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
	{
		return PlatformManager->ShouldUseTouchControls() || bForceTouchControls;
	}

	return SVirtualJoystick::ShouldDisplayTouchInterface() || bForceTouchControls;
}

void AAshesOfHeavenPlayerController::HandleApplicationSuspended()
{
	if (!IsLocalController())
	{
		return;
	}

	bWasPausedBeforeApplicationSuspended = IsPaused();
	if (!bWasPausedBeforeApplicationSuspended)
	{
		SetPause(true);
	}
}

void AAshesOfHeavenPlayerController::HandleApplicationResumed()
{
	if (IsLocalController() && !bWasPausedBeforeApplicationSuspended)
	{
		SetPause(false);
	}
	bWasPausedBeforeApplicationSuspended = false;
}
