#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "AHEncounterTypes.generated.h"

UENUM(BlueprintType)
enum class EAHEncounterDifficulty : uint8
{
	Story,
	Soldier,
	Veteran,
	Damnation
};

UENUM(BlueprintType)
enum class EAHEncounterPhaseTrigger : uint8
{
	Immediate,
	ForceRemainingRatio,
	PreviousPhaseCleared,
	ScriptedTrigger
};

UENUM(BlueprintType)
enum class EAHEncounterCompletionRule : uint8
{
	AllPhasesAndEnemiesDefeated,
	ObjectiveCompleted,
	ScriptedTrigger
};

UENUM(BlueprintType)
enum class EAHEncounterLOSRule : uint8
{
	Any,
	HiddenFromPlayer,
	VisibleToPlayer
};

UENUM(BlueprintType, meta=(Bitflags, UseEnumValuesAsMaskValuesInEditor="true"))
enum class EAHEncounterDirection : uint8
{
	None = 0,
	North = 1 << 0,
	East = 1 << 1,
	South = 1 << 2,
	West = 1 << 3
};
ENUM_CLASS_FLAGS(EAHEncounterDirection);

/** One authored enemy request within a phase or boss/hero slot. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterSpawnSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FPrimaryAssetId ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=1))
	int32 Count = 1;

	/** Optional region restriction for this slot. Empty uses the phase or encounter regions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TArray<FName> AllowedRegions;
};

/** Weighted archetype entry used only inside the encounter's finite authored spawn plan. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterEnemyPoolEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FPrimaryAssetId ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0.0))
	float Weight = 1.0f;

	/** Negative uses the cost authored on the enemy archetype asset. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	float SpawnCostOverride = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0))
	int32 MinimumPhase = 0;

	/** Zero means the authored phase limit is the only count cap. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0))
	int32 MaximumPerEncounter = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty", meta=(ClampMin=0.0))
	float VeteranWeightMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty", meta=(ClampMin=0.0))
	float DamnationWeightMultiplier = 1.0f;
};

/** An approved tactical spawn volume. It is an allow-list, not a hint. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterSpawnRegion
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName RegionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FVector Center = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FVector Extent = FVector(500.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	EAHEncounterDirection Direction = EAHEncounterDirection::North;

	bool Contains(const FVector& Location) const
	{
		return FBox::BuildAABB(Center, Extent.GetAbs()).IsInsideOrOn(Location);
	}
};

/** One finite authored reinforcement phase. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterPhaseDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName PhaseId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	EAHEncounterPhaseTrigger Trigger = EAHEncounterPhaseTrigger::Immediate;

	/** Fraction of the opening force still active that arms this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0.0, ClampMax=1.0))
	float ForceRemainingRatio = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	FName ScriptedTriggerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0.0))
	float BonusCredits = 0.0f;

	/** Delay in seconds after the phase trigger becomes true. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0.0))
	float ReinforcementDelay = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TArray<FAHEncounterSpawnSlot> FixedComposition;

	/** Allows weighted selection after fixed slots, bounded by MaximumSpawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	bool bFillFromEnemyPool = false;

	/** Total enemies this phase may request. Zero means exactly the fixed composition. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(ClampMin=0))
	int32 MaximumSpawns = 0;

	/** Empty uses every region approved by the encounter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter")
	TArray<FName> AllowedRegions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Encounter", meta=(Bitmask, BitmaskEnum="/Script/AshesOfHeaven.EAHEncounterDirection"))
	int32 AllowedDirections = static_cast<int32>(EAHEncounterDirection::North)
		| static_cast<int32>(EAHEncounterDirection::East)
		| static_cast<int32>(EAHEncounterDirection::South)
		| static_cast<int32>(EAHEncounterDirection::West);
};

/** Composition, cadence, and AI-pressure changes for one campaign difficulty. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterDifficultyModifier
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty")
	EAHEncounterDifficulty Difficulty = EAHEncounterDifficulty::Soldier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty", meta=(ClampMin=0.0))
	float TacticalBudgetMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty", meta=(ClampMin=0.1))
	float ReinforcementDelayMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty")
	int32 MaximumActiveEnemyDelta = 0;

	/** Multiplies decision quality and fire-discipline tuning, never health or damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Difficulty", meta=(ClampMin=0.1))
	float AISophisticationMultiplier = 1.0f;
};

/** Per-archetype count retained at an encounter phase boundary. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterSpawnCount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FPrimaryAssetId ArchetypeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 Count = 0;
};

/** Deterministic phase-boundary state stored in a campaign checkpoint. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEncounterCheckpointState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FName EncounterId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 PhaseIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 DeterministicSeed = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 RandomDrawCountAtPhaseStart = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	float CreditsAtPhaseStart = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	float TotalSpentBeforePhase = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 TotalSpawnedBeforePhase = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 OpeningForceSize = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TArray<FAHEncounterSpawnCount> SpawnCountsBeforePhase;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TArray<FName> ScriptedTriggers;
};
