#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformSaveSubsystem.h"
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
	TestEqual(TEXT("Five objectives are configured"), Objectives->GetObjectiveCount(), 5);
	TestTrue(TEXT("First objective is active"), Objectives->IsCurrentObjective(FName(TEXT("ReachDefensivePosition"))));
	TestTrue(TEXT("First objective completes through the real subsystem"), Objectives->CompleteObjective(FName(TEXT("ReachDefensivePosition"))));
	TestTrue(TEXT("Second objective becomes active"), Objectives->IsCurrentObjective(FName(TEXT("EliminateVeilAssault"))));
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
	Save->CombatState.Grenades = 1;
	Save->CombatState.ObjectiveIndex = 2;
	Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_One")));

	TArray<uint8> Bytes;
	TestTrue(TEXT("Checkpoint save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("Checkpoint save deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("Checkpoint id survives serialization"), Loaded->CombatState.CheckpointId, FName(TEXT("Checkpoint_2")));
		TestEqual(TEXT("Health survives serialization"), Loaded->CombatState.Health, 73.0f);
		TestEqual(TEXT("Ammo survives serialization"), Loaded->CombatState.Ammo.Magazine, 19);
		TestTrue(TEXT("Encounter progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_One"))));
	}
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

#endif
