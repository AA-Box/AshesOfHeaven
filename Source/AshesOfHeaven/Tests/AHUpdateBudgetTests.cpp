#include "Misc/AutomationTest.h"

#include "Performance/AHUpdateBudgetSubsystem.h"
#include "GameFramework/Actor.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHSignificanceTierTransitionsTest,
	"AshesOfHeaven.Performance.SignificanceTierTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHSignificanceTierTransitionsTest::RunTest(const FString& Parameters)
{
	const FAHUpdateBudgetPolicy Policy = UAHUpdateBudgetSubsystem::BuildPolicy(FAHPerformanceProfile(), false);
	FAHSignificanceInput Input;
	Input.DistanceSquared = FMath::Square(1000.0f);
	TestEqual(TEXT("Nearby actor is Near"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Near);

	Input.DistanceSquared = FMath::Square(4000.0f);
	TestEqual(TEXT("Mid-distance actor is Mid"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Mid);

	Input.DistanceSquared = FMath::Square(9000.0f);
	TestEqual(TEXT("Distant actor is Far"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Far);

	Input.DistanceSquared = FMath::Square(20000.0f);
	TestEqual(TEXT("Offstage actor is Dormant"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Dormant);
	Input.bVisible = true;
	TestEqual(TEXT("Visible actor cannot fall below Mid"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Mid);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHSignificanceProtectionTest,
	"AshesOfHeaven.Performance.SignificanceProtections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHSignificanceProtectionTest::RunTest(const FString& Parameters)
{
	const FAHUpdateBudgetPolicy Policy = UAHUpdateBudgetSubsystem::BuildPolicy(FAHPerformanceProfile(), false);
	FAHSignificanceInput Input;
	Input.DistanceSquared = FMath::Square(50000.0f);
	Input.bActiveEncounterMember = true;
	TestEqual(TEXT("Active encounter combatant is never Dormant"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Far);

	Input.bCurrentAttacker = true;
	TestEqual(TEXT("Current attacker is Near"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Near);
	Input.bCurrentAttacker = false;
	Input.bGrenadeThreat = true;
	TestEqual(TEXT("Grenade-threat combatant is Near"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Near);
	Input.bGrenadeThreat = false;
	Input.bObjectiveRelevant = true;
	TestEqual(TEXT("Objective actor is Near"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Near);
	Input.bObjectiveRelevant = false;
	Input.bNarrativeRelevant = true;
	TestEqual(TEXT("Narrative actor is Near"), UAHUpdateBudgetSubsystem::EvaluateTier(Input, Policy), EAHSignificanceTier::Near);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHPlatformUpdateBudgetTest,
	"AshesOfHeaven.Performance.PlatformUpdateBudgets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHPlatformUpdateBudgetTest::RunTest(const FString& Parameters)
{
	FAHPerformanceProfile DesktopProfile;
	const FAHUpdateBudgetPolicy Desktop = UAHUpdateBudgetSubsystem::BuildPolicy(DesktopProfile, false);

	FAHPerformanceProfile MobileProfile;
	MobileProfile.MaxActiveCombatants = 8;
	MobileProfile.MaxMidDistanceActors = 24;
	MobileProfile.MaxDistantSimulationActors = 48;
	MobileProfile.MidDistanceTickInterval = 0.20f;
	const FAHUpdateBudgetPolicy Mobile = UAHUpdateBudgetSubsystem::BuildPolicy(MobileProfile, true);

	TestTrue(TEXT("Desktop has a larger Near budget"), Desktop.MaxNearActors > Mobile.MaxNearActors);
	TestTrue(TEXT("Desktop has a larger Mid budget"), Desktop.MaxMidActors > Mobile.MaxMidActors);
	TestTrue(TEXT("Mobile Far simulation is slower"), Mobile.FarRates.DistantBattlefieldSimulation > Desktop.FarRates.DistantBattlefieldSimulation);
	TestTrue(TEXT("Mobile cosmetics are throttled more aggressively"), Mobile.MidRates.CosmeticEffects > Desktop.MidRates.CosmeticEffects);
	TestTrue(TEXT("Mobile significance distances are shorter"), Mobile.MidDistance < Desktop.MidDistance);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHDeterministicUpdateScheduleTest,
	"AshesOfHeaven.Performance.DeterministicThrottledSchedule",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHDeterministicUpdateScheduleTest::RunTest(const FString& Parameters)
{
	const auto CountUpdates = [](int32 FramesPerSecond)
	{
		double NextDue = 0.0;
		double LastUpdate = 0.0;
		int32 UpdateCount = 0;
		float SimulatedSeconds = 0.0f;
		for (int32 Frame = 0; Frame <= FramesPerSecond * 10; ++Frame)
		{
			const double Now = static_cast<double>(Frame) / FramesPerSecond;
			float UpdateDelta = 0.0f;
			if (UAHUpdateBudgetSubsystem::ConsumeInterval(Now, 0.5f, NextDue, LastUpdate, UpdateDelta))
			{
				++UpdateCount;
				SimulatedSeconds += UpdateDelta;
			}
		}
		return TPair<int32, float>(UpdateCount, SimulatedSeconds);
	};

	const TPair<int32, float> At30Hz = CountUpdates(30);
	const TPair<int32, float> At60Hz = CountUpdates(60);
	TestEqual(TEXT("Update count is frame-rate independent"), At30Hz.Key, At60Hz.Key);
	TestTrue(TEXT("Accumulated throttled time is deterministic"), FMath::IsNearlyEqual(At30Hz.Value, At60Hz.Value, KINDA_SMALL_NUMBER));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHUpdateBudgetRegistrationCleanupTest,
	"AshesOfHeaven.Performance.RegistrationCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHUpdateBudgetRegistrationCleanupTest::RunTest(const FString& Parameters)
{
	UAHUpdateBudgetSubsystem* Subsystem = NewObject<UAHUpdateBudgetSubsystem>();
	AActor* Subject = NewObject<AActor>();
	AActor* TickOwner = NewObject<AActor>();
	Subsystem->RegisterCombatant(Subject, TickOwner);
	TestEqual(TEXT("Registration is tracked"), Subsystem->GetRegisteredCombatantCount(), 1);
	Subsystem->UnregisterCombatant(Subject);
	TestEqual(TEXT("EndPlay-style cleanup removes registration"), Subsystem->GetRegisteredCombatantCount(), 0);
	return true;
}

#endif
