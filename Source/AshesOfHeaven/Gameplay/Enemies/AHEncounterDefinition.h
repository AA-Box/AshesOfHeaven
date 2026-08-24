#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/DataAsset.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Encounters/AHEncounterTypes.h"
#include "AHEncounterDefinition.generated.h"

class UEnvQuery;

/** A predicted encounter composition. Counts remain on the encounter actor when level-specific. */
UCLASS(BlueprintType, Const)
class ASHESOFHEAVEN_API UAHEncounterDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Identity")
	FName EncounterId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Encounter")
	EAHChapterStage Stage = EAHChapterStage::OpeningBattle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Encounter")
	FName ObjectiveId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Budget", meta=(ClampMin="0.0"))
	float EnemyBudget = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Budget", meta=(ClampMin="0.0"))
	float StartingCredits = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Budget", meta=(ClampMin="0.0"))
	float CreditRegenerationPerSecond = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Composition")
	TArray<FAHEncounterEnemyPoolEntry> EnemyArchetypePool;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Population", meta=(ClampMin="1"))
	int32 MaximumActiveEnemies = 8;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Population", meta=(ClampMin="1"))
	int32 MobileMaximumActiveEnemies = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Population", meta=(ClampMin="1"))
	int32 MaximumTotalEnemies = 16;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Phases")
	TArray<FAHEncounterPhaseDefinition> Phases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Phases", meta=(ClampMin="0.0"))
	float DefaultReinforcementDelay = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning")
	TObjectPtr<UEnvQuery> SpawnQuery;

	/** Named parameters consumed by the authored EQS template (for example grid radius/spacing). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning")
	TMap<FName, float> SpawnQueryFloatParams;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning")
	TArray<FAHEncounterSpawnRegion> AllowedSpawnRegions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(ClampMin="0.0"))
	float MinimumDistanceFromPlayer = 700.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning")
	EAHEncounterLOSRule LOSRestriction = EAHEncounterLOSRule::HiddenFromPlayer;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Spawning", meta=(Bitmask, BitmaskEnum="/Script/AshesOfHeaven.EAHEncounterDirection"))
	int32 AllowedDirections = static_cast<int32>(EAHEncounterDirection::North)
		| static_cast<int32>(EAHEncounterDirection::East)
		| static_cast<int32>(EAHEncounterDirection::South)
		| static_cast<int32>(EAHEncounterDirection::West);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty")
	TArray<FAHEncounterDifficultyModifier> DifficultyModifiers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Completion")
	EAHEncounterCompletionRule CompletionRule = EAHEncounterCompletionRule::AllPhasesAndEnemiesDefeated;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Completion")
	FName CompletionTriggerId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Scripting")
	TArray<FName> ScriptedTriggers;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Composition")
	TArray<FAHEncounterSpawnSlot> BossHeroSlots;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Determinism")
	int32 DeterministicSeed = 1337;

	/** Repeated for the main body of the encounter. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemies")
	FPrimaryAssetId PrimaryEnemy;

	/** Placed at the end of the spawn sequence, matching the authored spawn locations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Enemies")
	TArray<FPrimaryAssetId> AdditionalEnemies;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preload")
	bool bPreloadVisuals = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Preload")
	bool bPreloadAudio = true;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	void GetPredictedEnemySet(TArray<FPrimaryAssetId>& OutEnemyIds) const;
	void BuildSpawnSequence(int32 EnemyCount, TArray<FPrimaryAssetId>& OutEnemyIds) const;
	const FAHEncounterDifficultyModifier& GetDifficultyModifier(EAHEncounterDifficulty Difficulty) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
