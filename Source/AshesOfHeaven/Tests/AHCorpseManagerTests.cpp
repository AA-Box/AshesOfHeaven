#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SkeletalMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Combat/AHCorpseManagerSubsystem.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Misc/AutomationTest.h"
#include "Platform/AHPlatformManagerSubsystem.h"

namespace AHCorpseTests
{
	struct FWorldFixture
	{
		UWorld* World = nullptr;
		UAHCorpseManagerSubsystem* Manager = nullptr;

		bool Setup(const TCHAR* Name)
		{
			const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
				.InitializeScenes(true)
				.AllowAudioPlayback(false)
				.RequiresHitProxies(false)
				.CreatePhysicsScene(true)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.EnableTraceCollision(true)
				.SetTransactional(false)
				.CreateFXSystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(Name), nullptr, true, ERHIFeatureLevel::Num, &Initialization, false);
			if (!World)
			{
				return false;
			}
			Manager = World->GetSubsystem<UAHCorpseManagerSubsystem>();
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			World->BeginPlay();
			return Manager != nullptr;
		}

		AAHVeilPilgrimCharacter* SpawnCorpseCandidate(const FVector& Location = FVector::ZeroVector, bool bUseRealRagdoll = false) const
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AAHVeilPilgrimCharacter* Actor = World->SpawnActor<AAHVeilPilgrimCharacter>(Location, FRotator::ZeroRotator, SpawnParameters);
			if (Actor && bUseRealRagdoll)
			{
				Actor->GetMesh()->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")));
			}
			return Actor;
		}

		static void Kill(AAHVeilPilgrimCharacter* Actor)
		{
			if (Actor && Actor->GetHealthComponent())
			{
				Actor->GetHealthComponent()->ResetHealth();
				Actor->TakeDamage(100000.0f, FDamageEvent(), nullptr, nullptr);
			}
		}

		void Teardown()
		{
			if (World)
			{
				World->DestroyWorld(false);
			}
			World = nullptr;
			Manager = nullptr;
		}
	};

	FAHCorpseBudget MakeTestBudget(int32 SoftLimit, int32 HardLimit)
	{
		FAHCorpseBudget Budget;
		Budget.SoftLimit = SoftLimit;
		Budget.HardLimit = HardLimit;
		Budget.DeathReactionSeconds = 0.0f;
		Budget.SettleDelaySeconds = 0.0f;
		Budget.MaximumRagdollSeconds = 0.0f;
		Budget.ReducedCostDelaySeconds = 0.0f;
		Budget.RecentlyRenderedGraceSeconds = 0.0f;
		Budget.MaximumCleanupPerPass = 32;
		return Budget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseMinimumLifetimeSoftCapTest, "AshesOfHeaven.Corpse.MinimumLifetimeAndSoftCap", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseMinimumLifetimeSoftCapTest::RunTest(const FString& Parameters)
{
	AHCorpseTests::FWorldFixture Fixture;
	TestTrue(TEXT("corpse test world and manager are created"), Fixture.Setup(TEXT("AHCorpseMinimumLifetimeWorld")));
	if (!Fixture.Manager)
	{
		Fixture.Teardown();
		return false;
	}
	FAHCorpseBudget Budget = AHCorpseTests::MakeTestBudget(2, 4);
	Budget.MinimumLifetimeSeconds = 10.0f;
	Budget.EmergencyMinimumLifetimeSeconds = 2.0f;
	Fixture.Manager->SetBudgetForTesting(Budget);

	for (int32 Index = 0; Index < 3; ++Index)
	{
		AHCorpseTests::FWorldFixture::Kill(Fixture.SpawnCorpseCandidate(FVector(Index * 200.0f, 0.0f, 0.0f)));
	}
	Fixture.Manager->ProcessLifecycleForTesting(5.0f);
	TestEqual(TEXT("soft pressure never bypasses minimum lifetime"), Fixture.Manager->GetPerformanceStats().OrdinaryCorpses, 3);
	Fixture.Manager->ProcessLifecycleForTesting(11.0f);
	TestEqual(TEXT("soft pressure cleans back to the soft profile limit"), Fixture.Manager->GetPerformanceStats().OrdinaryCorpses, 2);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseHardCapTest, "AshesOfHeaven.Corpse.HardCapEmergencyLifetime", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseHardCapTest::RunTest(const FString& Parameters)
{
	AHCorpseTests::FWorldFixture Fixture;
	TestTrue(TEXT("corpse test world and manager are created"), Fixture.Setup(TEXT("AHCorpseHardCapWorld")));
	if (!Fixture.Manager)
	{
		Fixture.Teardown();
		return false;
	}
	FAHCorpseBudget Budget = AHCorpseTests::MakeTestBudget(2, 3);
	Budget.MinimumLifetimeSeconds = 100.0f;
	Budget.EmergencyMinimumLifetimeSeconds = 2.0f;
	Fixture.Manager->SetBudgetForTesting(Budget);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		AHCorpseTests::FWorldFixture::Kill(Fixture.SpawnCorpseCandidate(FVector(Index * 200.0f, 0.0f, 0.0f)));
	}
	Fixture.Manager->ProcessLifecycleForTesting(1.0f);
	TestEqual(TEXT("hard cap retains the emergency death-reaction grace"), Fixture.Manager->GetPerformanceStats().OrdinaryCorpses, 4);
	Fixture.Manager->ProcessLifecycleForTesting(3.0f);
	TestEqual(TEXT("hard cap can bypass normal lifetime after emergency grace"), Fixture.Manager->GetPerformanceStats().OrdinaryCorpses, 3);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseVisiblePreservationTest, "AshesOfHeaven.Corpse.VisibleAndTargetedPreservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseVisiblePreservationTest::RunTest(const FString& Parameters)
{
	FAHCorpseBudget Budget;
	FAHCorpseCleanupEvaluation Evaluation;
	Evaluation.AgeSeconds = 1000.0f;
	Evaluation.bCurrentlyVisible = true;
	TestFalse(TEXT("on-camera corpses survive soft pressure"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, false));
	TestFalse(TEXT("on-camera corpses survive hard pressure"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));

	Evaluation.bCurrentlyVisible = false;
	Evaluation.bRecentlyRendered = true;
	TestFalse(TEXT("recently rendered corpses survive soft pressure"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, false));
	TestTrue(TEXT("hard pressure may reclaim a recently rendered body only after it leaves view"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));

	Evaluation.bRecentlyRendered = false;
	Evaluation.bTargetedInteractable = true;
	TestFalse(TEXT("the current interaction target survives cleanup"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseNarrativeAndLootPreservationTest, "AshesOfHeaven.Corpse.NarrativeAndLootablePreservation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseNarrativeAndLootPreservationTest::RunTest(const FString& Parameters)
{
	FAHCorpseBudget Budget;
	FAHCorpseCleanupEvaluation Evaluation;
	Evaluation.AgeSeconds = 1000.0f;
	Evaluation.bNarrative = true;
	TestFalse(TEXT("narrative bodies are never cleanup candidates"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));
	Evaluation.bNarrative = false;
	Evaluation.bPersistent = true;
	TestFalse(TEXT("persistent bodies are never cleanup candidates"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));
	Evaluation.bPersistent = false;
	Evaluation.bObjectiveCritical = true;
	TestFalse(TEXT("objective-critical bodies are never cleanup candidates"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));
	Evaluation.bObjectiveCritical = false;
	Evaluation.bScriptedCivilian = true;
	TestFalse(TEXT("scripted civilian bodies are never cleanup candidates"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));
	Evaluation.bScriptedCivilian = false;
	Evaluation.bHasImportantLoot = true;
	TestFalse(TEXT("unlooted important weapons preserve their body"), UAHCorpseManagerSubsystem::CanCleanupCandidate(Evaluation, Budget, true));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseCleanupOrderTest, "AshesOfHeaven.Corpse.CleanupOrder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseCleanupOrderTest::RunTest(const FString& Parameters)
{
	FAHCorpseBudget Budget;
	Budget.MinimumLifetimeSeconds = 10.0f;
	TArray<FAHCorpseCleanupEvaluation> Evaluations;
	FAHCorpseCleanupEvaluation& NearYoung = Evaluations.Emplace_GetRef();
	NearYoung.AgeSeconds = 20.0f;
	NearYoung.DistanceToClosestPlayer = 500.0f;
	NearYoung.QueueSequence = 1;
	FAHCorpseCleanupEvaluation& FarOld = Evaluations.Emplace_GetRef();
	FarOld.AgeSeconds = 45.0f;
	FarOld.DistanceToClosestPlayer = 7000.0f;
	FarOld.QueueSequence = 2;
	FAHCorpseCleanupEvaluation& ImportantFarOld = Evaluations.Emplace_GetRef();
	ImportantFarOld.AgeSeconds = 45.0f;
	ImportantFarOld.DistanceToClosestPlayer = 7000.0f;
	ImportantFarOld.Importance = 1.0f;
	ImportantFarOld.QueueSequence = 3;

	TestEqual(TEXT("cleanup selects the old distant ordinary corpse before a nearby or important one"),
		UAHCorpseManagerSubsystem::SelectCleanupCandidate(Evaluations, Budget, false), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpsePlatformBudgetTest, "AshesOfHeaven.Corpse.PlatformBudgets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpsePlatformBudgetTest::RunTest(const FString& Parameters)
{
	const FAHCorpseBudget Desktop = UAHPlatformManagerSubsystem::SelectCorpseBudget(false, true);
	const FAHCorpseBudget HighMobile = UAHPlatformManagerSubsystem::SelectCorpseBudget(true, true);
	const FAHCorpseBudget BaselineMobile = UAHPlatformManagerSubsystem::SelectCorpseBudget(true, false);
	TestTrue(TEXT("desktop ordinary corpse target is in the 20-30 range"), Desktop.SoftLimit >= 20 && Desktop.HardLimit <= 30);
	TestTrue(TEXT("high-end mobile ordinary corpse target is in the 8-12 range"), HighMobile.SoftLimit >= 8 && HighMobile.HardLimit <= 12);
	TestTrue(TEXT("baseline mobile ordinary corpse target is in the 5-8 range"), BaselineMobile.SoftLimit >= 5 && BaselineMobile.HardLimit <= 8);
	TestTrue(TEXT("each profile has a real soft-to-hard pressure band"), Desktop.SoftLimit < Desktop.HardLimit && HighMobile.SoftLimit < HighMobile.HardLimit && BaselineMobile.SoftLimit < BaselineMobile.HardLimit);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseCheckpointResetTest, "AshesOfHeaven.Corpse.CheckpointReset", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseCheckpointResetTest::RunTest(const FString& Parameters)
{
	AHCorpseTests::FWorldFixture Fixture;
	TestTrue(TEXT("corpse test world and manager are created"), Fixture.Setup(TEXT("AHCorpseCheckpointResetWorld")));
	if (!Fixture.Manager)
	{
		Fixture.Teardown();
		return false;
	}
	AAHVeilPilgrimCharacter* Ordinary = Fixture.SpawnCorpseCandidate(FVector::ZeroVector);
	AAHVeilPilgrimCharacter* Narrative = Fixture.SpawnCorpseCandidate(FVector(200.0f, 0.0f, 0.0f));
	Narrative->Tags.Add(FAHCorpseTags::Narrative);
	AHCorpseTests::FWorldFixture::Kill(Ordinary);
	AHCorpseTests::FWorldFixture::Kill(Narrative);
	Fixture.Manager->ResetOrdinaryCorpsesForCheckpoint();
	TestTrue(TEXT("ordinary combat corpses are removed on checkpoint reset"), Ordinary->IsActorBeingDestroyed());
	TestFalse(TEXT("narrative tagged corpses survive checkpoint reset"), Narrative->IsActorBeingDestroyed());
	TestEqual(TEXT("only the narrative body remains managed"), Fixture.Manager->GetPerformanceStats().ManagedCorpses, 1);

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseImportantLootTagTest, "AshesOfHeaven.Corpse.ImportantLootTag", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseImportantLootTagTest::RunTest(const FString& Parameters)
{
	AHCorpseTests::FWorldFixture Fixture;
	TestTrue(TEXT("corpse test world and manager are created"), Fixture.Setup(TEXT("AHCorpseLootTagWorld")));
	if (!Fixture.Manager)
	{
		Fixture.Teardown();
		return false;
	}
	FAHCorpseBudget Budget = AHCorpseTests::MakeTestBudget(0, 2);
	Budget.MinimumLifetimeSeconds = 0.0f;
	Budget.EmergencyMinimumLifetimeSeconds = 0.0f;
	Fixture.Manager->SetBudgetForTesting(Budget);

	AAHVeilPilgrimCharacter* Corpse = Fixture.SpawnCorpseCandidate();
	AAHWeaponBase* Weapon = Corpse->GetInventoryComponent()->AddWeaponClass(AAHWeaponBase::StaticClass());
	TestNotNull(TEXT("important loot test body has a weapon"), Weapon);
	if (Weapon)
	{
		Weapon->bImportantCorpseLoot = true;
	}
	Corpse->Tags.Add(FAHCorpseTags::Lootable);
	AHCorpseTests::FWorldFixture::Kill(Corpse);
	Fixture.Manager->ProcessLifecycleForTesting(1.0f);
	TestFalse(TEXT("lootable tag and important weapon preserve the body"), Corpse->IsActorBeingDestroyed());
	if (Weapon)
	{
		Weapon->bImportantCorpseLoot = false;
	}
	Corpse->Tags.Remove(FAHCorpseTags::Lootable);
	Fixture.Manager->ProcessLifecycleForTesting(2.0f);
	TestTrue(TEXT("body becomes removable after important loot protection clears"), Corpse->IsActorBeingDestroyed());

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseSettlingPerformanceTest, "AshesOfHeaven.Corpse.SettlingReducesCost", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseSettlingPerformanceTest::RunTest(const FString& Parameters)
{
	AHCorpseTests::FWorldFixture Fixture;
	TestTrue(TEXT("corpse test world and manager are created"), Fixture.Setup(TEXT("AHCorpsePerformanceWorld")));
	if (!Fixture.Manager)
	{
		Fixture.Teardown();
		return false;
	}
	FAHCorpseBudget Budget = AHCorpseTests::MakeTestBudget(10, 12);
	Budget.MinimumLifetimeSeconds = 100.0f;
	Fixture.Manager->SetBudgetForTesting(Budget);
	AAHVeilPilgrimCharacter* Corpse = Fixture.SpawnCorpseCandidate(FVector::ZeroVector, true);
	TestNotNull(TEXT("performance test corpse has a physics asset"), Corpse ? Corpse->GetMesh()->GetPhysicsAsset() : nullptr);
	AHCorpseTests::FWorldFixture::Kill(Corpse);
	const FAHCorpsePerformanceStats ActiveStats = Fixture.Manager->GetPerformanceStats();
	Fixture.Manager->ProcessLifecycleForTesting(1.0f);
	const FAHCorpsePerformanceStats SettledStats = Fixture.Manager->GetPerformanceStats();

	TestTrue(TEXT("death immediately disables actor and weapon ticks"), ActiveStats.TickingActors == 0 && ActiveStats.TickingWeapons == 0);
	TestTrue(TEXT("active ragdoll begins with a ticking skeletal component"), ActiveStats.TickingSkeletalMeshes >= 1);
	TestTrue(TEXT("settling reaches reduced-cost lifecycle state"), SettledStats.ReducedCostCorpses == 1);
	TestEqual(TEXT("reduced-cost corpses stop skeletal mesh ticks"), SettledStats.TickingSkeletalMeshes, 0);
	TestEqual(TEXT("reduced-cost corpses leave the rigid-body solver"), SettledStats.SimulatingSkeletalMeshes, 0);
	TestEqual(TEXT("reduced-cost corpses have no awake physics bodies"), SettledStats.AwakePhysicsBodies, 0);
	AddInfo(FString::Printf(TEXT("Corpse performance active->settled: mesh_ticks %d->%d, simulating_meshes %d->%d, awake_bodies %d->%d, exclusive_bytes %lld->%lld"),
		ActiveStats.TickingSkeletalMeshes, SettledStats.TickingSkeletalMeshes,
		ActiveStats.SimulatingSkeletalMeshes, SettledStats.SimulatingSkeletalMeshes,
		ActiveStats.AwakePhysicsBodies, SettledStats.AwakePhysicsBodies,
		ActiveStats.EstimatedExclusiveMemoryBytes, SettledStats.EstimatedExclusiveMemoryBytes));

	Fixture.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHCorpseRepeatedCombatCyclesTest, "AshesOfHeaven.Corpse.RepeatedCombatCycles", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHCorpseRepeatedCombatCyclesTest::RunTest(const FString& Parameters)
{
	AHCorpseTests::FWorldFixture Fixture;
	TestTrue(TEXT("corpse test world and manager are created"), Fixture.Setup(TEXT("AHCorpseRepeatedCyclesWorld")));
	if (!Fixture.Manager)
	{
		Fixture.Teardown();
		return false;
	}
	FAHCorpseBudget Budget = AHCorpseTests::MakeTestBudget(2, 3);
	Budget.MinimumLifetimeSeconds = 0.0f;
	Budget.EmergencyMinimumLifetimeSeconds = 0.0f;
	Fixture.Manager->SetBudgetForTesting(Budget);

	for (int32 Cycle = 0; Cycle < 12; ++Cycle)
	{
		AHCorpseTests::FWorldFixture::Kill(Fixture.SpawnCorpseCandidate(FVector(Cycle * 100.0f, 0.0f, 0.0f)));
		Fixture.Manager->ProcessLifecycleForTesting(static_cast<float>(Cycle + 1));
		TestTrue(TEXT("every repeated combat cycle stays at or below the hard cap"), Fixture.Manager->GetPerformanceStats().OrdinaryCorpses <= 3);
	}
	const FAHCorpsePerformanceStats Stats = Fixture.Manager->GetPerformanceStats();
	TestTrue(TEXT("repeated cycles settle back to the soft limit"), Stats.OrdinaryCorpses <= 2);
	TestTrue(TEXT("repeated cycles actually reclaim old bodies"), Stats.TotalRemovedOrRecycled >= 10);

	Fixture.Teardown();
	return true;
}

#endif
