#include "Gameplay/AI/AHEnvQueryTest_TacticalPosition.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_VectorBase.h"
#include "Gameplay/AI/AHTacticalEQSContexts.h"
#include "Gameplay/AI/AHTacticalPositionSubsystem.h"

namespace
{
	bool HasClearTrace(UWorld* World, const FVector& From, const FVector& To, const AActor* Querier, const AActor* Target)
	{
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AHTacticalEQSLine), true, Querier);
		FHitResult Hit;
		return !World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params) || Hit.GetActor() == Target;
	}

	bool HasNearbyHardCover(UWorld* World, const FVector& Candidate, const AActor* Querier)
	{
		if (!World)
		{
			return false;
		}
		FCollisionQueryParams Params(SCENE_QUERY_STAT(AHTacticalEQSCoverProbe), true, Querier);
		const FVector Start = Candidate + FVector(0.0f, 0.0f, 70.0f);
		for (const FVector& Direction : { FVector::ForwardVector, FVector::BackwardVector, FVector::RightVector, FVector::LeftVector })
		{
			FHitResult Hit;
			if (World->LineTraceSingleByChannel(Hit, Start, Start + Direction * 240.0f, ECC_Visibility, Params))
			{
				return true;
			}
		}
		return false;
	}
}

UAHEnvQueryTest_TacticalPosition::UAHEnvQueryTest_TacticalPosition(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Cost = EEnvTestCost::High;
	ValidItemType = UEnvQueryItemType_VectorBase::StaticClass();
	TestPurpose = EEnvTestPurpose::FilterAndScore;
	FilterType = EEnvTestFilterType::Minimum;
	FloatValueMin.DefaultValue = 0.001f;
	FloatValueMax.DefaultValue = 1.0f;
	ScoringEquation = EEnvTestScoreEquation::Linear;
	ClampMinType = EEnvQueryTestClamping::SpecifiedValue;
	ClampMaxType = EEnvQueryTestClamping::SpecifiedValue;
	ScoreClampMin.DefaultValue = 0.0f;
	ScoreClampMax.DefaultValue = 1.0f;
	ScoringFactor.DefaultValue = 1.0f;
	SetWorkOnFloatValues(true);
}

void UAHEnvQueryTest_TacticalPosition::RunTest(FEnvQueryInstance& QueryInstance) const
{
	const UObject* Owner = QueryInstance.Owner.Get();
	UAHTacticalPositionSubsystem* Tactical = QueryInstance.World ? QueryInstance.World->GetSubsystem<UAHTacticalPositionSubsystem>() : nullptr;
	const FAHTacticalPositionRequest* Request = Tactical ? Tactical->FindRequest(Owner) : nullptr;
	if (!Owner || !Request)
	{
		return;
	}

	AActor* Querier = Request->Querier.Get();
	TArray<AActor*> Targets;
	QueryInstance.PrepareContext(UAH_EQSContext_CombatTarget::StaticClass(), Targets);
	AActor* Target = Targets.Num() > 0 ? Targets[0] : nullptr;
	const FVector TargetLocation = Target ? Target->GetActorLocation() : Request->LastKnownTarget;
	TArray<AActor*> Squadmates;
	QueryInstance.PrepareContext(UAH_EQSContext_Squadmates::StaticClass(), Squadmates);

	FAHTacticalScoringParams Params;
	Params.Intent = Request->Intent;
	Params.QueryKind = QueryKind;
	Params.PreferredRange = Request->PreferredRange;
	Params.MinimumRange = Request->MinimumRange;
	Params.CurrentDistanceToTarget = Request->CurrentDistanceToTarget;
	Params.GrenadeDangerRadius = Request->GrenadeDangerRadius;
	Params.MinimumSquadSpacing = Request->bSuppressionAttacker ? 420.0f : 300.0f;
	Params.bAccurateAttacker = Request->bAccurateAttacker;
	Params.bSuppressionAttacker = Request->bSuppressionAttacker;
	Params.bSimplifiedScoring = Request->bSimplifiedScoring;

	for (FEnvQueryInstance::ItemIterator It(this, QueryInstance); It; ++It)
	{
		const FVector Candidate = GetItemLocation(QueryInstance, It.GetIndex());
		FAHTacticalCandidateFeatures Features;
		Features.bInPlayableBounds = !Request->PlayableBounds.IsValid || Request->PlayableBounds.IsInsideOrOn(Candidate);
		Features.DistanceToTarget = FVector::Dist2D(Candidate, TargetLocation);
		Features.DistanceFromQuerier = FVector::Dist2D(Candidate, Request->Origin);
		Features.PathCost = Features.DistanceFromQuerier;
		Features.VerticalAdvantage = Candidate.Z - TargetLocation.Z;

		if (Request->GrenadeDangerRadius > 0.0f)
		{
			Features.DistanceFromGrenade = FVector::Dist2D(Candidate, Request->GrenadeThreat);
		}

		const FVector CandidateChest = Candidate + FVector(0.0f, 0.0f, 70.0f);
		const FVector TargetChest = TargetLocation + FVector(0.0f, 0.0f, 62.0f);
		Features.bHasLineOfFire = HasClearTrace(QueryInstance.World, CandidateChest, TargetChest, Querier, Target);
		Features.bHasCoverFromTarget = !Features.bHasLineOfFire;
		Features.bVisibleToPlayer = Features.bHasLineOfFire;
		Features.bExposedLane = Features.bHasLineOfFire && (Request->bSimplifiedScoring || !HasNearbyHardCover(QueryInstance.World, Candidate, Querier));

		const FVector Baseline = (Request->Origin - TargetLocation).GetSafeNormal2D();
		const FVector CandidateDirection = (Candidate - TargetLocation).GetSafeNormal2D();
		if (!Baseline.IsNearlyZero() && !CandidateDirection.IsNearlyZero())
		{
			const float Dot = FMath::Clamp(static_cast<float>(FVector::DotProduct(Baseline, CandidateDirection)), -1.0f, 1.0f);
			Features.FlankingAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));
			Features.FlankSide = FVector::CrossProduct(Baseline, CandidateDirection).Z;
		}

		for (const AActor* Squadmate : Squadmates)
		{
			if (Squadmate && Squadmate != Querier)
			{
				Features.NearestSquadmateDistance = FMath::Min(Features.NearestSquadmateDistance, FVector::Dist2D(Candidate, Squadmate->GetActorLocation()));
			}
		}
		Features.ReservationPenalty = Tactical->GetReservationPenalty(Owner, Candidate);
		Features.RecentPositionPenalty = Tactical->GetRecentPositionPenalty(Owner, Candidate);

		const FAHTacticalScoreResult Score = AHTacticalScoring::ScoreCandidate(Features, Params);
		if (!Score.bAccepted)
		{
			It.ForceItemState(EEnvItemStatus::Failed);
			Tactical->RecordRejection(Owner, Score.RejectionReason);
			continue;
		}

		It.SetScore(TestPurpose, FilterType, Score.Score, 0.001f, 1.0f);
	}
}
