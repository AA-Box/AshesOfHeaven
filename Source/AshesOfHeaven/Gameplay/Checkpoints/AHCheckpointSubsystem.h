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
