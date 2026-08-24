#include "Gameplay/AI/AHEnvQueryGenerator_TacticalPositions.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Gameplay/AI/AHTacticalPositionSubsystem.h"

namespace
{
	void AddCandidate(TArray<FNavLocation>& Candidates, const FVector& Location, int32 Maximum)
	{
		if (Candidates.Num() < Maximum)
		{
			Candidates.Emplace(Location);
		}
	}

	void AddRing(
		TArray<FNavLocation>& Candidates,
		const FVector& Center,
		float Radius,
		float StartAngle,
		float ArcDegrees,
		int32 PointCount,
		int32 Maximum)
	{
		if (PointCount <= 0 || Candidates.Num() >= Maximum)
		{
			return;
		}
		const float Step = PointCount > 1 ? ArcDegrees / static_cast<float>(PointCount - 1) : 0.0f;
		for (int32 Index = 0; Index < PointCount && Candidates.Num() < Maximum; ++Index)
		{
			const float Angle = StartAngle + Step * Index;
			AddCandidate(Candidates, Center + FVector(Radius, 0.0f, 0.0f).RotateAngleAxis(Angle, FVector::UpVector), Maximum);
		}
	}
}

UAHEnvQueryGenerator_TacticalPositions::UAHEnvQueryGenerator_TacticalPositions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ItemType = UEnvQueryItemType_Point::StaticClass();
	bAutoSortTests = true;
	ProjectionData.SetNavmeshOnly();
	ProjectionData.ProjectDown = 500.0f;
	ProjectionData.ProjectUp = 300.0f;
	ProjectionData.ExtentX = 150.0f;
}

void UAHEnvQueryGenerator_TacticalPositions::GenerateItems(FEnvQueryInstance& QueryInstance) const
{
	const UObject* Owner = QueryInstance.Owner.Get();
	UAHTacticalPositionSubsystem* Tactical = QueryInstance.World ? QueryInstance.World->GetSubsystem<UAHTacticalPositionSubsystem>() : nullptr;
	const FAHTacticalPositionRequest* Request = Tactical ? Tactical->FindRequest(Owner) : nullptr;
	if (!Request)
	{
		return;
	}

	const int32 Maximum = FMath::Clamp(Request->MaxCandidatePoints, 8, 128);
	const int32 RingPoints = Request->bSimplifiedScoring ? 8 : 12;
	FRandomStream Random(Request->RandomSeed);
	const float Jitter = Random.FRandRange(-8.0f, 8.0f);
	TArray<FNavLocation> Candidates;
	Candidates.Reserve(Maximum);

	const FVector Target = Request->CombatTarget.IsValid() ? Request->CombatTarget->GetActorLocation() : Request->LastKnownTarget;
	const FVector FromTarget = (Request->Origin - Target).GetSafeNormal2D();
	const float BaseAngle = FromTarget.IsNearlyZero() ? 0.0f : FromTarget.Rotation().Yaw;

	switch (QueryKind)
	{
	case EAHTacticalQueryKind::Cover:
		AddCandidate(Candidates, Request->Origin, Maximum);
		AddRing(Candidates, Request->Origin, 350.0f, Jitter, 360.0f, RingPoints, Maximum);
		AddRing(Candidates, Request->Origin, 700.0f, Jitter + 15.0f, 360.0f, RingPoints + 2, Maximum);
		AddRing(Candidates, Request->Origin, 1050.0f, Jitter, 360.0f, RingPoints + 4, Maximum);
		break;
	case EAHTacticalQueryKind::Flank:
	{
		const float Side = Request->Intent == EAHTacticalIntent::FlankRight ? -1.0f : 1.0f;
		const float ArcStart = BaseAngle + Side * 35.0f + Jitter;
		const float Arc = Side * 75.0f;
		for (const float Scale : { 0.82f, 1.0f, 1.18f })
		{
			AddRing(Candidates, Target, Request->PreferredRange * Scale, ArcStart, Arc, RingPoints, Maximum);
		}
		break;
	}
	case EAHTacticalQueryKind::RangedFiring:
		for (const float Scale : { 0.78f, 1.0f, 1.22f })
		{
			AddRing(Candidates, Target, Request->PreferredRange * Scale, BaseAngle - 125.0f + Jitter, 250.0f, RingPoints, Maximum);
		}
		break;
	case EAHTacticalQueryKind::Retreat:
	{
		const FVector Threat = Request->Intent == EAHTacticalIntent::EscapeGrenade ? Request->GrenadeThreat : Target;
		const FVector Away = (Request->Origin - Threat).GetSafeNormal2D();
		const float AwayAngle = Away.IsNearlyZero() ? BaseAngle : Away.Rotation().Yaw;
		for (const float Distance : { 400.0f, 800.0f, 1200.0f })
		{
			AddRing(Candidates, Request->Origin, Distance, AwayAngle - 65.0f + Jitter, 130.0f, RingPoints, Maximum);
		}
		break;
	}
	case EAHTacticalQueryKind::Search:
		AddCandidate(Candidates, Request->LastKnownTarget, Maximum);
		AddRing(Candidates, Request->LastKnownTarget, 280.0f, Jitter, 360.0f, RingPoints, Maximum);
		AddRing(Candidates, Request->LastKnownTarget, 560.0f, Jitter + 20.0f, 360.0f, RingPoints, Maximum);
		AddRing(Candidates, Request->LastKnownTarget, 850.0f, Jitter, 360.0f, RingPoints, Maximum);
		break;
	case EAHTacticalQueryKind::Reinforcement:
		for (const float Scale : { 1.5f, 2.1f, 2.8f })
		{
			AddRing(Candidates, Target, Request->PreferredRange * Scale, Jitter, 360.0f, RingPoints, Maximum);
		}
		break;
	default:
		break;
	}

	ProjectAndFilterNavPoints(Candidates, QueryInstance);
	StoreNavPoints(Candidates, QueryInstance);
}
