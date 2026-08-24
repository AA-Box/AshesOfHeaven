#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Gameplay/Encounters/AHEncounterTypes.h"
#include "AHEncounterDirectorSubsystem.generated.h"

class AAHCombatantCharacter;
class UAHEncounterDefinition;
class UAHEnemyDefinition;
struct FEnvQueryResult;
struct FStreamableHandle;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHDirectedEncounterDelegate, FName, EncounterId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAHDirectedEncounterPhaseDelegate, FName, EncounterId, FName, PhaseId);

/**
 * Executes finite, authored campaign encounters. Composition, cadence, and EQS placement may
 * vary only within the definition's phase, budget, region, objective, and platform limits.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHEncounterDirectorSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

	/** Loads and begins the authored Primary Asset whose encounter ID matches EncounterId. */
	UFUNCTION(BlueprintCallable, Category="Encounter")
	bool BeginEncounter(FName EncounterId);

	/** Stops tactical execution without completing its associated story objective. */
	UFUNCTION(BlueprintCallable, Category="Encounter")
	void AbortEncounter(FName Reason = NAME_None);

	/** Records an authored story trigger used by phase and completion rules. */
	UFUNCTION(BlueprintCallable, Category="Encounter")
	void NotifyScriptedTrigger(FName TriggerId);

	/** Explicit resolution path for encounters with a scripted completion rule. */
	UFUNCTION(BlueprintCallable, Category="Encounter")
	void ResolveEncounter();

	UFUNCTION(BlueprintPure, Category="Encounter")
	bool IsEncounterActive() const { return ActiveDefinition != nullptr || PendingEncounterId.IsValid(); }

	UFUNCTION(BlueprintPure, Category="Encounter")
	FName GetActiveEncounterId() const;

	UFUNCTION(BlueprintPure, Category="Encounter")
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

	UFUNCTION(BlueprintPure, Category="Encounter")
	float GetCurrentCredits() const { return CurrentCredits; }

	UFUNCTION(BlueprintPure, Category="Encounter")
	int32 GetActiveEnemyCount() const { return ActiveEnemies.Num(); }

	UFUNCTION(BlueprintPure, Category="Encounter")
	int32 GetTotalSpawnedCount() const { return TotalSpawned; }

	UFUNCTION(BlueprintPure, Category="Encounter")
	bool IsActiveEncounterForStage(EAHChapterStage Stage) const;

	UFUNCTION(BlueprintPure, Category="Encounter")
	FAHEncounterCheckpointState CaptureCheckpointState() const;

	/** Queues deterministic restoration; it is applied when the matching definition finishes loading. */
	void RestoreCheckpointState(const FAHEncounterCheckpointState& State);

	/** Development diagnostics used by ah.Encounter.Dump. */
	FString BuildDebugString() const;
	void DumpToLog() const;

	UPROPERTY(BlueprintAssignable, Category="Encounter")
	FAHDirectedEncounterDelegate OnEncounterStarted;

	UPROPERTY(BlueprintAssignable, Category="Encounter")
	FAHDirectedEncounterPhaseDelegate OnEncounterPhaseStarted;

	UPROPERTY(BlueprintAssignable, Category="Encounter")
	FAHDirectedEncounterDelegate OnEncounterCompleted;

	UPROPERTY(BlueprintAssignable, Category="Encounter")
	FAHDirectedEncounterDelegate OnEncounterAborted;

	// Pure policy helpers exercised by automation tests.
	static EAHEncounterDifficulty DifficultyFromSaveValue(int32 SaveValue);
	static float CalculateSpawnCost(const FAHEncounterEnemyPoolEntry& Entry, const UAHEnemyDefinition* Archetype);
	static float CalculateEffectiveWeight(const FAHEncounterEnemyPoolEntry& Entry, EAHEncounterDifficulty Difficulty);
	static bool CanSpend(float Credits, float TotalSpent, float EffectiveBudget, float Cost);
	static int32 SelectWeightedPoolEntry(
		const TArray<FAHEncounterEnemyPoolEntry>& Pool,
		const TMap<FPrimaryAssetId, TObjectPtr<UAHEnemyDefinition>>& Archetypes,
		const TMap<FPrimaryAssetId, int32>& SpawnCounts,
		int32 PhaseIndex,
		EAHEncounterDifficulty Difficulty,
		float Credits,
		float TotalSpent,
		float EffectiveBudget,
		FRandomStream& RandomStream);
	static int32 CalculateActiveEnemyCap(const UAHEncounterDefinition& Definition, const FAHEncounterDifficultyModifier& Modifier, int32 PlatformCap, bool bMobile);
	static bool ShouldTriggerForceRemaining(int32 ActiveEnemies, int32 OpeningForceSize, float RemainingRatio);
	static bool IsQueryResultUsable(bool bSuccessful, int32 LocationCount);
	static bool IsArchetypeAvailable(const UAHEnemyDefinition* Archetype);
	static bool ShouldCompleteEncounter(const UAHEncounterDefinition& Definition, int32 PhaseIndex, bool bPhasePlanComplete, int32 ActiveEnemies, bool bObjectiveComplete, const TSet<FName>& TriggeredScripts);

