#include "Gameplay/Combat/AHInteractionComponent.h"

#include "Platform/AHPlatformManagerSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"

namespace
{
	TAutoConsoleVariable<int32> CVarInteractionDebug(
		TEXT("ah.Interaction.Debug"),
		0,
		TEXT("Draw interaction candidates, rejection reasons, scores, and the selected actor."),
		ECVF_Cheat);

	constexpr float PriorityLimit = 1.0f;
}

float IAHInteractable::GetInteractionPriority_Implementation() const
{
	return 0.0f;
}

float IAHInteractable::GetObjectiveInteractionPriority_Implementation() const
{
	return 0.0f;
}

float AHInteractionTargeting::ScoreCandidate(const FAHInteractionCandidateMetrics& Metrics, const FAHInteractionScoreWeights& Weights)
{
	if (!Metrics.bActionable || !Metrics.bVisible || Metrics.MaxDistance <= UE_SMALL_NUMBER || Metrics.Distance > Metrics.MaxDistance)
	{
		return -MAX_flt;
	}
	if (!Metrics.bDirectHit && (!Metrics.bOnScreen || Metrics.ViewDot < Metrics.MinimumViewDot))
	{
		return -MAX_flt;
	}

	const float DistanceScore = 1.0f - FMath::Clamp(Metrics.Distance / Metrics.MaxDistance, 0.0f, 1.0f);
	const float AngleDenominator = FMath::Max(1.0f - Metrics.MinimumViewDot, UE_SMALL_NUMBER);
	const float AngleScore = Metrics.bDirectHit
		? 1.0f
		: FMath::Clamp((Metrics.ViewDot - Metrics.MinimumViewDot) / AngleDenominator, 0.0f, 1.0f);
	const float ScreenScore = Metrics.bOnScreen ? 1.0f - FMath::Clamp(Metrics.ScreenCenterDistance, 0.0f, 1.0f) : 0.0f;

	return (Metrics.bDirectHit ? Weights.DirectHitBonus : 0.0f)
		+ (DistanceScore * Weights.DistanceWeight)
		+ (AngleScore * Weights.AngleWeight)
		+ (ScreenScore * Weights.ScreenCenterWeight)
		+ Weights.VisibilityWeight
		+ (FMath::Clamp(Metrics.InteractionPriority, -PriorityLimit, PriorityLimit) * Weights.InteractionPriorityWeight)
		+ (FMath::Clamp(Metrics.ObjectivePriority, -PriorityLimit, PriorityLimit) * Weights.ObjectivePriorityWeight)
		+ (Metrics.bCurrentTarget ? Weights.PersistenceBonus : 0.0f);
}

bool AHInteractionTargeting::IsValidScore(float Score)
{
	return FMath::IsFinite(Score) && Score > (-MAX_flt * 0.5f);
}

bool AHInteractionTargeting::ShouldReplaceCurrent(float CurrentScore, float ChallengerScore, float SwitchThreshold)
{
	if (!IsValidScore(ChallengerScore))
	{
		return false;
	}
	return !IsValidScore(CurrentScore) || ChallengerScore >= CurrentScore + FMath::Max(0.0f, SwitchThreshold);
}

float AHInteractionTargeting::MinimumViewDot(float AngularToleranceDegrees)
{
	return FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(AngularToleranceDegrees, 0.0f, 89.0f)));
}

UAHInteractionComponent::UAHInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void UAHInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(UpdateInterval);
	CandidateOverlaps.Reserve(24);
	CandidateActors.Reserve(16);
}

void UAHInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller || !Controller->IsLocalController() || !Controller->PlayerCameraManager)
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}

	UpdateTarget(Controller);
}

