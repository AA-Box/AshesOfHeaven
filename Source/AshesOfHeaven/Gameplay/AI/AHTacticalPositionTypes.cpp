#include "Gameplay/AI/AHTacticalPositionTypes.h"

namespace
{
	float CoverScore(const FAHTacticalCandidateFeatures& Features)
	{
		return Features.bHasCoverFromTarget ? 1.0f : 0.0f;
	}

	float LineOfFireScore(const FAHTacticalCandidateFeatures& Features)
	{
		return Features.bHasLineOfFire ? 1.0f : 0.0f;
	}

	float VerticalScore(float VerticalAdvantage)
	{
		return FMath::Clamp((VerticalAdvantage + 100.0f) / 500.0f, 0.0f, 1.0f);
	}

	float RetreatScore(const FAHTacticalCandidateFeatures& Features, const FAHTacticalScoringParams& Params)
	{
		if (Params.Intent == EAHTacticalIntent::EscapeGrenade && Params.GrenadeDangerRadius > 0.0f)
		{
			const float SafeBand = FMath::Max(400.0f, Params.GrenadeDangerRadius);
			return FMath::Clamp((Features.DistanceFromGrenade - Params.GrenadeDangerRadius) / SafeBand, 0.0f, 1.0f);
		}
		const float DesiredGain = FMath::Max(250.0f, Params.PreferredRange * 0.45f);
		return FMath::Clamp((Features.DistanceToTarget - Params.CurrentDistanceToTarget) / DesiredGain, 0.0f, 1.0f);
	}
}

float AHTacticalScoring::PreferredRangeScore(float Distance, float PreferredRange, float MinimumRange)
{
	const float SafePreferred = FMath::Max(PreferredRange, MinimumRange + 1.0f);
	if (Distance < MinimumRange)
	{
		return FMath::Clamp(Distance / FMath::Max(1.0f, MinimumRange), 0.0f, 0.25f);
	}

	const float Tolerance = FMath::Max(200.0f, SafePreferred * 0.65f);
	return 1.0f - FMath::Clamp(FMath::Abs(Distance - SafePreferred) / Tolerance, 0.0f, 1.0f);
}

float AHTacticalScoring::FlankingScore(float AngleDegrees)
{
	const float Angle = FMath::Abs(AngleDegrees);
	if (Angle < 35.0f || Angle > 110.0f)
	{
		return 0.0f;
	}

	return 1.0f - FMath::Clamp(FMath::Abs(Angle - 72.5f) / 37.5f, 0.0f, 1.0f);
}

float AHTacticalScoring::SquadSpacingScore(float Distance, float MinimumSpacing)
{
	if (!FMath::IsFinite(Distance) || Distance >= BIG_NUMBER * 0.5f)
	{
		return 1.0f;
	}
	if (Distance <= MinimumSpacing)
	{
		return 0.0f;
	}

	return FMath::Clamp((Distance - MinimumSpacing) / FMath::Max(1.0f, MinimumSpacing), 0.0f, 1.0f);
}

bool AHTacticalScoring::CanStartQuery(bool bQueryInFlight, double NowSeconds, double NextAllowedQueryTime, bool bHasUsableCachedLocation)
{
	return !bQueryInFlight && NowSeconds >= NextAllowedQueryTime && !bHasUsableCachedLocation;
}

