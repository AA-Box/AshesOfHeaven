#pragma once

#include "CoreMinimal.h"
#include "AHCorpseLifecycleTypes.generated.h"

/** Runtime stages for a managed combatant body. */
UENUM(BlueprintType)
enum class EAHCorpseLifecycleState : uint8
{
	Alive,
	DeathReaction,
	ActiveCorpse,
	SettledCorpse,
	ReducedCostCorpse,
	EligibleForCleanup,
	RemovedOrRecycled
};

/** Platform-owned corpse population, timing, and cleanup-scoring profile. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHCorpseBudget
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Population", meta=(ClampMin=0))
	int32 SoftLimit = 24;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Population", meta=(ClampMin=0))
	int32 HardLimit = 30;

	/** Ordinary bodies remain for at least this many seconds under soft-budget pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing", meta=(ClampMin=0.0))
	float MinimumLifetimeSeconds = 18.0f;

	/** Hard-cap pressure may bypass the normal lifetime, but never this short grace period. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing", meta=(ClampMin=0.0))
	float EmergencyMinimumLifetimeSeconds = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing", meta=(ClampMin=0.0))
	float DeathReactionSeconds = 1.0f;

	/** Physics is allowed to find a stable pose before sleep is considered authoritative. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing", meta=(ClampMin=0.0))
	float SettleDelaySeconds = 2.0f;

	/** Ragdoll simulation is forced to settle after this many seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing", meta=(ClampMin=0.0))
	float MaximumRagdollSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Timing", meta=(ClampMin=0.0))
	float ReducedCostDelaySeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visibility", meta=(ClampMin=0.0))
	float RecentlyRenderedGraceSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Visibility", meta=(ClampMin=0.0, ClampMax=45.0))
	float ViewPaddingDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cleanup", meta=(ClampMin=0.01))
	float CleanupIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cleanup", meta=(ClampMin=1))
	int32 MaximumCleanupPerPass = 4;

	/** Distance in centimetres that contributes one full unit to distance scoring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scoring", meta=(ClampMin=1.0))
	float DistanceScoreScale = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scoring", meta=(ClampMin=0.0))
	float AgeScoreWeight = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scoring", meta=(ClampMin=0.0))
	float DistanceScoreWeight = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scoring", meta=(ClampMin=0.0))
	float OffscreenScoreWeight = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scoring", meta=(ClampMin=0.0))
	float ImportanceScoreWeight = 6.0f;

	void Sanitize();
};

/** Snapshot used to make and test one cleanup decision. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHCorpseCleanupEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	float AgeSeconds = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	float DistanceToClosestPlayer = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Corpse", meta=(ClampMin=0.0, ClampMax=1.0))
	float Importance = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bAllowCleanup = true;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bPersistent = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bNarrative = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bObjectiveCritical = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bScriptedCivilian = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bCurrentlyVisible = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bRecentlyRendered = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bTargetedInteractable = false;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	bool bHasImportantLoot = false;

	/** Monotonic registration order; lower values are older when scores tie. */
	uint64 QueueSequence = 0;
};

/** Current corpse costs exposed to profiling and automated verification. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHCorpsePerformanceStats
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 ManagedCorpses = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 OrdinaryCorpses = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 ActiveRagdolls = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 ReducedCostCorpses = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 ManagedSkeletalMeshes = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 SimulatingSkeletalMeshes = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 AwakePhysicsBodies = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 TickingActors = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 TickingSkeletalMeshes = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 TickingWeapons = 0;

	/** Exclusive resource bytes reported by managed actors and components; shared assets are excluded. */
	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int64 EstimatedExclusiveMemoryBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 TotalRegistered = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 TotalSettled = 0;

	UPROPERTY(BlueprintReadOnly, Category="Corpse")
	int32 TotalRemovedOrRecycled = 0;
};

/** Actor tags understood alongside the equivalent editable combatant flags. */
struct ASHESOFHEAVEN_API FAHCorpseTags
{
	static const FName Persistent;
	static const FName Narrative;
	static const FName Lootable;
	static const FName AllowCleanup;
	static const FName ObjectiveCritical;
	static const FName ScriptedCivilian;
};
