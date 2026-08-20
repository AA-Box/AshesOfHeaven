// Copyright Epic Games, Inc. All Rights Reserved.

#include "Variant_Horror/HorrorPlayerController.h"
#include "AshesOfHeavenCameraManager.h"
#include "HorrorCharacter.h"
#include "HorrorUI.h"

AHorrorPlayerController::AHorrorPlayerController()
{
	// Keep the horror camera choice while inheriting shared platform input/lifecycle behavior.
	PlayerCameraManagerClass = AAshesOfHeavenCameraManager::StaticClass();
}

void AHorrorPlayerController::OnPossess(APawn* aPawn)
{
	Super::OnPossess(aPawn);

	if (IsLocalPlayerController())
	{
		if (AHorrorCharacter* HorrorCharacter = Cast<AHorrorCharacter>(aPawn))
		{
			if (!HorrorUI)
			{
				HorrorUI = CreateWidget<UHorrorUI>(this, HorrorUIClass);
				if (HorrorUI)
				{
					HorrorUI->AddToPlayerScreen(0);
				}
			}

			if (HorrorUI)
			{
				HorrorUI->SetupCharacter(HorrorCharacter);
			}
		}
	}
}

