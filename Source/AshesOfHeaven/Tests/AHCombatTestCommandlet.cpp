#include "Tests/AHCombatTestCommandlet.h"

#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"

UAHCombatVerificationCommandlet::UAHCombatVerificationCommandlet()
{
	IsEditor = true;
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
	ShowProgress = false;
}

int32 UAHCombatVerificationCommandlet::Main(const FString& Params)
{
	FString Filter = TEXT("AshesOfHeaven.Combat");
	FParse::Value(*Params, TEXT("Filter="), Filter);

	int32 FailureCount = 0;
	bool bAllSuccessful = true;
	int32 RunCount = 0;
	auto Expect = [&FailureCount](const FString& TestName, const TCHAR* CheckName, bool bCondition)
	{
		if (!bCondition)
		{
			++FailureCount;
			UE_LOG(LogTemp, Error, TEXT("%s: FAIL - %s"), *TestName, CheckName);
		}
	};
	auto BeginTest = [&RunCount, &Filter](const TCHAR* TestName)
	{
		const FString Name(TestName);
		return Name.StartsWith(Filter);
	};
	auto FinishTest = [&bAllSuccessful](const FString& TestName, int32 FailureCountBefore, int32 CurrentFailureCount)
	{
		const bool bPassed = CurrentFailureCount == FailureCountBefore;
		bAllSuccessful &= bPassed;
		UE_LOG(LogTemp, Display, TEXT("%s: %s"), *TestName, bPassed ? TEXT("PASS") : TEXT("FAIL"));
	};

	if (BeginTest(TEXT("AshesOfHeaven.Combat.HealthDamage")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.HealthDamage");
		const int32 FailureCountBefore = FailureCount;
		UAHHealthComponent* Health = NewObject<UAHHealthComponent>();
		Health->MaxHealth = 100.0f;
		Health->ResetHealth();
		Expect(TestName, TEXT("health starts full"), FMath::IsNearlyEqual(Health->GetHealth(), 100.0f));
		Expect(TestName, TEXT("damage is applied"), FMath::IsNearlyEqual(Health->ApplyDamage(35.0f), 35.0f));
		Expect(TestName, TEXT("health is reduced"), FMath::IsNearlyEqual(Health->GetHealth(), 65.0f));
		Health->ApplyDamage(100.0f);
		Expect(TestName, TEXT("lethal damage marks dead"), Health->IsDead());
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.ArmorAbsorptionAndRegenTiming")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.ArmorAbsorptionAndRegenTiming");
		const int32 FailureCountBefore = FailureCount;
		UAHArmorComponent* Armor = NewObject<UAHArmorComponent>();
		Armor->MaxArmor = 100.0f;
		Armor->RegenerationDelay = 4.0f;
		Armor->ResetArmor();
		Expect(TestName, TEXT("armor absorbs incoming damage"), FMath::IsNearlyEqual(Armor->AbsorbDamage(35.0f), 35.0f));
		Expect(TestName, TEXT("armor is reduced before health"), FMath::IsNearlyEqual(Armor->GetArmor(), 65.0f));
		Expect(TestName, TEXT("regeneration delay is enforced"), FMath::IsNearlyEqual(Armor->GetTimeUntilRegeneration(2.0f), 2.0f));
		Expect(TestName, TEXT("regeneration becomes available after delay"), FMath::IsNearlyZero(Armor->GetTimeUntilRegeneration(4.0f)));
		float ArmorDamage = 0.0f;
		Expect(TestName, TEXT("pure armor rule returns health damage"), FMath::IsNearlyEqual(UAHCombatRulesLibrary::ApplyArmorAbsorption(120.0f, 50.0f, ArmorDamage), 70.0f));
		Expect(TestName, TEXT("pure armor rule consumes remaining armor"), FMath::IsNearlyEqual(ArmorDamage, 50.0f));
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.AmmoConsumptionAndReloadCalculation")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.AmmoConsumptionAndReloadCalculation");
		const int32 FailureCountBefore = FailureCount;
		FAHAmmoState Ammo;
		Ammo.MagazineCapacity = 36;
		Ammo.Magazine = 11;
		Ammo.Reserve = 18;
		Expect(TestName, TEXT("reload transfers only missing rounds"), UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo) == 18);
		Ammo.Reserve = 100;
		Expect(TestName, TEXT("reload never exceeds magazine capacity"), UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo) == 25);
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.GrenadeInventory")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.GrenadeInventory");
		const int32 FailureCountBefore = FailureCount;
		UAHInventoryComponent* Inventory = NewObject<UAHInventoryComponent>();
		Inventory->MaximumGrenades = 4;
		Inventory->AddGrenades(2);
		Expect(TestName, TEXT("grenade pickup replenishes inventory"), Inventory->GetGrenades() == 2);
		Expect(TestName, TEXT("grenade can be consumed"), Inventory->ConsumeGrenade());
		Expect(TestName, TEXT("consumed grenade is removed"), Inventory->GetGrenades() == 1);
		Inventory->AddGrenades(10);
		Expect(TestName, TEXT("grenade inventory is capped"), Inventory->GetGrenades() == 4);
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.ObjectiveTransitions")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.ObjectiveTransitions");
		const int32 FailureCountBefore = FailureCount;
		UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
		Expect(TestName, TEXT("five objectives are configured"), Objectives->GetObjectiveCount() == 5);
		Expect(TestName, TEXT("first objective is active"), Objectives->IsCurrentObjective(FName(TEXT("ReachDefensivePosition"))));
		Expect(TestName, TEXT("first objective completes through the real subsystem"), Objectives->CompleteObjective(FName(TEXT("ReachDefensivePosition"))));
		Expect(TestName, TEXT("second objective becomes active"), Objectives->IsCurrentObjective(FName(TEXT("EliminateVeilAssault"))));
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.CheckpointSerialization")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.CheckpointSerialization");
		const int32 FailureCountBefore = FailureCount;
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
		Expect(TestName, TEXT("checkpoint save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
		UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		Expect(TestName, TEXT("checkpoint save deserializes"), Loaded != nullptr);
		if (Loaded)
		{
			Expect(TestName, TEXT("checkpoint id survives serialization"), Loaded->CombatState.CheckpointId == FName(TEXT("Checkpoint_2")));
			Expect(TestName, TEXT("health survives serialization"), FMath::IsNearlyEqual(Loaded->CombatState.Health, 73.0f));
			Expect(TestName, TEXT("ammo survives serialization"), Loaded->CombatState.Ammo.Magazine == 19);
			Expect(TestName, TEXT("encounter progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_One"))));
		}
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.FactionHostility")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.FactionHostility");
		const int32 FailureCountBefore = FailureCount;
		Expect(TestName, TEXT("player is hostile to veil"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Player, EAHFaction::Veil));
		Expect(TestName, TEXT("veil is hostile to player"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Veil, EAHFaction::Player));
		Expect(TestName, TEXT("human is hostile to veil"), UAHCombatRulesLibrary::IsHostile(EAHFaction::Human, EAHFaction::Veil));
		Expect(TestName, TEXT("human allies are not hostile"), !UAHCombatRulesLibrary::IsHostile(EAHFaction::Human, EAHFaction::Player));
		Expect(TestName, TEXT("neutral is not hostile"), !UAHCombatRulesLibrary::IsHostile(EAHFaction::Neutral, EAHFaction::Veil));
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	UE_LOG(LogTemp, Display, TEXT("AshesOfHeaven combat commandlet: %d tests, %d failed checks, %s"), RunCount, FailureCount, bAllSuccessful ? TEXT("PASS") : TEXT("FAIL"));
	return RunCount > 0 && bAllSuccessful ? 0 : 1;
}
