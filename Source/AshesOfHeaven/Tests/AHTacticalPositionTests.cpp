#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/AI/AHTacticalPositionSubsystem.h"
#include "Gameplay/AI/AHTacticalPositionTypes.h"
#include "Misc/AutomationTest.h"
#include "Platform/AHPlatformManagerSubsystem.h"

namespace
{
	constexpr EAutomationTestFlags TacticalTestFlags =
		EAutomationTestFlags::EditorContext
		| EAutomationTestFlags::CommandletContext
		| EAutomationTestFlags::ProductFilter;

	FAHTacticalCandidateFeatures MakeViableCandidate()
	{
		FAHTacticalCandidateFeatures Features;
		Features.bReachable = true;
		Features.bInPlayableBounds = true;
		Features.bHasLineOfFire = true;
		Features.DistanceToTarget = 1100.0f;
		Features.DistanceFromQuerier = 600.0f;
		Features.DistanceFromGrenade = BIG_NUMBER;
		Features.NearestSquadmateDistance = 900.0f;
		Features.PathCost = 650.0f;
		return Features;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalReachabilityTest,
	"AshesOfHeaven.AI.Tactical.ReachableLocation",
	TacticalTestFlags)

bool FAHTacticalReachabilityTest::RunTest(const FString& Parameters)
{
	FAHTacticalScoringParams Params;
	const FAHTacticalScoreResult Reachable = AHTacticalScoring::ScoreCandidate(MakeViableCandidate(), Params);
	TestTrue(TEXT("a reachable point inside the playable area is eligible"), Reachable.bAccepted);

	FAHTacticalCandidateFeatures Blocked = MakeViableCandidate();
	Blocked.bReachable = false;
	const FAHTacticalScoreResult Rejected = AHTacticalScoring::ScoreCandidate(Blocked, Params);
	TestFalse(TEXT("unreachable navigation points are rejected"), Rejected.bAccepted);
	TestEqual(TEXT("reachability rejection is diagnostic"), Rejected.RejectionReason, FName(TEXT("Unreachable")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalCoverPreferenceTest,
	"AshesOfHeaven.AI.Tactical.CoverPreference",
	TacticalTestFlags)

bool FAHTacticalCoverPreferenceTest::RunTest(const FString& Parameters)
{
	FAHTacticalScoringParams Params;
	Params.Intent = EAHTacticalIntent::FindCover;
	Params.QueryKind = EAHTacticalQueryKind::Cover;

	FAHTacticalCandidateFeatures Exposed = MakeViableCandidate();
	Exposed.bExposedLane = true;
	FAHTacticalCandidateFeatures Covered = Exposed;
	Covered.bHasCoverFromTarget = true;
	Covered.bExposedLane = false;

	const float ExposedScore = AHTacticalScoring::ScoreCandidate(Exposed, Params).Score;
	const float CoveredScore = AHTacticalScoring::ScoreCandidate(Covered, Params).Score;
	TestTrue(TEXT("the focused cover query ranks actual cover above an exposed firing lane"), CoveredScore > ExposedScore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalFlankAngleTest,
	"AshesOfHeaven.AI.Tactical.FlankAngle",
	TacticalTestFlags)

bool FAHTacticalFlankAngleTest::RunTest(const FString& Parameters)
{
	FAHTacticalScoringParams Params;
	Params.Intent = EAHTacticalIntent::FlankLeft;
	Params.QueryKind = EAHTacticalQueryKind::Flank;

	FAHTacticalCandidateFeatures Lateral = MakeViableCandidate();
	Lateral.FlankSide = 1.0f;
	Lateral.FlankingAngleDegrees = 72.5f;
	FAHTacticalCandidateFeatures Forward = Lateral;
	Forward.FlankingAngleDegrees = 10.0f;
	FAHTacticalCandidateFeatures WrongSide = Lateral;
	WrongSide.FlankSide = -1.0f;

	const FAHTacticalScoreResult LateralResult = AHTacticalScoring::ScoreCandidate(Lateral, Params);
	const FAHTacticalScoreResult ForwardResult = AHTacticalScoring::ScoreCandidate(Forward, Params);
	const FAHTacticalScoreResult WrongSideResult = AHTacticalScoring::ScoreCandidate(WrongSide, Params);
	TestTrue(TEXT("35-110 degree lateral positions beat nearly-forward positions"), LateralResult.Score > ForwardResult.Score);
	TestFalse(TEXT("left-flank queries reject right-side points"), WrongSideResult.bAccepted);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalMinimumRangeRetreatTest,
	"AshesOfHeaven.AI.Tactical.MinimumRangeRetreat",
	TacticalTestFlags)

bool FAHTacticalMinimumRangeRetreatTest::RunTest(const FString& Parameters)
{
	FAHTacticalScoringParams Params;
	Params.Intent = EAHTacticalIntent::Retreat;
	Params.QueryKind = EAHTacticalQueryKind::Retreat;
	Params.MinimumRange = 650.0f;
	Params.PreferredRange = 1100.0f;
	Params.CurrentDistanceToTarget = 350.0f;

	FAHTacticalCandidateFeatures Near = MakeViableCandidate();
	Near.DistanceToTarget = 500.0f;
	FAHTacticalCandidateFeatures Standoff = Near;
	Standoff.DistanceToTarget = 1100.0f;
	Standoff.bHasCoverFromTarget = true;

	TestTrue(
		TEXT("inside minimum range, increased standoff plus cover receives the higher score"),
		AHTacticalScoring::ScoreCandidate(Standoff, Params).Score > AHTacticalScoring::ScoreCandidate(Near, Params).Score);
	TestEqual(
		TEXT("retreat intent routes to the focused retreat query"),
		AHTacticalScoring::QueryKindForIntent(EAHTacticalIntent::Retreat),
		EAHTacticalQueryKind::Retreat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalSquadSpacingTest,
	"AshesOfHeaven.AI.Tactical.SquadSpacing",
	TacticalTestFlags)

bool FAHTacticalSquadSpacingTest::RunTest(const FString& Parameters)
{
	FAHTacticalScoringParams Params;
	Params.MinimumSquadSpacing = 300.0f;
	FAHTacticalCandidateFeatures Crowded = MakeViableCandidate();
	Crowded.NearestSquadmateDistance = 100.0f;
	FAHTacticalCandidateFeatures Spaced = Crowded;
	Spaced.NearestSquadmateDistance = 750.0f;

	const FAHTacticalScoreResult CrowdedResult = AHTacticalScoring::ScoreCandidate(Crowded, Params);
	const FAHTacticalScoreResult SpacedResult = AHTacticalScoring::ScoreCandidate(Spaced, Params);
	TestFalse(TEXT("points that would stack squadmates are rejected"), CrowdedResult.bAccepted);
	TestTrue(TEXT("a separated point remains eligible"), SpacedResult.bAccepted);
	TestEqual(TEXT("crowding rejection is diagnostic"), CrowdedResult.RejectionReason, FName(TEXT("SquadCrowding")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalFailedQueryFallbackTest,
	"AshesOfHeaven.AI.Tactical.FailedQueryFallback",
	TacticalTestFlags)

bool FAHTacticalFailedQueryFallbackTest::RunTest(const FString& Parameters)
{
	UAHTacticalPositionSubsystem* Subsystem = NewObject<UAHTacticalPositionSubsystem>();
	FAHTacticalPositionRequest Request;
	Request.Intent = EAHTacticalIntent::FindCover;
	Request.FallbackLocation = FVector(125.0f, -300.0f, 20.0f);
	bool bCompleted = false;
	FAHTacticalPositionResult Result;
	Subsystem->RequestPosition(Request, FAHTacticalQueryFinished::CreateLambda(
		[&bCompleted, &Result](const FAHTacticalPositionResult& InResult)
		{
			bCompleted = true;
			Result = InResult;
		}));

	TestTrue(TEXT("an invalid/unavailable EQS request completes synchronously through fallback"), bCompleted);
	TestTrue(TEXT("fallback use is reported"), Result.bUsedFallback);
	TestEqual(TEXT("fallback location is preserved"), Result.Location, Request.FallbackLocation);
	TestEqual(TEXT("failure reason is reported"), Result.FailureReason, FName(TEXT("InvalidQuerier")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalNoQuerySpamTest,
	"AshesOfHeaven.AI.Tactical.NoQuerySpam",
	TacticalTestFlags)

bool FAHTacticalNoQuerySpamTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("an in-flight request blocks a restart"), AHTacticalScoring::CanStartQuery(true, 10.0, 5.0, false));
	TestFalse(TEXT("the per-controller cadence blocks an early restart"), AHTacticalScoring::CanStartQuery(false, 4.9, 5.0, false));
	TestFalse(TEXT("a usable cached location blocks redundant work"), AHTacticalScoring::CanStartQuery(false, 10.0, 5.0, true));
	TestTrue(TEXT("a request starts only when idle, due, and uncached"), AHTacticalScoring::CanStartQuery(false, 10.0, 5.0, false));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalMobileBudgetTest,
	"AshesOfHeaven.AI.Tactical.MobileBudget",
	TacticalTestFlags)

bool FAHTacticalMobileBudgetTest::RunTest(const FString& Parameters)
{
	FAHPerformanceProfile MobilePerformance;
	UAHPlatformManagerSubsystem::ApplyEQSPerformanceBudget(MobilePerformance, true, true, EAHQualityPreset::Mobile);
	TestEqual(TEXT("mobile performance mode caps concurrent EQS work"), MobilePerformance.MaxConcurrentEQSQueries, 2);
	TestEqual(TEXT("mobile performance mode caps generated points"), MobilePerformance.EQSMaxCandidatePoints, 24);
	TestTrue(TEXT("mobile scoring omits secondary probes"), MobilePerformance.bUseSimplifiedEQSScoring);
	TestTrue(TEXT("mobile query cadence is lower than desktop"), MobilePerformance.EQSQueryUpdateInterval >= 1.5f);
	TestTrue(TEXT("far-away mobile AI stop expensive repositioning"), MobilePerformance.EQSExpensiveRepositionDistance <= 3500.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHTacticalGrenadeAvoidanceTest,
	"AshesOfHeaven.AI.Tactical.GrenadeAvoidance",
	TacticalTestFlags)

bool FAHTacticalGrenadeAvoidanceTest::RunTest(const FString& Parameters)
{
	FAHTacticalScoringParams Params;
	Params.Intent = EAHTacticalIntent::EscapeGrenade;
	Params.QueryKind = EAHTacticalQueryKind::Retreat;
	Params.GrenadeDangerRadius = 500.0f;

	FAHTacticalCandidateFeatures Dangerous = MakeViableCandidate();
	Dangerous.DistanceFromGrenade = 300.0f;
	FAHTacticalCandidateFeatures MarginallySafe = Dangerous;
	MarginallySafe.DistanceFromGrenade = 550.0f;
	FAHTacticalCandidateFeatures Safe = Dangerous;
	Safe.DistanceFromGrenade = 1000.0f;

	const FAHTacticalScoreResult DangerousResult = AHTacticalScoring::ScoreCandidate(Dangerous, Params);
	const FAHTacticalScoreResult SafeResult = AHTacticalScoring::ScoreCandidate(Safe, Params);
	TestFalse(TEXT("points inside the grenade radius are rejected"), DangerousResult.bAccepted);
	TestEqual(TEXT("grenade rejection is diagnostic"), DangerousResult.RejectionReason, FName(TEXT("GrenadeDanger")));
	TestTrue(TEXT("points outside the grenade radius remain eligible"), SafeResult.bAccepted);
	TestTrue(
		TEXT("escape scoring rewards additional distance beyond the danger radius"),
		SafeResult.Score > AHTacticalScoring::ScoreCandidate(MarginallySafe, Params).Score);
	return true;
}

#endif
