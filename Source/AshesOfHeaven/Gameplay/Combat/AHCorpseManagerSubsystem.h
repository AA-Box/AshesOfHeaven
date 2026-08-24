#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCorpseLifecycleTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "AHCorpseManagerSubsystem.generated.h"

class AAHCombatantCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAHCorpseStateChangedDelegate, AAHCombatantCharacter*, Corpse, EAHCorpseLifecycleState, PreviousState, EAHCorpseLifecycleState, NewState);

/**
 * Keeps combat aftermath within the active platform budget. Bodies settle and become cheap in
 * place; only ordinary, off-camera, non-critical corpses participate in scored cleanup.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHCorpseManagerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Begins lifecycle management for a dead combatant. Re-registering the same actor is harmless. */
	void RegisterCorpse(AAHCombatantCharacter* Corpse);

	/** Removes all ordinary runtime bodies while retaining authored or explicitly preserved bodies. */
	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Corpse")
	void ResetOrdinaryCorpsesForCheckpoint();

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Corpse")
	EAHCorpseLifecycleState GetCorpseState(const AAHCombatantCharacter* Corpse) const;

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Corpse")
	FAHCorpsePerformanceStats GetPerformanceStats() const;

	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Corpse")
	void LogPerformanceStats() const;

	const FAHCorpseBudget& GetBudget() const { return Budget; }

	/** Preservation gate shared by runtime selection and deterministic automation coverage. */
	static bool CanCleanupCandidate(const FAHCorpseCleanupEvaluation& Evaluation, const FAHCorpseBudget& InBudget, bool bHardCapPressure);
	static float CalculateCleanupScore(const FAHCorpseCleanupEvaluation& Evaluation, const FAHCorpseBudget& InBudget);
	static int32 SelectCleanupCandidate(const TArray<FAHCorpseCleanupEvaluation>& Evaluations, const FAHCorpseBudget& InBudget, bool bHardCapPressure);

	UPROPERTY(BlueprintAssignable, Category="Ashes of Heaven|Corpse")
	FAHCorpseStateChangedDelegate OnCorpseStateChanged;

#if WITH_DEV_AUTOMATION_TESTS
	void SetBudgetForTesting(const FAHCorpseBudget& InBudget);
	void ProcessLifecycleForTesting(float CurrentTimeSeconds);
#endif

private:
	struct FManagedCorpse
	{
		TWeakObjectPtr<AAHCombatantCharacter> Actor;
		float DeathTimeSeconds = 0.0f;
		float StateEnteredTimeSeconds = 0.0f;
		EAHCorpseLifecycleState State = EAHCorpseLifecycleState::DeathReaction;
		uint64 QueueSequence = 0;
	};

	void ProcessLifecycle(float CurrentTimeSeconds);
	void AdvanceLifecycle(FManagedCorpse& Entry, float CurrentTimeSeconds);
	void TransitionState(FManagedCorpse& Entry, EAHCorpseLifecycleState NewState, float CurrentTimeSeconds);
	void CleanupUnderPressure(float CurrentTimeSeconds);
	void RemoveCorpseAt(int32 EntryIndex, bool bCheckpointReset);
	void CompactInvalidEntries();

	FAHCorpseCleanupEvaluation BuildEvaluation(const FManagedCorpse& Entry, float CurrentTimeSeconds) const;
	bool IsInAnyPlayerView(const AAHCombatantCharacter* Corpse) const;
	bool IsTargetedInteractable(const AAHCombatantCharacter* Corpse) const;
	float DistanceToClosestPlayer(const AAHCombatantCharacter* Corpse) const;
	bool IsPermanentPreservation(const AAHCombatantCharacter* Corpse) const;
	bool IsOrdinaryCorpse(const AAHCombatantCharacter* Corpse) const;
	int32 CountOrdinaryCorpses() const;

	TArray<FManagedCorpse> CorpseQueue;
	FAHCorpseBudget Budget;
	float CleanupAccumulator = 0.0f;
	float LastHardCapWarningTime = -BIG_NUMBER;
	uint64 NextQueueSequence = 1;
	int32 TotalRegistered = 0;
	int32 TotalSettled = 0;
	int32 TotalRemovedOrRecycled = 0;
};
