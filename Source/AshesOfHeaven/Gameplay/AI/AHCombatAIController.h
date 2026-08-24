#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Gameplay/AI/AHTacticalPositionTypes.h"
#include "AHCombatAIController.generated.h"

class UAIPerceptionComponent;
class AAHCombatantCharacter;

UCLASS()
class ASHESOFHEAVEN_API AAHCombatAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAHCombatAIController();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="AI")
	TObjectPtr<UAIPerceptionComponent> AIPerception;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
	float SightRange = 2600.0f;

	/** 1.0 lands every shot on the target point; 0.0 scatters across MaxAimErrorDegrees.
	 * This was declared and never read, so every AI fired with the weapon's 1-degree mechanical
	 * spread and nothing else - a five-man squad killed the player in well under a second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin=0.0, ClampMax=1.0))
	float Accuracy = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin=0.0))
	float MaxAimErrorDegrees = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
	bool bPreferCover = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Tactics")
	EAHAITacticalDifficulty TacticalDifficulty = EAHAITacticalDifficulty::Regular;

	/** Chance that a suppression attacker spends a reposition cycle flanking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Tactics", meta=(ClampMin=0.0, ClampMax=1.0))
	float BaseFlankWillingness = 0.22f;

	/** Distance the combatant tries to fight from. It closes to this and no further. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin=200.0))
	float PreferredEngagementRange = 1100.0f;

	/** Inside this it breaks contact instead of standing on top of its target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin=100.0))
	float MinimumEngagementRange = 650.0f;

	/** Rounds sent before the trigger comes off. Every combatant held full auto down until the
	 * magazine ran dry, which is a ~65% duty cycle and roughly 180 damage a second per rifle at
	 * the player's 200 effective health. Shooters fire in bursts; this is that. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire Discipline", meta=(ClampMin=1))
	int32 BurstRounds = 4;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire Discipline", meta=(ClampMin=0.0))
	float MinBurstPause = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire Discipline", meta=(ClampMin=0.0))
	float MaxBurstPause = 1.6f;

	/** How many combatants aim to kill at one target at a time. Everyone else keeps firing at
	 * SuppressionSpreadDegrees so the fight still sounds like a fight. This is the combat-ring
	 * rule: incoming damage stops scaling with squad size and starts being a design number. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire Discipline", meta=(ClampMin=1))
	int32 MaxSimultaneousAttackers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire Discipline", meta=(ClampMin=0.0))
	float SuppressionSpreadDegrees = 14.0f;

	/** Wide fire for this long after first sighting a target, so contact opens with rounds going
	 * past the player rather than into them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI|Fire Discipline", meta=(ClampMin=0.0))
	float FirstContactGraceSeconds = 1.25f;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	void ReactToGrenade(const FVector& GrenadeLocation, float Radius);
	void DebugDrawAI() const;

	UFUNCTION(BlueprintPure, Category="AI")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="AI")
	FVector GetLastKnownLocation() const { return LastKnownLocation; }

	UFUNCTION(BlueprintPure, Category="AI|Tactics")
	EAHTacticalIntent GetTacticalIntent() const { return CurrentTacticalIntent; }

	UFUNCTION(BlueprintPure, Category="AI|Tactics")
	FVector GetCachedTacticalLocation() const { return CachedTacticalLocation; }

	UFUNCTION(BlueprintPure, Category="AI|Tactics")
	bool HasTacticalQueryInFlight() const { return bTacticalQueryInFlight; }

	int32 GetTacticalQueryRequestCount() const { return TacticalQueryRequestCount; }

protected:
	UFUNCTION()
	void HandlePawnDeath();

	void UpdateTarget();
	void UpdateCombatBehavior(float DeltaSeconds);
	AActor* FindBestTarget() const;
	bool HasLineOfSightTo(AActor* Target) const;
	void MoveWithFallback(const FVector& Destination, float DeltaSeconds, bool bForceNewRequest = false);
	/** Keeps the weapon fed. A combatant that runs dry and never reloads just stands there. */
	void MaintainWeapon();
	FVector GetStandoffLocation(const FVector& TargetLocation, float Range) const;
	void FaceLocation(const FVector& Target, float DeltaSeconds);
	/** Decides whether this combatant is one of the MaxSimultaneousAttackers on its target. */
	void UpdateAttackSlot();
	void ApplyAimDiscipline(float Now);
	void UpdateBurstFire(float Now);
	EAHTacticalIntent ChooseRepositionIntent() const;
	void SetTacticalIntent(EAHTacticalIntent NewIntent, float LifetimeSeconds = 0.0f);
	void ExecuteTacticalMovement(EAHTacticalIntent Intent, AActor* Target, float DeltaSeconds);
	void RequestTacticalPosition(EAHTacticalIntent Intent, AActor* Target);
	void HandleTacticalQueryFinished(const FAHTacticalPositionResult& Result);
	FVector BuildTacticalFallback(EAHTacticalIntent Intent, AActor* Target) const;
	float GetTacticalQueryInterval() const;
	float GetTacticalQualityTolerance() const;
	float GetFlankWillingness() const;
	float GetGrenadeReactionDelay() const;
	float GetEffectiveAccuracy() const;
	float GetBurstPauseScale() const;
	bool IsExpensiveTacticalQueryAllowed(AActor* Target) const;

	TWeakObjectPtr<AAHCombatantCharacter> Combatant;
	TWeakObjectPtr<AActor> CurrentTarget;
	FVector LastKnownLocation = FVector::ZeroVector;
	FVector GrenadeThreatLocation = FVector::ZeroVector;
	float GrenadeThreatRadius = 0.0f;
	float GrenadeThreatExpiryTime = -BIG_NUMBER;
	float GrenadeReactionReadyTime = -BIG_NUMBER;
	/** Goal of the path request currently in flight, so it is not restarted every tick. */
	FVector CurrentMoveGoal = FVector::ZeroVector;
	/** Sweep point while hunting a lost target; re-rolled on a timer, never per frame. */
	FVector SearchLocation = FVector::ZeroVector;
	FVector CachedTacticalLocation = FVector::ZeroVector;
	float CachedTacticalScore = 0.0f;
	EAHTacticalIntent CachedTacticalIntent = EAHTacticalIntent::Hold;
	EAHTacticalIntent CurrentTacticalIntent = EAHTacticalIntent::Hold;
	float TacticalIntentEndTime = -BIG_NUMBER;
	float NextTacticalQueryTime = 0.0f;
	int32 TacticalQueryRequestCount = 0;
	float LastSeenTime = -BIG_NUMBER;
	float NextDecisionTime = 0.0f;
	float NextShotTime = 0.0f;
	float BurstEndTime = 0.0f;
	/** World time this combatant last opened on its current target; negative means no contact. */
	float FirstContactTime = -BIG_NUMBER;
	bool bAttackSlotHeld = true;
	float NextRepositionTime = 0.0f;
	float NextSearchTime = 0.0f;
	bool bHasSeenTarget = false;
	bool bInvestigating = false;
	bool bHasCachedTacticalLocation = false;
	bool bCachedTacticalFallback = false;
	bool bTacticalQueryInFlight = false;
};