void UAHInteractionComponent::UpdateTarget(APlayerController* Controller)
{
	UWorld* World = GetWorld();
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (!World || !Controller || !Pawn)
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector ViewDirection = ViewRotation.Vector();
	const FVector DirectTraceEnd = ViewLocation + (ViewDirection * InteractionDistance);

	FCollisionQueryParams DirectParams(SCENE_QUERY_STAT(AHInteractionDirect), true, GetOwner());
	FHitResult DirectHit;
	AActor* DirectActor = nullptr;
	if (World->LineTraceSingleByChannel(DirectHit, ViewLocation, DirectTraceEnd, ECC_Visibility, DirectParams))
	{
		DirectActor = DirectHit.GetActor();
	}

	CandidateOverlaps.Reset();
	CandidateActors.Reset();
	if (IsValid(DirectActor))
	{
		CandidateActors.Add(DirectActor);
	}

	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(AHInteractionCandidates), false, GetOwner());
	World->OverlapMultiByChannel(
		CandidateOverlaps,
		Pawn->GetActorLocation(),
		FQuat::Identity,
		ECC_Visibility,
		FCollisionShape::MakeSphere(CandidateRadius),
		OverlapParams);
	for (const FOverlapResult& Overlap : CandidateOverlaps)
	{
		if (AActor* Candidate = Overlap.GetActor())
		{
			CandidateActors.AddUnique(Candidate);
		}
	}

	const bool bDebug = CVarInteractionDebug.GetValueOnGameThread() != 0;
	const float DebugDuration = FMath::Max(UpdateInterval * 1.25f, 0.1f);
	const float ToleranceDegrees = UsesAssistedAngularTolerance(Controller)
		? AssistedAngularToleranceDegrees
		: MouseAngularToleranceDegrees;
	const float MinimumViewDot = AHInteractionTargeting::MinimumViewDot(ToleranceDegrees);
	const FAHInteractionScoreWeights Weights = GetScoreWeights();

	int32 ViewportWidth = 0;
	int32 ViewportHeight = 0;
	Controller->GetViewportSize(ViewportWidth, ViewportHeight);
	const FVector2D ScreenCenter(ViewportWidth * 0.5f, ViewportHeight * 0.5f);
	const float ScreenHalfDiagonal = FMath::Max(ScreenCenter.Size(), 1.0f);

	AActor* BestTarget = nullptr;
	FText BestPrompt;
	float BestScore = -MAX_flt;
	float CurrentScore = -MAX_flt;
	FText RefreshedCurrentPrompt;
	AActor* const ExistingTarget = CurrentTarget.Get();

	for (const TWeakObjectPtr<AActor>& WeakCandidate : CandidateActors)
	{
		AActor* Candidate = WeakCandidate.Get();
		if (!IsValid(Candidate) || !Candidate->GetClass()->ImplementsInterface(UAHInteractable::StaticClass()))
		{
			continue;
		}

		const bool bDirectHit = Candidate == DirectActor;
		const FVector CandidateLocation = bDirectHit ? DirectHit.ImpactPoint : GetCandidateLocation(Candidate);
		const FText Prompt = IAHInteractable::Execute_GetInteractionPrompt(Candidate);
		if (Prompt.IsEmpty())
		{
			if (bDebug)
			{
				DrawDebugSphere(World, CandidateLocation, 18.0f, 8, FColor::Silver, false, DebugDuration);
				DrawDebugString(World, CandidateLocation + FVector(0.0f, 0.0f, 24.0f),
					FString::Printf(TEXT("%s  NO ACTION"), *GetNameSafe(Candidate)), nullptr, FColor::Silver, DebugDuration, false, 0.9f);
			}
			continue;
		}

		const FVector ToCandidate = CandidateLocation - ViewLocation;
		const float Distance = ToCandidate.Size();
		const float ViewDot = Distance > UE_SMALL_NUMBER ? FVector::DotProduct(ToCandidate / Distance, ViewDirection) : 1.0f;
		FVector2D ScreenPosition = ScreenCenter;
		const bool bOnScreen = ViewportWidth > 0 && ViewportHeight > 0
			&& Controller->ProjectWorldLocationToScreen(CandidateLocation, ScreenPosition, true);
		const float ScreenCenterDistance = bOnScreen
			? FVector2D::Distance(ScreenPosition, ScreenCenter) / ScreenHalfDiagonal
			: 1.0f;
		const bool bVisible = bDirectHit || HasLineOfSight(Candidate, ViewLocation, CandidateLocation);

		FAHInteractionCandidateMetrics Metrics;
		Metrics.bActionable = true;
		Metrics.bVisible = bVisible;
		Metrics.bDirectHit = bDirectHit;
		Metrics.bCurrentTarget = Candidate == ExistingTarget;
		Metrics.bOnScreen = bOnScreen;
		Metrics.Distance = Distance;
		Metrics.MaxDistance = bDirectHit ? InteractionDistance : CandidateRadius;
		Metrics.ViewDot = ViewDot;
		Metrics.MinimumViewDot = MinimumViewDot;
		Metrics.ScreenCenterDistance = ScreenCenterDistance;
		Metrics.InteractionPriority = IAHInteractable::Execute_GetInteractionPriority(Candidate);
		Metrics.ObjectivePriority = IAHInteractable::Execute_GetObjectiveInteractionPriority(Candidate);

		const float Score = AHInteractionTargeting::ScoreCandidate(Metrics, Weights);
		if (Candidate == ExistingTarget)
		{
			CurrentScore = Score;
			RefreshedCurrentPrompt = Prompt;
		}
		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
			BestPrompt = Prompt;
		}

		if (bDebug)
		{
			const bool bValid = AHInteractionTargeting::IsValidScore(Score);
			const FColor Color = !bVisible ? FColor::Red : (bValid ? FColor::Green : FColor::Orange);
			const TCHAR* Result = !bVisible
				? TEXT("OCCLUDED")
				: (Distance > Metrics.MaxDistance
					? TEXT("OUT OF RANGE")
					: (!bOnScreen ? TEXT("OFF SCREEN") : (ViewDot < MinimumViewDot && !bDirectHit ? TEXT("OUTSIDE CONE") : TEXT(""))));
			const FString Label = bValid
				? FString::Printf(TEXT("%s  %.1f%s"), *GetNameSafe(Candidate), Score, bDirectHit ? TEXT("  DIRECT") : TEXT(""))
				: FString::Printf(TEXT("%s  %s"), *GetNameSafe(Candidate), Result);
			DrawDebugLine(World, ViewLocation, CandidateLocation, Color, false, DebugDuration, 0, bDirectHit ? 2.5f : 0.75f);
			DrawDebugSphere(World, CandidateLocation, bDirectHit ? 24.0f : 18.0f, 8, Color, false, DebugDuration);
			DrawDebugString(World, CandidateLocation + FVector(0.0f, 0.0f, 24.0f), Label, nullptr, Color, DebugDuration, false, 0.9f);
		}
	}

	AActor* SelectedTarget = BestTarget;
	FText SelectedPrompt = BestPrompt;
	if (ExistingTarget && BestTarget != ExistingTarget
		&& AHInteractionTargeting::IsValidScore(CurrentScore)
		&& !AHInteractionTargeting::ShouldReplaceCurrent(CurrentScore, BestScore, SwitchThreshold))
	{
		SelectedTarget = ExistingTarget;
		SelectedPrompt = RefreshedCurrentPrompt;
	}
	if (!AHInteractionTargeting::IsValidScore(BestScore) && SelectedTarget != ExistingTarget)
	{
		SelectedTarget = nullptr;
		SelectedPrompt = FText::GetEmpty();
	}

	SetCurrentTarget(SelectedTarget, SelectedPrompt);

	if (bDebug)
	{
		DrawDebugSphere(World, Pawn->GetActorLocation(), CandidateRadius, 24, FColor(80, 160, 255), false, DebugDuration);
		DrawDebugLine(World, ViewLocation, DirectTraceEnd, FColor::Cyan, false, DebugDuration, 0, 0.5f);
		if (SelectedTarget)
		{
			const FVector SelectedLocation = SelectedTarget == DirectActor ? DirectHit.ImpactPoint : GetCandidateLocation(SelectedTarget);
			DrawDebugBox(World, SelectedLocation, FVector(26.0f), FColor::Yellow, false, DebugDuration, 0, 2.5f);
			DrawDebugString(World, SelectedLocation + FVector(0.0f, 0.0f, 48.0f),
				FString::Printf(TEXT("SELECTED  %s"), *GetNameSafe(SelectedTarget)), nullptr, FColor::Yellow, DebugDuration, false, 1.0f);
		}
	}
}

