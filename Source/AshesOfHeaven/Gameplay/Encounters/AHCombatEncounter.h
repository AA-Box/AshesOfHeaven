#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHCombatEncounter.generated.h"

class UBoxComponent;
class AAHCombatantCharacter;
class AAHVeilPilgrimCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHEncounterCompleteDelegate, FName, EncounterId);

UCLASS()
class ASHESOFHEAVEN_API AAHCombatEncounter : public AActor
{
	GENERATED_BODY()

public:
	AAHCombatEncounter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> ActivationVolume;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName EncounterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TSubclassOf<AAHCombatantCharacter> EnemyClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TArray<TSubclassOf<AAHCombatantCharacter>> AdditionalEnemyClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	int32 EnemyCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TArray<FVector> SpawnLocations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName ObjectiveOnComplete = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	bool bActivateOnPlayerOverlap = true;

	UPROPERTY(BlueprintAssignable, Category="Encounter")
	FAHEncounterCompleteDelegate OnEncounterComplete;

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Encounter")
	void ActivateEncounter();

	UFUNCTION(BlueprintPure, Category="Encounter")
	bool IsActive() const { return bActive; }

	UFUNCTION(BlueprintPure, Category="Encounter")
	bool IsComplete() const { return bComplete; }

	/** Enemies this encounter is still waiting on. Zero is what completes it. */
	UFUNCTION(BlueprintPure, Category="Encounter")
	int32 GetActiveEnemyCount() const { return ActiveEnemies.Num(); }

protected:
	UFUNCTION()
	void OnActivationOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEnemyDied();

	UFUNCTION()
	void OnEnemyDestroyed(AActor* DestroyedActor);

	void CompleteEncounter();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAHCombatantCharacter>> ActiveEnemies;

	bool bActive = false;
	bool bComplete = false;
};
