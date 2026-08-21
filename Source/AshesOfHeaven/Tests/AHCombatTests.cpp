#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHHealthDamageTest, "AshesOfHeaven.Combat.HealthDamage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHHealthDamageTest::RunTest(const FString& Parameters)
{
	UAHHealthComponent* Health = NewObject<UAHHealthComponent>();
	Health->MaxHealth = 100.0f;
	Health->ResetHealth();
	TestEqual(TEXT("Health starts full"), Health->GetHealth(), 100.0f);
	TestEqual(TEXT("Damage is applied"), Health->ApplyDamage(35.0f), 35.0f);
	TestEqual(TEXT("Health is reduced"), Health->GetHealth(), 65.0f);
	TestTrue(TEXT("Lethal damage marks dead"), Health->ApplyDamage(100.0f) > 0.0f && Health->IsDead());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHArmorTimingTest, "AshesOfHeaven.Combat.ArmorAbsorptionAndRegenTiming", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHArmorTimingTest::RunTest(const FString& Parameters)
{
	UAHArmorComponent* Armor = NewObject<UAHArmorComponent>();
	Armor->MaxArmor = 100.0f;
	Armor->RegenerationDelay = 4.0f;
	Armor->ResetArmor();
	TestEqual(TEXT("Armor absorbs incoming damage"), Armor->AbsorbDamage(35.0f), 35.0f);
	TestEqual(TEXT("Armor is reduced before health"), Armor->GetArmor(), 65.0f);
	TestEqual(TEXT("Regeneration delay is enforced"), Armor->GetTimeUntilRegeneration(2.0f), 2.0f);
	TestEqual(TEXT("Regeneration becomes available after delay"), Armor->GetTimeUntilRegeneration(4.0f), 0.0f);
	float ArmorDamage = 0.0f;
	TestEqual(TEXT("Pure armor rule returns health damage"), UAHCombatRulesLibrary::ApplyArmorAbsorption(120.0f, 50.0f, ArmorDamage), 70.0f);
	TestEqual(TEXT("Pure armor rule consumes remaining armor"), ArmorDamage, 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHAmmoReloadTest, "AshesOfHeaven.Combat.AmmoConsumptionAndReloadCalculation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHAmmoReloadTest::RunTest(const FString& Parameters)
{
	FAHAmmoState Ammo;
	Ammo.MagazineCapacity = 36;
	Ammo.Magazine = 11;
	Ammo.Reserve = 18;
	TestEqual(TEXT("Reload transfers only missing rounds"), UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo), 18);
	Ammo.Reserve = 100;
	TestEqual(TEXT("Reload never exceeds magazine capacity"), UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo), 25);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHGrenadeInventoryTest, "AshesOfHeaven.Combat.GrenadeInventory", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHGrenadeInventoryTest::RunTest(const FString& Parameters)
{
	UAHInventoryComponent* Inventory = NewObject<UAHInventoryComponent>();
	Inventory->MaximumGrenades = 4;
	Inventory->AddGrenades(2);
	TestEqual(TEXT("Grenade pickup replenishes inventory"), Inventory->GetGrenades(), 2);
	TestTrue(TEXT("Grenade can be consumed"), Inventory->ConsumeGrenade());
	TestEqual(TEXT("Consumed grenade is removed"), Inventory->GetGrenades(), 1);
	Inventory->AddGrenades(10);
	TestEqual(TEXT("Grenade inventory is capped"), Inventory->GetGrenades(), 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjectiveTransitionTest, "AshesOfHeaven.Combat.ObjectiveTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjectiveTransitionTest::RunTest(const FString& Parameters)
{
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	const TArray<FName> ObjectiveChain = {
		FName(TEXT("ReachDefensivePosition")),
		FName(TEXT("EliminateVeilAssault")),
		FName(TEXT("AdvanceThroughBreach")),
		FName(TEXT("DefendEvacuationGate")),
		FName(TEXT("ReachExtraction"))
	};
	TestEqual(TEXT("Five objectives are configured"), Objectives->GetObjectiveCount(), ObjectiveChain.Num());
	for (int32 Index = 0; Index < ObjectiveChain.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Objective %d is active"), Index + 1), Objectives->IsCurrentObjective(ObjectiveChain[Index]));
		TestTrue(*FString::Printf(TEXT("Objective %d completes"), Index + 1), Objectives->CompleteObjective(ObjectiveChain[Index]));
	}
	TestTrue(TEXT("Mission completion is reachable after the fifth objective"), Objectives->IsMissionComplete());
	TestEqual(TEXT("All objectives are recorded as completed"), Objectives->GetCompletedObjectiveIds().Num(), ObjectiveChain.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHObjectiveRestoreStateTest, "AshesOfHeaven.Combat.ObjectiveRestoreState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHObjectiveRestoreStateTest::RunTest(const FString& Parameters)
{
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	Objectives->RestoreState(2);
	TestTrue(TEXT("Checkpoint restores the current objective"), Objectives->IsCurrentObjective(FName(TEXT("AdvanceThroughBreach"))));
	TestEqual(TEXT("Checkpoint restores completed objective history"), Objectives->GetCompletedObjectiveIds().Num(), 2);
	TestFalse(TEXT("Partial checkpoint is not mission complete"), Objectives->IsMissionComplete());
	Objectives->RestoreState(5);
	TestTrue(TEXT("Completed checkpoint restores mission completion"), Objectives->IsMissionComplete());
	TestEqual(TEXT("Completed checkpoint restores all objective history"), Objectives->GetCompletedObjectiveIds().Num(), 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCheckpointSerializationTest, "AshesOfHeaven.Combat.CheckpointSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCheckpointSerializationTest::RunTest(const FString& Parameters)
{
	UAHSaveGame* Save = NewObject<UAHSaveGame>();
	Save->CombatState.bValid = true;
	Save->CombatState.CheckpointId = FName(TEXT("Checkpoint_2"));
	Save->CombatState.Health = 73.0f;
	Save->CombatState.Armor = 41.0f;
	Save->CombatState.Ammo.Magazine = 19;
	Save->CombatState.Ammo.Reserve = 94;
	Save->CombatState.Grenades = 3;
	Save->CombatState.ObjectiveIndex = 2;
	Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_One")));
	Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_Two")));

	TArray<uint8> Bytes;
	TestTrue(TEXT("Checkpoint save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Checkpoint save deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("Checkpoint id survives serialization"), Loaded->CombatState.CheckpointId, FName(TEXT("Checkpoint_2")));
		TestEqual(TEXT("Health survives serialization"), Loaded->CombatState.Health, 73.0f);
		TestEqual(TEXT("Ammo survives serialization"), Loaded->CombatState.Ammo.Magazine, 19);
		TestEqual(TEXT("Grenade state survives serialization"), Loaded->CombatState.Grenades, 3);
		TestEqual(TEXT("Objective state survives serialization"), Loaded->CombatState.ObjectiveIndex, 2);
		TestTrue(TEXT("Encounter one progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_One"))));
		TestTrue(TEXT("Encounter two progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_Two"))));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEncounterConfigurationTest, "AshesOfHeaven.Combat.EncounterConfiguration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEncounterConfigurationTest::RunTest(const FString& Parameters)
{
	AAHCombatEncounter* Encounter = NewObject<AAHCombatEncounter>();
	TestTrue(TEXT("Encounter enemies default to Veil Pilgrims"), Encounter->EnemyClass == AAHVeilPilgrimCharacter::StaticClass());
	TestFalse(TEXT("New encounter is inactive"), Encounter->IsActive());
	TestFalse(TEXT("New encounter is incomplete"), Encounter->IsComplete());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHFactionHostilityTest, "AshesOfHeaven.Combat.FactionHostility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHFactionHostilityTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Player is hostile to Veil"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Player, EAHFaction::Veil));
	TestTrue(TEXT("Veil is hostile to player"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Veil, EAHFaction::Player));
	TestTrue(TEXT("Human is hostile to Veil"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Human, EAHFaction::Veil));
	TestFalse(TEXT("Human allies are not hostile"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Human, EAHFaction::Player));
	TestFalse(TEXT("Neutral is not hostile"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Neutral, EAHFaction::Veil));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterStageOrderingTest, "AshesOfHeaven.Chapter.StageOrdering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterStageOrderingTest::RunTest(const FString& Parameters)
{
	const TArray<EAHChapterStage> Stages = {
		EAHChapterStage::OpeningBlack, EAHChapterStage::ErebusOpening, EAHChapterStage::OpeningBattle,
		EAHChapterStage::TransitStation, EAHChapterStage::VeilRevelation, EAHChapterStage::OpenBattlefield,
		EAHChapterStage::ManticoreSection, EAHChapterStage::CathedralApproach, EAHChapterStage::FailsafeOrder,
		EAHChapterStage::CathedralInterior, EAHChapterStage::SaelTransmission, EAHChapterStage::FailsafeTerminal,
		EAHChapterStage::Escape, EAHChapterStage::OtherLucian, EAHChapterStage::ErebusDestruction,
		EAHChapterStage::TenYearsLater, EAHChapterStage::MayaScene, EAHChapterStage::NysaTransmission,
		EAHChapterStage::FleetDeparture, EAHChapterStage::StarsDisappearing, EAHChapterStage::ChapterComplete
	};
	TestEqual(TEXT("All authored Chapter stages are present"), Stages.Num(), 21);
	for (int32 Index = 1; Index < Stages.Num(); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("Stage %d follows stage %d"), Index, Index - 1), static_cast<uint8>(Stages[Index]) > static_cast<uint8>(Stages[Index - 1]));
	}
	TestEqual(TEXT("ChapterComplete is the terminal stage"), Stages.Last(), EAHChapterStage::ChapterComplete);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterObjectiveChainTest, "AshesOfHeaven.Chapter.ObjectiveChain", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterObjectiveChainTest::RunTest(const FString& Parameters)
{
	UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
	TArray<FAHObjectiveDefinition> Definitions;
	for (int32 Index = 0; Index < 17; ++Index)
	{
		Definitions.Add({FName(*FString::Printf(TEXT("Ch01_Objective_%02d"), Index + 1)), FText::FromString(FString::Printf(TEXT("CHAPTER OBJECTIVE %02d"), Index + 1)), FText::FromString(TEXT("Greybox verification objective."))});
	}
	Objectives->ConfigureObjectives(Definitions, 0);
	TestEqual(TEXT("Chapter objective chain contains seventeen objectives"), Objectives->GetObjectiveCount(), 17);
	for (const FAHObjectiveDefinition& Definition : Definitions)
	{
		TestTrue(*FString::Printf(TEXT("Completes %s"), *Definition.Id.ToString()), Objectives->CompleteObjective(Definition.Id));
	}
	TestTrue(TEXT("Chapter completion is reachable"), Objectives->IsMissionComplete());
	TestEqual(TEXT("All Chapter objective history is retained"), Objectives->GetCompletedObjectiveIds().Num(), 17);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterStateSerializationTest, "AshesOfHeaven.Chapter.StateSerialization", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterStateSerializationTest::RunTest(const FString& Parameters)
{
	UAHSaveGame* Save = NewObject<UAHSaveGame>();
	Save->CombatState.bValid = true;
	Save->CombatState.MapName = TEXT("L_ChapterOne_Greybox");
	Save->CombatState.CheckpointId = FName(TEXT("Ch01_Checkpoint_03"));
	Save->CombatState.ObjectiveIndex = 7;
	Save->CombatState.Ammo.Magazine = 14;
	Save->CombatState.Ammo.Reserve = 121;
	Save->CombatState.Grenades = 1;
	Save->CombatState.ChapterState.Stage = EAHChapterStage::FailsafeOrder;
	Save->CombatState.ChapterState.CountdownSeconds = 401.5f;
	Save->CombatState.ChapterState.bCountdownActive = true;
	Save->CombatState.ChapterState.Vehicle.bSpawned = true;
	Save->CombatState.ChapterState.Vehicle.Health = 287.0f;
	TArray<uint8> Bytes;
	TestTrue(TEXT("Chapter checkpoint serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Chapter checkpoint deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("Chapter stage survives restore"), Loaded->CombatState.ChapterState.Stage, EAHChapterStage::FailsafeOrder);
		TestEqual(TEXT("Chapter countdown survives restore"), Loaded->CombatState.ChapterState.CountdownSeconds, 401.5f);
		TestEqual(TEXT("Ammo survives restore"), Loaded->CombatState.Ammo.Magazine, 14);
		TestEqual(TEXT("Reserve ammo survives restore"), Loaded->CombatState.Ammo.Reserve, 121);
		TestEqual(TEXT("Grenades survive restore"), Loaded->CombatState.Grenades, 1);
		TestEqual(TEXT("Manticore state survives restore"), Loaded->CombatState.ChapterState.Vehicle.Health, 287.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHChapterCountdownNarrativeTest, "AshesOfHeaven.Chapter.CountdownAndNarrativeState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHChapterCountdownNarrativeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UAHChapterSubsystem* Chapter = NewObject<UAHChapterSubsystem>(TestGameInstance);
	Chapter->StartCountdown(522.0f);
	Chapter->TickCountdown(22.0f);
	TestEqual(TEXT("Countdown advances by elapsed time"), Chapter->GetCountdownSeconds(), 500.0f);
	TestTrue(TEXT("Countdown stays active before expiry"), Chapter->IsCountdownActive());
	Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_VeilRevelation")));
	Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_VeilRevelation")));
	TestEqual(TEXT("Narrative events are one-shot"), Chapter->GetState().CompletedNarrativeEvents.Num(), 1);
	FAHChapterState State = Chapter->GetState();
	UAHChapterSubsystem* Restored = NewObject<UAHChapterSubsystem>(TestGameInstance);
	Restored->RestoreState(State);
	TestTrue(TEXT("Countdown and narrative state restore"), FMath::IsNearlyEqual(Restored->GetCountdownSeconds(), 500.0f) && Restored->HasCompletedNarrativeEvent(FName(TEXT("Ch01_VeilRevelation"))));
	return true;
}

#endif