FAHTacticalScoreResult AHTacticalScoring::ScoreCandidate(const FAHTacticalCandidateFeatures& Features, const FAHTacticalScoringParams& Params)
{
	FAHTacticalScoreResult Result;
	if (!Features.bReachable)
	{
		Result.RejectionReason = TEXT("Unreachable");
		return Result;
	}
	if (!Features.bInPlayableBounds)
	{
		Result.RejectionReason = TEXT("OutsidePlayableBounds");
		return Result;
	}
	if (Features.NearestSquadmateDistance < Params.MinimumSquadSpacing * 0.55f)
	{
		Result.RejectionReason = TEXT("SquadCrowding");
		return Result;
	}
	if (Params.GrenadeDangerRadius > 0.0f && Features.DistanceFromGrenade < Params.GrenadeDangerRadius)
	{
		Result.RejectionReason = TEXT("GrenadeDanger");
		return Result;
	}
	if (Params.Intent == EAHTacticalIntent::FlankLeft && Features.FlankSide < 0.05f)
	{
		Result.RejectionReason = TEXT("WrongFlankSide");
		return Result;
	}
	if (Params.Intent == EAHTacticalIntent::FlankRight && Features.FlankSide > -0.05f)
	{
		Result.RejectionReason = TEXT("WrongFlankSide");
		return Result;
	}

	const float Range = PreferredRangeScore(Features.DistanceToTarget, Params.PreferredRange, Params.MinimumRange);
	const float Flank = FlankingScore(Features.FlankingAngleDegrees);
	const float Spacing = SquadSpacingScore(Features.NearestSquadmateDistance, Params.MinimumSquadSpacing);
	const float Cover = CoverScore(Features);
	const float LineOfFire = LineOfFireScore(Features);
	const float Vertical = Params.bSimplifiedScoring ? 0.5f : VerticalScore(Features.VerticalAdvantage);
	const float Exposure = Features.bExposedLane ? 0.0f : 1.0f;
	const float Reservation = 1.0f - FMath::Clamp(Features.ReservationPenalty, 0.0f, 1.0f);
	const float Recent = 1.0f - FMath::Clamp(Features.RecentPositionPenalty, 0.0f, 1.0f);
	float WeightedScore = 0.0f;
	float TotalWeight = 0.0f;

	auto Add = [&WeightedScore, &TotalWeight](float Value, float Weight)
	{
		WeightedScore += FMath::Clamp(Value, 0.0f, 1.0f) * Weight;
		TotalWeight += Weight;
	};

	switch (Params.QueryKind)
	{
	case EAHTacticalQueryKind::Cover:
		Add(Cover, Params.bSuppressionAttacker ? 3.5f : 3.0f);
		Add(Range, 1.4f);
		Add(LineOfFire, Params.bAccurateAttacker ? 1.2f : 0.5f);
		Add(Exposure, 1.1f);
		Add(Spacing, Params.bSuppressionAttacker ? 1.5f : 1.0f);
		break;
	case EAHTacticalQueryKind::Flank:
		Add(Flank, 3.5f);
		Add(LineOfFire, 1.8f);
		Add(Range, 1.5f);
		Add(Cover, 0.8f);
		Add(Spacing, 1.4f);
		break;
	case EAHTacticalQueryKind::RangedFiring:
		Add(LineOfFire, Params.bAccurateAttacker ? 3.5f : 2.0f);
		Add(Range, 2.5f);
		Add(Cover, Params.bSuppressionAttacker ? 1.8f : 0.8f);
		Add(Vertical, 0.8f);
		Add(Spacing, 1.0f);
		break;
	case EAHTacticalQueryKind::Retreat:
		Add(RetreatScore(Features, Params), 3.2f);
		Add(Cover, 2.0f);
		Add(Range, 1.5f);
		Add(Exposure, 1.0f);
		Add(Spacing, 0.8f);
		break;
	case EAHTacticalQueryKind::Search:
		Add(1.0f - FMath::Clamp(Features.DistanceToTarget / FMath::Max(800.0f, Params.PreferredRange), 0.0f, 1.0f), 2.0f);
		Add(Spacing, 1.8f);
		Add(Cover, 0.6f);
		Add(Exposure, 0.5f);
		break;
	case EAHTacticalQueryKind::Reinforcement:
		Add(Features.bVisibleToPlayer ? 0.0f : 1.0f, 3.5f);
		Add(Features.DistanceToTarget >= Params.PreferredRange ? 1.0f : 0.0f, 1.8f);
		Add(Cover, 1.2f);
		Add(Spacing, 1.5f);
		break;
	default:
		break;
	}

	Add(Reservation, 1.5f);
	Add(Recent, 1.2f);
	if (!Params.bSimplifiedScoring)
	{
		const float TravelEfficiency = 1.0f - FMath::Clamp(Features.PathCost / FMath::Max(1000.0f, Params.PreferredRange * 2.5f), 0.0f, 1.0f);
		Add(TravelEfficiency, 0.7f);
	}

	Result.bAccepted = TotalWeight > 0.0f;
	Result.Score = Result.bAccepted ? WeightedScore / TotalWeight : 0.0f;
	return Result;
}

EAHTacticalQueryKind AHTacticalScoring::QueryKindForIntent(EAHTacticalIntent Intent)
{
	switch (Intent)
	{
	case EAHTacticalIntent::FindCover: return EAHTacticalQueryKind::Cover;
	case EAHTacticalIntent::FlankLeft:
	case EAHTacticalIntent::FlankRight: return EAHTacticalQueryKind::Flank;
	case EAHTacticalIntent::Retreat:
	case EAHTacticalIntent::EscapeGrenade: return EAHTacticalQueryKind::Retreat;
	case EAHTacticalIntent::SearchLastKnown: return EAHTacticalQueryKind::Search;
	case EAHTacticalIntent::ReinforcementSpawn: return EAHTacticalQueryKind::Reinforcement;
	case EAHTacticalIntent::Advance:
	case EAHTacticalIntent::Reposition:
	case EAHTacticalIntent::Hold:
	default: return EAHTacticalQueryKind::RangedFiring;
	}
}

const TCHAR* AHTacticalScoring::IntentToString(EAHTacticalIntent Intent)
{
	switch (Intent)
	{
	case EAHTacticalIntent::Hold: return TEXT("Hold");
	case EAHTacticalIntent::Advance: return TEXT("Advance");
	case EAHTacticalIntent::Retreat: return TEXT("Retreat");
	case EAHTacticalIntent::FindCover: return TEXT("FindCover");
	case EAHTacticalIntent::FlankLeft: return TEXT("FlankLeft");
	case EAHTacticalIntent::FlankRight: return TEXT("FlankRight");
	case EAHTacticalIntent::Reposition: return TEXT("Reposition");
	case EAHTacticalIntent::SearchLastKnown: return TEXT("SearchLastKnown");
	case EAHTacticalIntent::EscapeGrenade: return TEXT("EscapeGrenade");
	case EAHTacticalIntent::ReinforcementSpawn: return TEXT("ReinforcementSpawn");
	default: return TEXT("Unknown");
	}
}
