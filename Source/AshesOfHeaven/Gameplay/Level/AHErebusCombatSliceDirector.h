#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Weapons/AHWeaponPickup.h"
#include "AHErebusCombatSliceDirector.generated.h"

class AAHCombatEncounter;
class AAHCombatantCharacter;
class UStaticMesh;
class UMaterialInterface;

UCLASS()
class ASHESOFHEAVEN_API AAHErebusCombatSliceDirector : public AActor
{
	GENERATED_BODY()

public:
	AAHErebusCombatSliceDirector();

	virtual void BeginPlay() override;

protected:
	void BuildBlockout();
	void BuildMissionActors();
	void SpawnBlock(const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator);
	AAHCombatEncounter* SpawnEncounter(FName Id, const FVector& Location, int32 Count, FName ObjectiveOnComplete, const TArray<FVector>& Spawns);
	void SpawnPickup(const FVector& Location, EAHResourcePickupType Type, const FGuid& PersistentId, int32 Amount = 36);
	void SpawnCheckpoint(const FVector& Location, FName Id);
	void SpawnObjectiveZone(const FVector& Location, const FVector& Extent, FName Id);
	void SpawnFriendly(const FVector& Location);

	UFUNCTION()
	void HandleObjectiveChanged(FText Objective, int32 Index, int32 Count);

	UFUNCTION()
	void HandleEncounterComplete(FName EncounterId);

	TObjectPtr<UStaticMesh> BlockMesh;
	TObjectPtr<UMaterialInterface> BlockMaterial;
	TObjectPtr<AAHCombatEncounter> FirstEncounter;
	TObjectPtr<AAHCombatEncounter> SecondEncounter;
	TObjectPtr<AAHCombatEncounter> FinalEncounter;
};
