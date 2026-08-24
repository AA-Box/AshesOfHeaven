#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Misc/AutomationTest.h"

namespace
{
	FAHInteractionCandidateMetrics MakeCandidate(float Distance = 150.0f)
	{
		FAHInteractionCandidateMetrics Candidate;
		Candidate.Distance = Distance;
		Candidate.MaxDistance = 350.0f;
		Candidate.ViewDot = 1.0f;
		Candidate.MinimumViewDot = AHInteractionTargeting::MinimumViewDot(16.0f);
		Candidate.ScreenCenterDistance = 0.0f;
		return Candidate;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionDirectTargetWinsTest,
	"AshesOfHeaven.Interaction.DirectTargetWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionDirectTargetWinsTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	FAHInteractionCandidateMetrics Direct = MakeCandidate(260.0f);
	Direct.bDirectHit = true;
	FAHInteractionCandidateMetrics NearbyObjective = MakeCandidate(100.0f);
	NearbyObjective.InteractionPriority = 0.6f;
	NearbyObjective.ObjectivePriority = 1.0f;

	TestTrue(TEXT("A valid crosshair hit outranks a nearby high-priority fallback"),
		AHInteractionTargeting::ScoreCandidate(Direct, Weights) > AHInteractionTargeting::ScoreCandidate(NearbyObjective, Weights));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionClosestNotAlwaysBestTest,
	"AshesOfHeaven.Interaction.ClosestIsNotAlwaysBest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionClosestNotAlwaysBestTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	FAHInteractionCandidateMetrics CloseEdge = MakeCandidate(60.0f);
	CloseEdge.ViewDot = AHInteractionTargeting::MinimumViewDot(16.0f) + 0.001f;
	CloseEdge.ScreenCenterDistance = 0.30f;
	FAHInteractionCandidateMetrics FarCentered = MakeCandidate(210.0f);

	TestTrue(TEXT("Aim alignment can outweigh raw proximity"),
		AHInteractionTargeting::ScoreCandidate(FarCentered, Weights) > AHInteractionTargeting::ScoreCandidate(CloseEdge, Weights));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionOccludedCandidateLosesTest,
	"AshesOfHeaven.Interaction.OccludedCandidateLoses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionOccludedCandidateLosesTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	FAHInteractionCandidateMetrics Occluded = MakeCandidate(80.0f);
	Occluded.bVisible = false;
	Occluded.ObjectivePriority = 10.0f;
	const float Score = AHInteractionTargeting::ScoreCandidate(Occluded, Weights);

	TestFalse(TEXT("Occlusion is an eligibility failure, not a soft rendering hint"), AHInteractionTargeting::IsValidScore(Score));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionObjectivePriorityTest,
	"AshesOfHeaven.Interaction.ObjectivePriority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionObjectivePriorityTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	FAHInteractionCandidateMetrics Regular = MakeCandidate(130.0f);
	Regular.InteractionPriority = 0.5f;
	FAHInteractionCandidateMetrics Objective = MakeCandidate(170.0f);
	Objective.InteractionPriority = 0.5f;
	Objective.ObjectivePriority = 1.0f;

	TestTrue(TEXT("Actor-authored objective relevance can beat a slightly nearer regular interaction"),
		AHInteractionTargeting::ScoreCandidate(Objective, Weights) > AHInteractionTargeting::ScoreCandidate(Regular, Weights));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionStickySelectionTest,
	"AshesOfHeaven.Interaction.StickySelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionStickySelectionTest::RunTest(const FString& Parameters)
{
	const float CurrentScore = 600.0f;
	TestFalse(TEXT("A marginal challenger does not replace the current target"),
		AHInteractionTargeting::ShouldReplaceCurrent(CurrentScore, CurrentScore + 59.0f, 60.0f));
	TestTrue(TEXT("A challenger that clears the switch margin replaces it"),
		AHInteractionTargeting::ShouldReplaceCurrent(CurrentScore, CurrentScore + 60.0f, 60.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionCorpseVsWeaponTest,
	"AshesOfHeaven.Interaction.CorpseVsDroppedWeapon",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionCorpseVsWeaponTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	FAHInteractionCandidateMetrics Corpse = MakeCandidate(145.0f);
	Corpse.InteractionPriority = 0.20f;
	FAHInteractionCandidateMetrics DroppedWeapon = Corpse;
	DroppedWeapon.InteractionPriority = 0.50f;

	TestTrue(TEXT("The dropped weapon's actor-authored priority resolves an otherwise identical overlap"),
		AHInteractionTargeting::ScoreCandidate(DroppedWeapon, Weights) > AHInteractionTargeting::ScoreCandidate(Corpse, Weights));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionEmptyPromptIgnoredTest,
	"AshesOfHeaven.Interaction.EmptyPromptIgnored",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionEmptyPromptIgnoredTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	FAHInteractionCandidateMetrics EmptyPrompt = MakeCandidate(40.0f);
	EmptyPrompt.bActionable = false;
	EmptyPrompt.bDirectHit = true;
	EmptyPrompt.ObjectivePriority = 10.0f;

	TestFalse(TEXT("An empty prompt cannot become a target even under the crosshair"),
		AHInteractionTargeting::IsValidScore(AHInteractionTargeting::ScoreCandidate(EmptyPrompt, Weights)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionRemovedTargetTest,
	"AshesOfHeaven.Interaction.RemovedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionRemovedTargetTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("A valid candidate replaces a removed or otherwise invalid current target immediately"),
		AHInteractionTargeting::ShouldReplaceCurrent(-MAX_flt, 400.0f, 60.0f));
	TestFalse(TEXT("An invalid candidate cannot replace anything"),
		AHInteractionTargeting::ShouldReplaceCurrent(-MAX_flt, -MAX_flt, 60.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInteractionControllerToleranceTest,
	"AshesOfHeaven.Interaction.ControllerTolerance",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInteractionControllerToleranceTest::RunTest(const FString& Parameters)
{
	const FAHInteractionScoreWeights Weights;
	const float CandidateDot = FMath::Cos(FMath::DegreesToRadians(22.0f));
	FAHInteractionCandidateMetrics MouseCandidate = MakeCandidate();
	MouseCandidate.ViewDot = CandidateDot;
	MouseCandidate.MinimumViewDot = AHInteractionTargeting::MinimumViewDot(16.0f);
	FAHInteractionCandidateMetrics AssistedCandidate = MouseCandidate;
	AssistedCandidate.MinimumViewDot = AHInteractionTargeting::MinimumViewDot(28.0f);

	TestFalse(TEXT("The candidate is outside the mouse cone"),
		AHInteractionTargeting::IsValidScore(AHInteractionTargeting::ScoreCandidate(MouseCandidate, Weights)));
	TestTrue(TEXT("The same candidate is inside the controller/touch cone"),
		AHInteractionTargeting::IsValidScore(AHInteractionTargeting::ScoreCandidate(AssistedCandidate, Weights)));
	return true;
}

#endif
