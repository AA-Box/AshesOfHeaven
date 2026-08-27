// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/WorldState/AHWorldStateTypes.h"
#include "AHPlatformSaveSubsystem.generated.h"

/** Logical campaign state. Unreal resolves the physical save location per platform. */
UCLASS()
class ASHESOFHEAVEN_API UAHSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	int32 SaveVersion = AHChapterStateConstants::CurrentSaveVersion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FString MapName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	FName CheckpointId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	float CampaignProgress = 0.0f;

	/**
	 * Chapters the player has finished, written when the chapter actually completes rather
	 * than derived from CombatState. Checkpoint capture is spatial and refuses or normalizes
	 * states the restore path rejects, so completion recorded only there is lost on relaunch.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Campaign")
	TArray<FName> CompletedChapters;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	TArray<FName> Collectibles;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Save")
	int32 Difficulty = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Accessibility")
	bool bSubtitlesEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Accessibility")
	float SubtitleFontScale = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Accessibility")
	float SubtitleBackgroundOpacity = 0.80f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Combat")
	FAHCombatCheckpointState CombatState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	FAHWorldStateSaveData WorldState;
};

/** Platform-neutral save API for checkpoints, settings, and suspend protection. */
UCLASS()
class ASHESOFHEAVEN_API UAHPlatformSaveSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Save")
	bool SaveCheckpoint(FName CheckpointId, float CampaignProgress, const FString& MapName, int32 Difficulty = 1);

	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Save")
	bool LoadCheckpoint(FName& CheckpointId, float& CampaignProgress, FString& MapName, int32& Difficulty);

	bool SaveCombatCheckpoint(const FAHCombatCheckpointState& State);

	/**
	 * Records campaign completion on disk immediately, independently of checkpoint capture,
	 * so finishing the chapter survives quitting and relaunching the process.
	 */
	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Save")
	bool MarkChapterComplete(FName ChapterId);

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Save")
	bool IsChapterComplete(FName ChapterId) const;
	bool LoadCombatCheckpoint(FAHCombatCheckpointState& State) const;
	int32 GetDifficulty() const;
	bool LoadWorldState(FAHWorldStateSaveData& State) const;

	/** Upgrades compatible legacy data in memory; unknown future versions are left untouched. */
	static bool MigrateSaveObject(UAHSaveGame* SaveObject);

	/** Development-only clean-run support; callers decide whether exposing it is appropriate. */
	bool ResetProgress();

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Save")
	bool HasSave() const;

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Save")
	FString GetSaveSlotName() const;

	/** Called by the platform manager before mobile OS termination can occur. */
	void SaveSuspensionCheckpoint();

private:
	UAHSaveGame* LoadSaveObject() const;
	UAHSaveGame* GetOrCreateSaveObject() const;
	void CaptureLiveWorldState(UAHSaveGame* SaveObject) const;
};
