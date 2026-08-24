#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/DataAsset.h"
#include "AHEncounterDefinition.generated.h"

/** Lightweight predicted encounter composition used to preload enemy Primary Assets. */
UCLASS(BlueprintType, Const)
class ASHESOFHEAVEN_API UAHEncounterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Identity")
	FName EncounterId = NAME_None;

	/** Repeated for the main body of the encounter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemies")
	FPrimaryAssetId PrimaryEnemy;

	/** Placed at the end of the spawn sequence, matching authored spawn locations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemies")
	TArray<FPrimaryAssetId> AdditionalEnemies;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preload")
	bool bPreloadVisuals = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preload")
	bool bPreloadAudio = true;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	void GetPredictedEnemySet(TArray<FPrimaryAssetId>& OutEnemyIds) const;
	void BuildSpawnSequence(int32 EnemyCount, TArray<FPrimaryAssetId>& OutEnemyIds) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