void UAHInteractionComponent::SetCurrentTarget(AActor* NewTarget, const FText& NewPrompt)
{
	if (NewTarget != CurrentTarget.Get() || !NewPrompt.EqualTo(CurrentPrompt))
	{
		CurrentTarget = NewTarget;
		CurrentPrompt = NewPrompt;
		OnTargetChanged.Broadcast(NewTarget);
	}
}

bool UAHInteractionComponent::HasLineOfSight(AActor* Candidate, const FVector& ViewLocation, const FVector& CandidateLocation) const
{
	if (!IsValid(Candidate) || !GetWorld())
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHInteractionVisibility), false, GetOwner());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, CandidateLocation, ECC_Visibility, Params)
		|| Hit.GetActor() == Candidate;
}

bool UAHInteractionComponent::UsesAssistedAngularTolerance(APlayerController* Controller)
{
	const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	if (Platform && Platform->ShouldUseTouchControls())
	{
		return true;
	}
	if (!Controller || (Platform && !Platform->GetInputProfile().bSupportsExternalController))
	{
		return false;
	}

	const float MouseActivity = FMath::Max(
		FMath::Abs(Controller->GetInputAnalogKeyState(EKeys::MouseX)),
		FMath::Abs(Controller->GetInputAnalogKeyState(EKeys::MouseY)));
	const float GamepadActivity = FMath::Max(
		FMath::Max(
			FMath::Abs(Controller->GetInputAnalogKeyState(EKeys::Gamepad_RightX)),
			FMath::Abs(Controller->GetInputAnalogKeyState(EKeys::Gamepad_RightY))),
		FMath::Max(
			FMath::Abs(Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftX)),
			FMath::Abs(Controller->GetInputAnalogKeyState(EKeys::Gamepad_LeftY))));

	if (MouseActivity > 0.01f)
	{
		bUsingAssistedInput = false;
	}
	else if (GamepadActivity > 0.15f
		|| Controller->IsInputKeyDown(EKeys::Gamepad_FaceButton_Left)
		|| Controller->IsInputKeyDown(EKeys::Gamepad_FaceButton_Bottom)
		|| Controller->IsInputKeyDown(EKeys::Gamepad_LeftTrigger)
		|| Controller->IsInputKeyDown(EKeys::Gamepad_RightTrigger))
	{
		bUsingAssistedInput = true;
	}
	return bUsingAssistedInput;
}