private:
	struct FPendingSpawn
	{
		FPrimaryAssetId ArchetypeId;
		float Cost = 0.0f;
		TArray<FName> AllowedRegions;
	};

	void HandleEncounterLoaded(FPrimaryAssetId EncounterAssetId, int32 RequestSerial);
	void HandleRosterLoaded(FGuid RequestId, bool bSuccess, const TArray<UAHEnemyDefinition*>& Definitions, const FString& Error, int32 RequestSerial);
	void StartLoadedEncounter();
	void EnterPhase(int32 PhaseIndex, bool bRestoring);
	void BuildFixedSpawnQueue(const FAHEncounterPhaseDefinition& Phase);
	void TryDispatchSpawn();
	bool SelectNextSpawn(FPendingSpawn& OutSpawn);
	void BeginSpawnQuery(const FPendingSpawn& Spawn);
	void HandleSpawnQueryFinished(TSharedPtr<FEnvQueryResult> Result, int32 RequestSerial);
	bool TryResolveSafeSpawnLocation(const FVector& QueryLocation, const TArray<FName>& RegionRestriction, FVector& OutSpawnLocation) const;
	const FAHEncounterSpawnRegion* FindContainingRegion(const FVector& Location, const TArray<FName>& RegionRestriction) const;
	bool SpawnSelectedEnemy(const FVector& Location);
	void ApplyEnemyTacticalProfile(AAHCombatantCharacter* Enemy, const UAHEnemyDefinition& Archetype) const;
	void EvaluatePhaseProgress();
	bool IsNextPhaseTriggerSatisfied(const FAHEncounterPhaseDefinition& NextPhase) const;
	bool IsCurrentPhasePlanComplete() const;
	void CompleteEncounter();
	void ResetRuntime(bool bDestroyActiveEnemies);
	void PersistPhaseBoundary();
	void AdvanceRandomStream(int32 DrawCount);
	int32 DrawRandomIndex(int32 MaximumInclusive);
	EAHEncounterDifficulty ResolveDifficulty() const;
	int32 ResolveActiveEnemyCap() const;
	void ReleasePlatformSlot();

	UFUNCTION()
	void HandleEnemyDied();

	UFUNCTION()
	void HandleEnemyDestroyed(AActor* DestroyedActor);

	UFUNCTION()
	void HandleObjectiveCompleted(FName ObjectiveId);

	UPROPERTY(Transient)
	TObjectPtr<UAHEncounterDefinition> ActiveDefinition;

	UPROPERTY(Transient)
	TMap<FPrimaryAssetId, TObjectPtr<UAHEnemyDefinition>> LoadedArchetypes;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AAHCombatantCharacter>> ActiveEnemies;

	FPrimaryAssetId PendingEncounterId;
	TSharedPtr<FStreamableHandle> PendingLoadHandle;
	FGuid EnemyAssetLeaseId;
	TArray<FPrimaryAssetId> RequestedArchetypeIds;
	TArray<FPendingSpawn> FixedSpawnQueue;
	FPendingSpawn PendingSpawn;
	FVector PendingSpawnLocation = FVector::ZeroVector;
	TMap<FPrimaryAssetId, int32> SpawnCounts;
	TSet<FName> TriggeredScripts;
	FAHEncounterCheckpointState PendingRestoreState;
	FRandomStream RandomStream;
	EAHEncounterDifficulty ActiveDifficulty = EAHEncounterDifficulty::Soldier;
	int32 RequestSerial = 0;
	int32 CurrentPhaseIndex = INDEX_NONE;
	int32 PhaseSpawned = 0;
	int32 OpeningForceSize = 0;
	int32 TotalSpawned = 0;
	int32 RandomDrawCount = 0;
	float CurrentCredits = 0.0f;
	float TotalSpent = 0.0f;
	float EffectiveBudget = 0.0f;
	float NextSpawnAttemptTime = 0.0f;
	float ArmedPhaseStartTime = 0.0f;
	bool bNextPhaseArmed = false;
	bool bQueryPending = false;
	bool bRosterLoadPending = false;
	bool bPlatformQuerySlotHeld = false;
	bool bCompletingObjective = false;
	FPrimaryAssetId LastSelectedArchetype;

	// Known deterministic restart point for the active phase.
	float PhaseStartCredits = 0.0f;
	float PhaseStartSpent = 0.0f;
	int32 PhaseStartTotalSpawned = 0;
	int32 PhaseStartRandomDrawCount = 0;
	TMap<FPrimaryAssetId, int32> PhaseStartSpawnCounts;
};
