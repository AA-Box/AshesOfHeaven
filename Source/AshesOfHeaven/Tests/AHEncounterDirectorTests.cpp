#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Characters/AHVeilWardenCharacter.h"
#include "Gameplay/Encounters/AHEncounterDirectorSubsystem.h"
#include "Gameplay/Enemies/AHEncounterDefinition.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	FPrimaryAssetId ArchetypeId(const TCHAR* Name)
	{
		return AHEnemyAssets::EnemyId(FName(Name));
	}

	UAHEnemyDefinition* MakeArchetype(const TCHAR* Name, float Cost)
	{
		UAHEnemyDefinition* Archetype = NewObject<UAHEnemyDefinition>();
		Archetype->EnemyId = FName(Name);
		Archetype->CombatClass = AAHVeilPilgrimCharacter::StaticClass();
		Archetype->Difficulty.ThreatCost = Cost;
		return Archetype;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterDeterministicSelectionTest, "AshesOfHeaven.EncounterDirector.DeterministicSelection", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterDeterministicSelectionTest::RunTest(const FString& Parameters)
{
	const FPrimaryAssetId PilgrimId = ArchetypeId(TEXT("Pilgrim"));
	const FPrimaryAssetId WardenId = ArchetypeId(TEXT("Warden"));
	TArray<FAHEncounterEnemyPoolEntry> Pool;
	Pool.Add({PilgrimId, 1.0f, 1.0f, 0, 0, 1.0f, 1.0f});
	Pool.Add({WardenId, 3.0f, 1.0f, 0, 0, 1.0f, 1.0f});
	TMap<FPrimaryAssetId, TObjectPtr<UAHEnemyDefinition>> Archetypes;
	Archetypes.Add(PilgrimId, MakeArchetype(TEXT("Pilgrim"), 1.0f));
	Archetypes.Add(WardenId, MakeArchetype(TEXT("Warden"), 1.0f));
	TMap<FPrimaryAssetId, int32> Counts;
	FRandomStream First(81421);
	FRandomStream Second(81421);
	for (int32 Draw = 0; Draw < 32; ++Draw)
	{
		const int32 A = UAHEncounterDirectorSubsystem::SelectWeightedPoolEntry(Pool, Archetypes, Counts, 0, EAHEncounterDifficulty::Soldier, 20.0f, 0.0f, 20.0f, First);
		const int32 B = UAHEncounterDirectorSubsystem::SelectWeightedPoolEntry(Pool, Archetypes, Counts, 0, EAHEncounterDifficulty::Soldier, 20.0f, 0.0f, 20.0f, Second);
		TestEqual(TEXT("same seed produces the same composition draw"), A, B);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterCreditCalculationTest, "AshesOfHeaven.EncounterDirector.SpawnCreditCalculation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterCreditCalculationTest::RunTest(const FString& Parameters)
{
	UAHEnemyDefinition* Archetype = MakeArchetype(TEXT("Pilgrim"), 2.0f);
	FAHEncounterEnemyPoolEntry Entry;
	Entry.SpawnCostOverride = -1.0f;
	TestEqual(TEXT("archetype cost is used by default"), UAHEncounterDirectorSubsystem::CalculateSpawnCost(Entry, Archetype), 2.0f);
	Entry.SpawnCostOverride = 1.25f;
	TestEqual(TEXT("encounter cost override wins"), UAHEncounterDirectorSubsystem::CalculateSpawnCost(Entry, Archetype), 1.25f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterWeightingTest, "AshesOfHeaven.EncounterDirector.Weighting", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterWeightingTest::RunTest(const FString& Parameters)
{
	const FPrimaryAssetId PilgrimId = ArchetypeId(TEXT("Pilgrim"));
	const FPrimaryAssetId WardenId = ArchetypeId(TEXT("Warden"));
	TArray<FAHEncounterEnemyPoolEntry> Pool;
	Pool.Add({PilgrimId, 1.0f, 1.0f, 0, 0, 1.0f, 1.0f});
	Pool.Add({WardenId, 9.0f, 1.0f, 0, 0, 1.1f, 1.25f});
	TMap<FPrimaryAssetId, TObjectPtr<UAHEnemyDefinition>> Archetypes;
	Archetypes.Add(PilgrimId, MakeArchetype(TEXT("Pilgrim"), 1.0f));
	Archetypes.Add(WardenId, MakeArchetype(TEXT("Warden"), 1.0f));
	TMap<FPrimaryAssetId, int32> Counts;
	FRandomStream Stream(77);
	int32 WardenDraws = 0;
	for (int32 Draw = 0; Draw < 200; ++Draw)
	{
		WardenDraws += UAHEncounterDirectorSubsystem::SelectWeightedPoolEntry(Pool, Archetypes, Counts, 0, EAHEncounterDifficulty::Soldier, 10.0f, 0.0f, 10.0f, Stream) == 1 ? 1 : 0;
	}
	TestTrue(TEXT("higher authored weight dominates deterministic samples"), WardenDraws > 150);
	TestTrue(TEXT("Veteran Warden modifier increases effective weight"), UAHEncounterDirectorSubsystem::CalculateEffectiveWeight(Pool[1], EAHEncounterDifficulty::Veteran) > Pool[1].Weight);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterBudgetGateTest, "AshesOfHeaven.EncounterDirector.NoOverBudgetSpawn", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterBudgetGateTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("affordable spawn is admitted"), UAHEncounterDirectorSubsystem::CanSpend(4.0f, 12.0f, 16.0f, 4.0f));
	TestFalse(TEXT("insufficient credits reject spawn"), UAHEncounterDirectorSubsystem::CanSpend(3.9f, 8.0f, 16.0f, 4.0f));
	TestFalse(TEXT("lifetime budget rejects spawn"), UAHEncounterDirectorSubsystem::CanSpend(10.0f, 13.0f, 16.0f, 4.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterActiveCapTest, "AshesOfHeaven.EncounterDirector.MaximumActiveEnemies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterActiveCapTest::RunTest(const FString& Parameters)
{
	UAHEncounterDefinition* Definition = NewObject<UAHEncounterDefinition>();
	Definition->MaximumActiveEnemies = 8;
	Definition->MobileMaximumActiveEnemies = 5;
	FAHEncounterDifficultyModifier Modifier;
	Modifier.MaximumActiveEnemyDelta = 2;
	TestEqual(TEXT("desktop encounter cap includes difficulty pressure"), UAHEncounterDirectorSubsystem::CalculateActiveEnemyCap(*Definition, Modifier, 24, false), 10);
	TestEqual(TEXT("global platform cap remains authoritative"), UAHEncounterDirectorSubsystem::CalculateActiveEnemyCap(*Definition, Modifier, 6, false), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterPhaseTransitionTest, "AshesOfHeaven.EncounterDirector.PhaseTransition", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterPhaseTransitionTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("three of five does not arm half-force reinforcement"), UAHEncounterDirectorSubsystem::ShouldTriggerForceRemaining(3, 5, 0.5f));
	TestTrue(TEXT("two of five arms half-force reinforcement"), UAHEncounterDirectorSubsystem::ShouldTriggerForceRemaining(2, 5, 0.5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterCheckpointRestoreTest, "AshesOfHeaven.EncounterDirector.CheckpointRestoration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterCheckpointRestoreTest::RunTest(const FString& Parameters)
{
	UAHSaveGame* Save = NewObject<UAHSaveGame>();
	Save->CombatState.bValid = true;
	FAHEncounterCheckpointState& Encounter = Save->CombatState.EncounterState;
	Encounter.bValid = true;
	Encounter.EncounterId = FName(TEXT("Erebus_DefensiveLine"));
	Encounter.PhaseIndex = 1;
	Encounter.DeterministicSeed = 71337;
	Encounter.RandomDrawCountAtPhaseStart = 9;
	Encounter.TotalSpentBeforePhase = 8.0f;
	Encounter.ScriptedTriggers.Add(FName(TEXT("WestLaneOpened")));
	TArray<uint8> Bytes;
	TestTrue(TEXT("encounter checkpoint serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("encounter checkpoint deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("phase survives restore"), Loaded->CombatState.EncounterState.PhaseIndex, 1);
		TestEqual(TEXT("seed survives restore"), Loaded->CombatState.EncounterState.DeterministicSeed, 71337);
		TestEqual(TEXT("spent budget survives restore"), Loaded->CombatState.EncounterState.TotalSpentBeforePhase, 8.0f);
		TestTrue(TEXT("scripted trigger state survives restore"), Loaded->CombatState.EncounterState.ScriptedTriggers.Contains(FName(TEXT("WestLaneOpened"))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterInvalidEQSTest, "AshesOfHeaven.EncounterDirector.InvalidEQSResult", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterInvalidEQSTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("failed EQS is rejected"), UAHEncounterDirectorSubsystem::IsQueryResultUsable(false, 4));
	TestFalse(TEXT("empty EQS is rejected"), UAHEncounterDirectorSubsystem::IsQueryResultUsable(true, 0));
	TestTrue(TEXT("successful non-empty EQS is usable"), UAHEncounterDirectorSubsystem::IsQueryResultUsable(true, 1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterUnavailableAssetTest, "AshesOfHeaven.EncounterDirector.UnavailableAsset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterUnavailableAssetTest::RunTest(const FString& Parameters)
{
	TestFalse(TEXT("missing archetype is unavailable"), UAHEncounterDirectorSubsystem::IsArchetypeAvailable(nullptr));
	UAHEnemyDefinition* MissingClass = NewObject<UAHEnemyDefinition>();
	TestFalse(TEXT("archetype without combatant class is unavailable"), UAHEncounterDirectorSubsystem::IsArchetypeAvailable(MissingClass));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterDifficultyTest, "AshesOfHeaven.EncounterDirector.DifficultyModifier", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterDifficultyTest::RunTest(const FString& Parameters)
{
	UAHEncounterDefinition* Definition = NewObject<UAHEncounterDefinition>();
	FAHEncounterDifficultyModifier Veteran;
	Veteran.Difficulty = EAHEncounterDifficulty::Veteran;
	Veteran.TacticalBudgetMultiplier = 1.15f;
	Veteran.ReinforcementDelayMultiplier = 0.9f;
	Veteran.AISophisticationMultiplier = 1.1f;
	Definition->DifficultyModifiers.Add(Veteran);
	const FAHEncounterDifficultyModifier& Resolved = Definition->GetDifficultyModifier(EAHEncounterDifficulty::Veteran);
	TestEqual(TEXT("Veteran adds fifteen percent tactical budget"), Resolved.TacticalBudgetMultiplier, 1.15f);
	TestTrue(TEXT("Veteran reinforces sooner"), Resolved.ReinforcementDelayMultiplier < 1.0f);
	TestTrue(TEXT("Veteran raises AI sophistication"), Resolved.AISophisticationMultiplier > 1.0f);
	TestEqual(TEXT("Damnation save value resolves without health scaling"), UAHEncounterDirectorSubsystem::DifficultyFromSaveValue(3), EAHEncounterDifficulty::Damnation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterPlatformCapTest, "AshesOfHeaven.EncounterDirector.PlatformAICap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterPlatformCapTest::RunTest(const FString& Parameters)
{
	UAHEncounterDefinition* Definition = NewObject<UAHEncounterDefinition>();
	Definition->MaximumActiveEnemies = 9;
	Definition->MobileMaximumActiveEnemies = 5;
	FAHEncounterDifficultyModifier Modifier;
	TestEqual(TEXT("desktop receives authored battlefield population"), UAHEncounterDirectorSubsystem::CalculateActiveEnemyCap(*Definition, Modifier, 24, false), 9);
	TestEqual(TEXT("mobile sequences the same encounter through five active AI"), UAHEncounterDirectorSubsystem::CalculateActiveEnemyCap(*Definition, Modifier, 16, true), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterCompletionTest, "AshesOfHeaven.EncounterDirector.EncounterCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterCompletionTest::RunTest(const FString& Parameters)
{
	UAHEncounterDefinition* Definition = NewObject<UAHEncounterDefinition>();
	Definition->CompletionRule = EAHEncounterCompletionRule::AllPhasesAndEnemiesDefeated;
	Definition->Phases.SetNum(2);
	TSet<FName> Scripts;
	TestFalse(TEXT("non-final phase cannot complete"), UAHEncounterDirectorSubsystem::ShouldCompleteEncounter(*Definition, 0, true, 0, false, Scripts));
	TestFalse(TEXT("living enemies block completion"), UAHEncounterDirectorSubsystem::ShouldCompleteEncounter(*Definition, 1, true, 1, false, Scripts));
	TestTrue(TEXT("final dispatched phase with no living enemies completes"), UAHEncounterDirectorSubsystem::ShouldCompleteEncounter(*Definition, 1, true, 0, false, Scripts));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterAssetManifestTest, "AshesOfHeaven.EncounterDirector.AuthoredAssetManifest", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterAssetManifestTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Erebus opening definition exists"), LoadObject<UAHEncounterDefinition>(nullptr, TEXT("/Game/Ashes/Data/Encounters/DA_Encounter_ErebusOpening.DA_Encounter_ErebusOpening")));
	UAHEncounterDefinition* DefensiveLine = LoadObject<UAHEncounterDefinition>(nullptr, TEXT("/Game/Ashes/Data/Encounters/DA_Encounter_DefensiveLine.DA_Encounter_DefensiveLine"));
	TestNotNull(TEXT("Defensive Line definition exists"), DefensiveLine);
	TestNotNull(TEXT("Cathedral approach definition exists"), LoadObject<UAHEncounterDefinition>(nullptr, TEXT("/Game/Ashes/Data/Encounters/DA_Encounter_CathedralApproach.DA_Encounter_CathedralApproach")));
	if (DefensiveLine)
	{
		TestEqual(TEXT("Defensive Line is the migrated encounter ID"), DefensiveLine->EncounterId, FName(TEXT("Erebus_DefensiveLine")));
		TestEqual(TEXT("Defensive Line owns sixteen tactical budget"), DefensiveLine->EnemyBudget, 16.0f);
		TestEqual(TEXT("Defensive Line has two authored phases"), DefensiveLine->Phases.Num(), 2);
		TestNotNull(TEXT("Defensive Line has an EQS spawn query"), DefensiveLine->SpawnQuery.Get());
		TestFalse(TEXT("Defensive Line spawn regions are explicitly bounded"), DefensiveLine->AllowedSpawnRegions.IsEmpty());
	}
	return true;
}

#endif
