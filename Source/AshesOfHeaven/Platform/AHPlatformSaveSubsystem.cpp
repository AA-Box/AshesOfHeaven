// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHPlatformSaveSubsystem.h"
#include "Platform/AHPlatformSettings.h"
#include "Gameplay/WorldState/AHWorldStateSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

FString UAHPlatformSaveSubsystem::GetSaveSlotName() const
{
	return UAHPlatformSettings::Get()->DefaultSaveSlot;
}

UAHSaveGame* UAHPlatformSaveSubsystem::LoadSaveObject() const
{
	UAHSaveGame* SaveObject = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromSlot(GetSaveSlotName(), 0));
	MigrateSaveObject(SaveObject);
	return SaveObject;
}

UAHSaveGame* UAHPlatformSaveSubsystem::GetOrCreateSaveObject() const
{
	if (UAHSaveGame* Existing = LoadSaveObject())
	{
		return Existing;
	}
	return Cast<UAHSaveGame>(UGameplayStatics::CreateSaveGameObject(UAHSaveGame::StaticClass()));
}

bool UAHPlatformSaveSubsystem::MigrateSaveObject(UAHSaveGame* SaveObject)
{
	if (!SaveObject || SaveObject->SaveVersion > AHChapterStateConstants::CurrentSaveVersion)
	{
		return false;
	}
	bool bMigrated = false;
	if (SaveObject->SaveVersion < AHWorldStateConstants::FirstSaveVersion)
	{
		// Older saves had no actor-state table. Campaign/checkpoint data remains valid and
		// every savable actor safely falls back to its authored default.
		SaveObject->WorldState = FAHWorldStateSaveData();
		bMigrated = true;
	}
	if (SaveObject->WorldState.SchemaVersion <= 0)
	{
		SaveObject->WorldState.SchemaVersion = AHWorldStateConstants::CurrentSchemaVersion;
		bMigrated = true;
	}
	if (SaveObject->SaveVersion != AHChapterStateConstants::CurrentSaveVersion)
	{
		SaveObject->SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
		bMigrated = true;
	}
	return bMigrated;
}

void UAHPlatformSaveSubsystem::CaptureLiveWorldState(UAHSaveGame* SaveObject) const
{
	if (!SaveObject)
	{
		return;
	}
	if (UWorld* World = GetWorld())
	{
		if (UAHWorldStateSubsystem* WorldState = World->GetSubsystem<UAHWorldStateSubsystem>())
		{
			SaveObject->WorldState = WorldState->BuildSaveData();
		}
	}
}

bool UAHPlatformSaveSubsystem::SaveCheckpoint(FName CheckpointId, float CampaignProgress, const FString& MapName, int32 Difficulty)
{
	UAHSaveGame* SaveObject = GetOrCreateSaveObject();
	if (!SaveObject)
	{
		return false;
	}

	SaveObject->CheckpointId = CheckpointId;
	SaveObject->CampaignProgress = FMath::Clamp(CampaignProgress, 0.0f, 1.0f);
	SaveObject->MapName = MapName;
	SaveObject->Difficulty = Difficulty;
	SaveObject->CombatState.bValid = false;
	SaveObject->SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
	CaptureLiveWorldState(SaveObject);
	return UGameplayStatics::SaveGameToSlot(SaveObject, GetSaveSlotName(), 0);
}

bool UAHPlatformSaveSubsystem::SaveCombatCheckpoint(const FAHCombatCheckpointState& State)
{
	UAHSaveGame* SaveObject = GetOrCreateSaveObject();
	if (!SaveObject)
	{
		return false;
	}

	SaveObject->CheckpointId = State.CheckpointId;
	SaveObject->MapName = State.MapName;
	SaveObject->SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
	const bool bChapterOneState = State.MapName.Contains(TEXT("ChapterOne"), ESearchCase::IgnoreCase)
		|| State.CheckpointId.ToString().StartsWith(TEXT("Ch01_"))
		|| State.ChapterState.CompletedSections.Num() > 0;
	const int32 ObjectiveCount = bChapterOneState ? AHChapterStateConstants::ObjectiveCount : 5;
	SaveObject->CampaignProgress = FMath::Clamp(static_cast<float>(State.ObjectiveIndex) / static_cast<float>(ObjectiveCount), 0.0f, 1.0f);
	SaveObject->CombatState = State;
	SaveObject->CombatState.bValid = true;
	CaptureLiveWorldState(SaveObject);
	return UGameplayStatics::SaveGameToSlot(SaveObject, GetSaveSlotName(), 0);
}

bool UAHPlatformSaveSubsystem::LoadCombatCheckpoint(FAHCombatCheckpointState& State) const
{
	if (const UAHSaveGame* SaveObject = LoadSaveObject())
	{
		State = SaveObject->CombatState;
		return State.bValid;
	}
	return false;
}

int32 UAHPlatformSaveSubsystem::GetDifficulty() const
{
	if (const UAHSaveGame* SaveObject = LoadSaveObject())
	{
		return SaveObject->Difficulty;
	}
	return 1;
}

bool UAHPlatformSaveSubsystem::LoadWorldState(FAHWorldStateSaveData& State) const
{
	if (const UAHSaveGame* SaveObject = LoadSaveObject())
	{
		State = SaveObject->WorldState;
		return true;
	}
	return false;
}

bool UAHPlatformSaveSubsystem::ResetProgress()
{
	if (UWorld* World = GetWorld())
	{
		if (UAHWorldStateSubsystem* WorldState = World->GetSubsystem<UAHWorldStateSubsystem>())
		{
			WorldState->ResetWorldState();
		}
	}
	return !HasSave() || UGameplayStatics::DeleteGameInSlot(GetSaveSlotName(), 0);
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

	UAHSaveGame* SaveObject = GetOrCreateSaveObject();
	if (!SaveObject)
	{
		return;
	}
	if (SaveObject->CheckpointId == NAME_None)
	{
		SaveObject->CheckpointId = FName(TEXT("AppSuspension"));
	}
	if (CurrentMap != TEXT("Unknown") || SaveObject->MapName.IsEmpty())
	{
		SaveObject->MapName = CurrentMap;
	}
	SaveObject->SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
	CaptureLiveWorldState(SaveObject);
	UGameplayStatics::SaveGameToSlot(SaveObject, GetSaveSlotName(), 0);
}