FVector UAHInteractionComponent::GetCandidateLocation(AActor* Candidate) const
{
	if (!Candidate)
	{
		return FVector::ZeroVector;
	}

	FVector BoundsOrigin;
	FVector BoundsExtent;
	Candidate->GetActorBounds(true, BoundsOrigin, BoundsExtent);
	return BoundsExtent.IsNearlyZero() ? Candidate->GetActorLocation() : BoundsOrigin;
}

FAHInteractionScoreWeights UAHInteractionComponent::GetScoreWeights() const
{
	FAHInteractionScoreWeights Weights;
	Weights.DirectHitBonus = DirectHitBonus;
	Weights.DistanceWeight = DistanceWeight;
	Weights.AngleWeight = AngleWeight;
	Weights.ScreenCenterWeight = ScreenCenterWeight;
	Weights.VisibilityWeight = VisibilityWeight;
	Weights.InteractionPriorityWeight = InteractionPriorityWeight;
	Weights.ObjectivePriorityWeight = ObjectivePriorityWeight;
	Weights.PersistenceBonus = PersistenceBonus;
	return Weights;
}

void UAHInteractionComponent::Interact()
{
	AActor* Target = CurrentTarget.Get();
	if (!IsValid(Target) || !Target->GetClass()->ImplementsInterface(UAHInteractable::StaticClass()))
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}

	const FText PromptBeforeInteraction = IAHInteractable::Execute_GetInteractionPrompt(Target);
	if (PromptBeforeInteraction.IsEmpty())
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
		return;
	}

	IAHInteractable::Execute_Interact(Target, GetOwner());
	if (IsValid(Target))
	{
		const FText RefreshedPrompt = IAHInteractable::Execute_GetInteractionPrompt(Target);
		SetCurrentTarget(RefreshedPrompt.IsEmpty() ? nullptr : Target, RefreshedPrompt);
	}
	else
	{
		SetCurrentTarget(nullptr, FText::GetEmpty());
	}
}
