#include "Tests/AHCombatTestCommandlet.h"

#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Engine/GameInstance.h"
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
	FString Filter = TEXT("AshesOfHeaven");
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
		const TArray<FName> ObjectiveChain = {
			FName(TEXT("ReachDefensivePosition")),
			FName(TEXT("EliminateVeilAssault")),
			FName(TEXT("AdvanceThroughBreach")),
			FName(TEXT("DefendEvacuationGate")),
			FName(TEXT("ReachExtraction"))
		};
		Expect(TestName, TEXT("five objectives are configured"), Objectives->GetObjectiveCount() == ObjectiveChain.Num());
		for (int32 Index = 0; Index < ObjectiveChain.Num(); ++Index)
		{
			Expect(TestName, TEXT("objective chain stays ordered and active"), Objectives->IsCurrentObjective(ObjectiveChain[Index]));
			Expect(TestName, TEXT("objective completes through the real subsystem"), Objectives->CompleteObjective(ObjectiveChain[Index]));
		}
		Expect(TestName, TEXT("mission completion is reachable"), Objectives->IsMissionComplete());
		Expect(TestName, TEXT("all five objectives are recorded"), Objectives->GetCompletedObjectiveIds().Num() == ObjectiveChain.Num());
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.ObjectiveRestoreState")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.ObjectiveRestoreState");
		const int32 FailureCountBefore = FailureCount;
		UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
		Objectives->RestoreState(2);
		Expect(TestName, TEXT("partial checkpoint restores current objective"), Objectives->IsCurrentObjective(FName(TEXT("AdvanceThroughBreach"))));
		Expect(TestName, TEXT("partial checkpoint restores objective history"), Objectives->GetCompletedObjectiveIds().Num() == 2);
		Expect(TestName, TEXT("partial checkpoint is not complete"), !Objectives->IsMissionComplete());
		Objectives->RestoreState(5);
		Expect(TestName, TEXT("completed checkpoint restores mission completion"), Objectives->IsMissionComplete());
		Expect(TestName, TEXT("completed checkpoint restores all objective history"), Objectives->GetCompletedObjectiveIds().Num() == 5);
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
		Save->CombatState.Grenades = 3;
		Save->CombatState.ObjectiveIndex = 2;
		Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_One")));
		Save->CombatState.CompletedEncounters.Add(FName(TEXT("Encounter_Two")));
		TArray<uint8> Bytes;
		Expect(TestName, TEXT("checkpoint save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
		UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		Expect(TestName, TEXT("checkpoint save deserializes"), Loaded != nullptr);
		if (Loaded)
		{
			Expect(TestName, TEXT("checkpoint id survives serialization"), Loaded->CombatState.CheckpointId == FName(TEXT("Checkpoint_2")));
			Expect(TestName, TEXT("health survives serialization"), FMath::IsNearlyEqual(Loaded->CombatState.Health, 73.0f));
			Expect(TestName, TEXT("ammo survives serialization"), Loaded->CombatState.Ammo.Magazine == 19);
			Expect(TestName, TEXT("grenade state survives serialization"), Loaded->CombatState.Grenades == 3);
			Expect(TestName, TEXT("objective state survives serialization"), Loaded->CombatState.ObjectiveIndex == 2);
			Expect(TestName, TEXT("encounter one progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_One"))));
			Expect(TestName, TEXT("encounter two progression survives serialization"), Loaded->CombatState.CompletedEncounters.Contains(FName(TEXT("Encounter_Two"))));
		}
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Combat.EncounterConfiguration")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Combat.EncounterConfiguration");
		const int32 FailureCountBefore = FailureCount;
		AAHCombatEncounter* Encounter = NewObject<AAHCombatEncounter>();
		Expect(TestName, TEXT("encounter enemies default to Veil Pilgrims"), Encounter->EnemyClass == AAHVeilPilgrimCharacter::StaticClass());
		Expect(TestName, TEXT("new encounter starts incomplete and inactive"), !Encounter->IsActive() && !Encounter->IsComplete());
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

	if (BeginTest(TEXT("AshesOfHeaven.Chapter.StageOrdering")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Chapter.StageOrdering");
		const int32 FailureCountBefore = FailureCount;
		const TArray<EAHChapterStage> Stages = {
			EAHChapterStage::OpeningBlack,
			EAHChapterStage::ErebusOpening,
			EAHChapterStage::OpeningBattle,
			EAHChapterStage::TransitStation,
			EAHChapterStage::VeilRevelation,
			EAHChapterStage::OpenBattlefield,
			EAHChapterStage::ManticoreSection,
			EAHChapterStage::CathedralApproach,
			EAHChapterStage::FailsafeOrder,
			EAHChapterStage::CathedralInterior,
			EAHChapterStage::SaelTransmission,
			EAHChapterStage::FailsafeTerminal,
			EAHChapterStage::Escape,
			EAHChapterStage::OtherLucian,
			EAHChapterStage::ErebusDestruction,
			EAHChapterStage::TenYearsLater,
			EAHChapterStage::MayaScene,
			EAHChapterStage::NysaTransmission,
			EAHChapterStage::FleetDeparture,
			EAHChapterStage::StarsDisappearing,
			EAHChapterStage::ChapterComplete
		};
		Expect(TestName, TEXT("Chapter stage list contains every required stage"), Stages.Num() == 21);
		for (int32 Index = 1; Index < Stages.Num(); ++Index)
		{
			Expect(TestName, TEXT("Chapter stages remain in authored order"), static_cast<uint8>(Stages[Index]) > static_cast<uint8>(Stages[Index - 1]));
		}
		Expect(TestName, TEXT("Chapter completion is the terminal stage"), Stages.Last() == EAHChapterStage::ChapterComplete);
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Chapter.ObjectiveChain")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Chapter.ObjectiveChain");
		const int32 FailureCountBefore = FailureCount;
		UAHObjectiveSubsystem* Objectives = NewObject<UAHObjectiveSubsystem>();
		TArray<FAHObjectiveDefinition> Definitions;
		for (int32 Index = 0; Index < 17; ++Index)
		{
			Definitions.Add({FName(*FString::Printf(TEXT("Ch01_Objective_%02d"), Index + 1)), FText::FromString(FString::Printf(TEXT("CHAPTER OBJECTIVE %02d"), Index + 1)), FText::FromString(TEXT("Greybox verification objective."))});
		}
		Objectives->ConfigureObjectives(Definitions, 0);
		Expect(TestName, TEXT("Chapter objective chain contains all seventeen objectives"), Objectives->GetObjectiveCount() == 17);
		for (const FAHObjectiveDefinition& Definition : Definitions)
		{
			Expect(TestName, TEXT("each Chapter objective completes in order"), Objectives->CompleteObjective(Definition.Id));
		}
		Expect(TestName, TEXT("Chapter completion state is reachable"), Objectives->IsMissionComplete());
		Expect(TestName, TEXT("completed Chapter objective history is complete"), Objectives->GetCompletedObjectiveIds().Num() == 17);
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Chapter.StateSerialization")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Chapter.StateSerialization");
		const int32 FailureCountBefore = FailureCount;
		UAHSaveGame* Save = NewObject<UAHSaveGame>();
		Save->CombatState.bValid = true;
		Save->CombatState.MapName = TEXT("L_ChapterOne_Greybox");
		Save->CombatState.CheckpointId = FName(TEXT("Ch01_Checkpoint_03"));
		Save->CombatState.ObjectiveIndex = 7;
		Save->CombatState.Ammo.Magazine = 14;
		Save->CombatState.Ammo.Reserve = 121;
		Save->CombatState.Grenades = 1;
		Save->CombatState.ChapterState.Stage = EAHChapterStage::FailsafeOrder;
		Save->CombatState.ChapterState.ObjectiveIndex = 7;
		Save->CombatState.ChapterState.CheckpointId = FName(TEXT("Ch01_Checkpoint_03"));
		Save->CombatState.ChapterState.CountdownSeconds = 401.5f;
		Save->CombatState.ChapterState.bCountdownActive = true;
		Save->CombatState.ChapterState.bFailsafeConfirmed = false;
		Save->CombatState.ChapterState.CompletedEncounters.Add(FName(TEXT("Ch01_OpeningBattle")));
		Save->CombatState.ChapterState.Vehicle.bSpawned = true;
		Save->CombatState.ChapterState.Vehicle.Health = 287.0f;
		TArray<uint8> Bytes;
		Expect(TestName, TEXT("Chapter checkpoint serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
		UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
		Expect(TestName, TEXT("Chapter checkpoint deserializes"), Loaded != nullptr);
		if (Loaded)
		{
			Expect(TestName, TEXT("Chapter stage survives restore"), Loaded->CombatState.ChapterState.Stage == EAHChapterStage::FailsafeOrder);
			Expect(TestName, TEXT("Chapter countdown survives restore"), FMath::IsNearlyEqual(Loaded->CombatState.ChapterState.CountdownSeconds, 401.5f));
			Expect(TestName, TEXT("ammo survives Chapter restore"), Loaded->CombatState.Ammo.Magazine == 14 && Loaded->CombatState.Ammo.Reserve == 121);
			Expect(TestName, TEXT("grenades survive Chapter restore"), Loaded->CombatState.Grenades == 1);
			Expect(TestName, TEXT("vehicle state survives Chapter restore"), FMath::IsNearlyEqual(Loaded->CombatState.ChapterState.Vehicle.Health, 287.0f));
		}
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	if (BeginTest(TEXT("AshesOfHeaven.Chapter.CountdownAndNarrativeState")))
	{
		const FString TestName = TEXT("AshesOfHeaven.Chapter.CountdownAndNarrativeState");
		const int32 FailureCountBefore = FailureCount;
		UGameInstance* TestGameInstance = NewObject<UGameInstance>(GetTransientPackage());
		UAHChapterSubsystem* Chapter = NewObject<UAHChapterSubsystem>(TestGameInstance);
		Chapter->StartCountdown(522.0f);
		Chapter->TickCountdown(22.0f);
		Expect(TestName, TEXT("failsafe countdown advances deterministically"), FMath::IsNearlyEqual(Chapter->GetCountdownSeconds(), 500.0f));
		Expect(TestName, TEXT("countdown remains active before expiry"), Chapter->IsCountdownActive());
		Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_VeilRevelation")));
		Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_VeilRevelation")));
		Expect(TestName, TEXT("one-shot narrative events are idempotent"), Chapter->GetState().CompletedNarrativeEvents.Num() == 1);
		FAHChapterState SavedState = Chapter->GetState();
		UAHChapterSubsystem* Restored = NewObject<UAHChapterSubsystem>(TestGameInstance);
		Restored->RestoreState(SavedState);
		Expect(TestName, TEXT("countdown and narrative state restore together"), FMath::IsNearlyEqual(Restored->GetCountdownSeconds(), 500.0f) && Restored->HasCompletedNarrativeEvent(FName(TEXT("Ch01_VeilRevelation"))));
		FinishTest(TestName, FailureCountBefore, FailureCount);
		++RunCount;
	}

	UE_LOG(LogTemp, Display, TEXT("AshesOfHeaven combat commandlet: %d tests, %d failed checks, %s"), RunCount, FailureCount, bAllSuccessful ? TEXT("PASS") : TEXT("FAIL"));
	return RunCount > 0 && bAllSuccessful ? 0 : 1;
}
