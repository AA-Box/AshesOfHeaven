#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "GameFramework/Actor.h"
#include "AHCombatEncounter.generated.h"

class UBoxComponent;
class AAHCombatantCharacter;
class UAHEnemyDefinition;

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

	/** Lightweight AHEncounter ID; it contains enemy IDs, never hard enemy class references. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FPrimaryAssetId EncounterDefinitionId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	int32 EnemyCount = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TArray<FVector> SpawnLocations;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName ObjectiveOnComplete = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	bool bActivateOnPlayerOverlap = true;

	/** Starts prediction at BeginPlay so first contact never owns the first heavy load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	bool bPreloadOnBeginPlay = true;

	UPROPERTY(BlueprintAssignable, Category="Encounter")
	FAHEncounterCompleteDelegate OnEncounterComplete;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Encounter")
	void ActivateEncounter();

	UFUNCTION(BlueprintCallable, Category="Encounter")
	void PreloadEncounterAssets();

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
	void SpawnLoadedEnemies();
	void HandleAssetsReady(FGuid RequestId, bool bSuccess, const TArray<UAHEnemyDefinition*>& Definitions, const FString& Error);
	void ReleaseAssetLease();

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAHCombatantCharacter>> ActiveEnemies;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UAHEnemyDefinition>> LoadedEnemyDefinitions;

	bool bActive = false;
	bool bComplete = false;
	bool bActivationRequested = false;
	bool bPreloadStarted = false;
	bool bAssetsReady = false;
	FGuid AssetLease;
};
