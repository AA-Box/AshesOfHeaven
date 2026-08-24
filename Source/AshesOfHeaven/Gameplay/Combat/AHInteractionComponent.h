#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/OverlapResult.h"
#include "AHInteractionComponent.generated.h"

UINTERFACE(BlueprintType)
class ASHESOFHEAVEN_API UAHInteractable : public UInterface
{
	GENERATED_BODY()
};

class ASHESOFHEAVEN_API IAHInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(AActor* Interactor);

	/** Empty means the actor has nothing actionable in its current state. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	FText GetInteractionPrompt() const;

	/** Actor-authored preference among otherwise comparable interactions. Zero is neutral; use -1 to 1. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	float GetInteractionPriority() const;
	virtual float GetInteractionPriority_Implementation() const;

	/** Mission relevance, authored separately so objectives can be tuned without a class hierarchy. Use 0 to 1. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	float GetObjectiveInteractionPriority() const;
	virtual float GetObjectiveInteractionPriority_Implementation() const;
};

/** Input values shared by the runtime selector and its deterministic automation tests. */
struct ASHESOFHEAVEN_API FAHInteractionCandidateMetrics
{
	bool bActionable = true;
	bool bVisible = true;
	bool bDirectHit = false;
	bool bCurrentTarget = false;
	bool bOnScreen = true;
	float Distance = 0.0f;
	float MaxDistance = 1.0f;
	float ViewDot = 1.0f;
	float MinimumViewDot = 0.0f;
	float ScreenCenterDistance = 0.0f;
	float InteractionPriority = 0.0f;
	float ObjectivePriority = 0.0f;
};

/** Tunable score weights; priority values are supplied by each interactable. */
struct ASHESOFHEAVEN_API FAHInteractionScoreWeights
{
	float DirectHitBonus = 1000.0f;
	float DistanceWeight = 150.0f;
	float AngleWeight = 200.0f;
	float ScreenCenterWeight = 150.0f;
	float VisibilityWeight = 100.0f;
	float InteractionPriorityWeight = 100.0f;
	float ObjectivePriorityWeight = 200.0f;
	float PersistenceBonus = 45.0f;
};

namespace AHInteractionTargeting
{
	ASHESOFHEAVEN_API float ScoreCandidate(const FAHInteractionCandidateMetrics& Metrics, const FAHInteractionScoreWeights& Weights);
	ASHESOFHEAVEN_API bool IsValidScore(float Score);
	ASHESOFHEAVEN_API bool ShouldReplaceCurrent(float CurrentScore, float ChallengerScore, float SwitchThreshold);
	ASHESOFHEAVEN_API float MinimumViewDot(float AngularToleranceDegrees);
}

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHInteractionTargetChangedDelegate, AActor*, Target);

UCLASS(ClassGroup=(AshesOfHeaven), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHInteractionComponent();

	/** Maximum camera-trace reach in centimetres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Acquisition", meta=(ClampMin=50.0))
	float InteractionDistance = 275.0f;

	/** Bounded fallback search around the player; the direct camera trace remains authoritative. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Acquisition", meta=(ClampMin=50.0, ClampMax=600.0))
	float CandidateRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Acquisition", meta=(ClampMin=0.02, ClampMax=0.5))
	float UpdateInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Acquisition", meta=(ClampMin=1.0, ClampMax=45.0))
	float MouseAngularToleranceDegrees = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Acquisition", meta=(ClampMin=1.0, ClampMax=60.0))
	float AssistedAngularToleranceDegrees = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float DirectHitBonus = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float DistanceWeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float AngleWeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float ScreenCenterWeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float VisibilityWeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float InteractionPriorityWeight = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float ObjectivePriorityWeight = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float PersistenceBonus = 45.0f;

	/** A challenger must clear this margin after all score terms, including persistence. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Scoring", meta=(ClampMin=0.0))
	float SwitchThreshold = 60.0f;

	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FAHInteractionTargetChangedDelegate OnTargetChanged;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Interact();

	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Interaction")
	FText GetPrompt() const { return CurrentPrompt; }

private:
	void UpdateTarget(class APlayerController* Controller);
	void SetCurrentTarget(AActor* NewTarget, const FText& NewPrompt);
	bool HasLineOfSight(AActor* Candidate, const FVector& ViewLocation, const FVector& CandidateLocation) const;
	bool UsesAssistedAngularTolerance(APlayerController* Controller);
	FVector GetCandidateLocation(AActor* Candidate) const;
	FAHInteractionScoreWeights GetScoreWeights() const;

	TWeakObjectPtr<AActor> CurrentTarget;
	FText CurrentPrompt;
	TArray<FOverlapResult> CandidateOverlaps;
	TArray<TWeakObjectPtr<AActor>> CandidateActors;
	bool bUsingAssistedInput = false;
};
