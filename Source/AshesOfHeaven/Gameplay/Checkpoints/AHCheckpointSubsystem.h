#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "AHCheckpointSubsystem.generated.h"

UCLASS()
class ASHESOFHEAVEN_API UAHCheckpointSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Checkpoint")
	bool CaptureCheckpoint(FName CheckpointId);

	UFUNCTION(BlueprintCallable, Category="Checkpoint")
	bool RestoreLatestCheckpoint();

	/**
	 * Applies a loaded checkpoint state to the live world. Returns false without touching
	 * the player when the checkpoint carries no real progress (the chapter-opening capture)
	 * or when its saved transform is no longer over valid gameplay ground, so a stale or
	 * corrupt save can never teleport a fresh run into void space.
	 */
	bool RestoreFromState(const FAHCombatCheckpointState& State);

	/** Recover to the canonical safe spawn for the restored chapter stage without using stale coordinates. */
	bool RecoverToCanonicalStage();

	/** True when the location is finite, inside the chapter play band, and has blocking ground beneath it. */
	static bool IsCheckpointTransformValid(UWorld* World, const FVector& Location);
	static bool IsCheckpointTransformValid(UWorld* World, EAHChapterStage Stage, const FVector& Location);

	UFUNCTION(BlueprintCallable, Category="Checkpoint")
	void ReloadLatestCheckpoint();

	UFUNCTION(BlueprintPure, Category="Checkpoint")
	bool HasCheckpoint() const;

	UFUNCTION(BlueprintPure, Category="Checkpoint")
	bool IsEncounterCompleted(FName EncounterId) const;

	void MarkEncounterCompleted(FName EncounterId);
	const FAHCombatCheckpointState& GetRuntimeState() const { return RuntimeState; }

private:
	bool LoadState();

	FAHCombatCheckpointState RuntimeState;
};
