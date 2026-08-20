// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHPlatformSaveSubsystem.h"
#include "Platform/AHPlatformSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

FString UAHPlatformSaveSubsystem::GetSaveSlotName() const
{
	return UAHPlatformSettings::Get()->DefaultSaveSlot;
}

UAHSaveGame* UAHPlatformSaveSubsystem::LoadSaveObject() const
{
	return Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(), 0));
}

bool UAHPlatformSaveSubsystem::SaveCheckpoint(FName CheckpointId, float CampaignProgress, const FString& MapName, int32 Difficulty)
{
	UAHSaveGame* SaveObject = LoadSaveObject();
	if (!SaveObject)
	{
		SaveObject = Cast<UAHSaveGame>(UGameplayStatics::CreateSaveGameObject(UAHSaveGame::StaticClass()));
	}
	if (!SaveObject)
	{
		return false;
	}

	SaveObject->CheckpointId = CheckpointId;
	SaveObject->CampaignProgress = FMath::Clamp(CampaignProgress, 0.0f, 1.0f);
	SaveObject->MapName = MapName;
	SaveObject->Difficulty = Difficulty;
	return UGameplayStatics::SaveGameToSlot(SaveObject, GetSaveSlotName(), 0);
}

bool UAHPlatformSaveSubsystem::LoadCheckpoint(FName& CheckpointId, float& CampaignProgress, FString& MapName, int32& Difficulty)
{
	if (const UAHSaveGame* SaveObject = LoadSaveObject())
	{
		CheckpointId = SaveObject->CheckpointId;
		CampaignProgress = SaveObject->CampaignProgress;
		MapName = SaveObject->MapName;
		Difficulty = SaveObject->Difficulty;
		return true;
	}

	return false;
}

bool UAHPlatformSaveSubsystem::HasSave() const
{
	return UGameplayStatics::DoesSaveGameExist(GetSaveSlotName(), 0);
}

void UAHPlatformSaveSubsystem::SaveSuspensionCheckpoint()
{
	FString CurrentMap = TEXT("Unknown");
	if (const UWorld* World = GetWorld())
	{
		CurrentMap = World->GetMapName();
	}

	FName ExistingCheckpoint = NAME_None;
	float ExistingProgress = 0.0f;
	int32 ExistingDifficulty = 1;
	FString ExistingMap;
	LoadCheckpoint(ExistingCheckpoint, ExistingProgress, ExistingMap, ExistingDifficulty);

	SaveCheckpoint(
		ExistingCheckpoint == NAME_None ? FName(TEXT("AppSuspension")) : ExistingCheckpoint,
		ExistingProgress,
		CurrentMap == TEXT("Unknown") && !ExistingMap.IsEmpty() ? ExistingMap : CurrentMap,
		ExistingDifficulty);
}

