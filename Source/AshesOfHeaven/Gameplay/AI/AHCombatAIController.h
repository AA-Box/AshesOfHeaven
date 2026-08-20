#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI", meta=(ClampMin=0.0, ClampMax=1.0))
	float Accuracy = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="AI")
	bool bPreferCover = true;

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	void ReactToGrenade(const FVector& GrenadeLocation, float Radius);
	void DebugDrawAI() const;

	UFUNCTION(BlueprintPure, Category="AI")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="AI")
	FVector GetLastKnownLocation() const { return LastKnownLocation; }

protected:
	UFUNCTION()
	void HandlePawnDeath();

	void UpdateTarget();
	void UpdateCombatBehavior(float DeltaSeconds);
	AActor* FindBestTarget() const;
	bool HasLineOfSightTo(AActor* Target) const;
	FVector ChooseCoverLocation(AActor* Target) const;
	void MoveWithFallback(const FVector& Destination, float DeltaSeconds);

	TWeakObjectPtr<AAHCombatantCharacter> Combatant;
	TWeakObjectPtr<AActor> CurrentTarget;
	FVector LastKnownLocation = FVector::ZeroVector;
	FVector EscapeLocation = FVector::ZeroVector;
	float LastSeenTime = -BIG_NUMBER;
	float NextDecisionTime = 0.0f;
	float NextShotTime = 0.0f;
	float NextRepositionTime = 0.0f;
	bool bHasSeenTarget = false;
	bool bInvestigating = false;
};
