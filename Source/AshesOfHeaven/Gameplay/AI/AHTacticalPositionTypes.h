#pragma once

#include "CoreMinimal.h"
#include "AHTacticalPositionTypes.generated.h"

class AActor;

/** The tactical decision made by the combat controller. EQS resolves only the location. */
UENUM(BlueprintType)
enum class EAHTacticalIntent : uint8
{
	Hold,
	Advance,
	Retreat,
	FindCover,
	FlankLeft,
	FlankRight,
	Reposition,
	SearchLastKnown,
	EscapeGrenade,
	ReinforcementSpawn
};

/** Focused query families with independent generators and scoring weights. */
UENUM()
enum class EAHTacticalQueryKind : uint8
{
	Cover,
	Flank,
	RangedFiring,
	Retreat,
	Search,
	Reinforcement
};

/** Difficulty changes decision quality and cadence without adding target knowledge. */
UENUM(BlueprintType)
enum class EAHAITacticalDifficulty : uint8
{
	Recruit,
	Regular,
	Veteran,
	Damnation
};

/** Data gathered for one generated point before focused query scoring. */
struct ASHESOFHEAVEN_API FAHTacticalCandidateFeatures
{
	bool bReachable = true;
	bool bInPlayableBounds = true;
	bool bHasCoverFromTarget = false;
	bool bHasLineOfFire = false;
	bool bExposedLane = false;
	bool bVisibleToPlayer = false;
	float DistanceToTarget = 0.0f;
	float DistanceFromQuerier = 0.0f;
	float DistanceFromGrenade = BIG_NUMBER;
	float NearestSquadmateDistance = BIG_NUMBER;
	float FlankingAngleDegrees = 0.0f;
	float FlankSide = 0.0f;
	float VerticalAdvantage = 0.0f;
	float PathCost = 0.0f;
	float ReservationPenalty = 0.0f;
	float RecentPositionPenalty = 0.0f;
};

/** Pure scoring inputs shared by runtime EQS and deterministic automation tests. */
struct ASHESOFHEAVEN_API FAHTacticalScoringParams
{
	EAHTacticalIntent Intent = EAHTacticalIntent::Hold;
	EAHTacticalQueryKind QueryKind = EAHTacticalQueryKind::RangedFiring;
	float PreferredRange = 1100.0f;
	float MinimumRange = 650.0f;
	float CurrentDistanceToTarget = 0.0f;
	float GrenadeDangerRadius = 0.0f;
	float MinimumSquadSpacing = 300.0f;
	bool bAccurateAttacker = true;
	bool bSuppressionAttacker = false;
	bool bSimplifiedScoring = false;
};

/** Score and the first hard rejection produced for a tactical candidate. */
struct ASHESOFHEAVEN_API FAHTacticalScoreResult
{
	bool bAccepted = false;
	float Score = 0.0f;
	FName RejectionReason = NAME_None;
};

/** Immutable data captured when an asynchronous tactical query starts. */
struct ASHESOFHEAVEN_API FAHTacticalPositionRequest
{
	TWeakObjectPtr<AActor> Querier;
	TWeakObjectPtr<AActor> CombatTarget;
	TArray<TWeakObjectPtr<AActor>> Squadmates;
	EAHTacticalIntent Intent = EAHTacticalIntent::Hold;
	EAHTacticalQueryKind QueryKind = EAHTacticalQueryKind::RangedFiring;
	FVector Origin = FVector::ZeroVector;
	FVector LastKnownTarget = FVector::ZeroVector;
	FVector GrenadeThreat = FVector::ZeroVector;
	FVector FallbackLocation = FVector::ZeroVector;
	FBox PlayableBounds = FBox(EForceInit::ForceInit);
	float PreferredRange = 1100.0f;
	float MinimumRange = 650.0f;
	float CurrentDistanceToTarget = 0.0f;
	float GrenadeDangerRadius = 0.0f;
	float QualityTolerance = 0.82f;
	float TimeoutSeconds = 0.25f;
	int32 MaxCandidatePoints = 64;
	int32 RandomSeed = 0;
	bool bAccurateAttacker = true;
	bool bSuppressionAttacker = false;
	bool bSimplifiedScoring = false;
};

/** Completion payload returned for both EQS and fallback paths. */
struct ASHESOFHEAVEN_API FAHTacticalPositionResult
{
	EAHTacticalIntent Intent = EAHTacticalIntent::Hold;
	FVector Location = FVector::ZeroVector;
	float Score = 0.0f;
	bool bSuccess = false;
	bool bUsedFallback = false;
	FName FailureReason = NAME_None;
	int32 QueryId = INDEX_NONE;
};

namespace AHTacticalScoring
{
	ASHESOFHEAVEN_API float PreferredRangeScore(float Distance, float PreferredRange, float MinimumRange);
	ASHESOFHEAVEN_API float FlankingScore(float AngleDegrees);
	ASHESOFHEAVEN_API float SquadSpacingScore(float Distance, float MinimumSpacing);
	ASHESOFHEAVEN_API bool CanStartQuery(bool bQueryInFlight, double NowSeconds, double NextAllowedQueryTime, bool bHasUsableCachedLocation);
	ASHESOFHEAVEN_API FAHTacticalScoreResult ScoreCandidate(const FAHTacticalCandidateFeatures& Features, const FAHTacticalScoringParams& Params);
	ASHESOFHEAVEN_API EAHTacticalQueryKind QueryKindForIntent(EAHTacticalIntent Intent);
	ASHESOFHEAVEN_API const TCHAR* IntentToString(EAHTacticalIntent Intent);
}
