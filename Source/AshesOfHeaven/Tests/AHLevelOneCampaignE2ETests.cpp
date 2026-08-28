#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Game/AHChapterOneGameMode.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/Level/AHChapterOneDirector.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Platform/AHPlatformSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

// End-to-end coverage for the Level One campaign lifecycle: play the twelve objectives on the
// real director, die and reload a checkpoint mid-run, finish the chapter, and prove the
// completion is on disk and still there after the world and game instance are destroyed and
// rebuilt. The names here are deliberately unique to this translation unit - adaptive unity
// builds hide same-named helpers in other test files until the tree is committed.
namespace AHLevelOneE2ESupport
{
	/**
	 * Points the save API at a throwaway slot for the life of the scope, so these tests can
	 * write and read real .sav files without touching the player's campaign save. The slot is
	 * deleted on both entry and exit, so a crashed run cannot leak state into the next one.
	 */
	struct FScopedCampaignSaveSlot
	{
		FString PreviousSlot;

		explicit FScopedCampaignSaveSlot(const TCHAR* SlotName)
		{
			UAHPlatformSettings* Settings = GetMutableDefault<UAHPlatformSettings>();
			PreviousSlot = Settings->DefaultSaveSlot;
			Settings->DefaultSaveSlot = SlotName;
			UGameplayStatics::DeleteGameInSlot(SlotName, 0);
		}

		~FScopedCampaignSaveSlot()
		{
			UAHPlatformSettings* Settings = GetMutableDefault<UAHPlatformSettings>();
			UGameplayStatics::DeleteGameInSlot(Settings->DefaultSaveSlot, 0);
			Settings->DefaultSaveSlot = PreviousSlot;
		}
	};

	/**
	 * One booted Level One session: game instance, world, a possessed player, and either the
	 * director alone or the real AAHChapterOneGameMode boot path that restores saved state and
	 * spawns the director itself. Tearing this down and booting another instance is the
	 * in-process equivalent of quitting and relaunching the game.
	 */
	struct FLevelOneSession
	{
		UGameInstance* GameInstance = nullptr;
		UWorld* World = nullptr;
		AAHChapterOneDirector* Director = nullptr;
		AAHChapterOneGameMode* GameMode = nullptr;
		AAHCombatPlayerController* Controller = nullptr;
		AAHCombatPlayerCharacter* Player = nullptr;

		bool Boot(bool bViaGameMode)
		{
			GameInstance = NewObject<UGameInstance>(GEngine);
			// The world name has to read as a Chapter One map: UAHCheckpointSubsystem stamps
			// GetWorld()->GetName() into the checkpoint and RestoreFromState refuses a
			// checkpoint whose map is not ChapterOne. A default "Untitled" fixture world
			// silently makes every restore a no-op.
			GameInstance->InitializeStandalone(FName(TEXT("L_ChapterOne_E2E")));
			World = GameInstance->GetWorld();
			if (!World)
			{
				return false;
			}
			World->InitializeActorsForPlay(FURL());

			// Spawn before begun-play and dispatch BeginPlay afterwards, the way the engine
			// does: UStaticMeshComponent::SetStaticMesh rejects static-mobility components
			// once the world has begun play, so the packaged assembly order has to be
			// reproduced exactly or the greybox collision never exists.
			if (bViaGameMode)
			{
				GameMode = World->SpawnActor<AAHChapterOneGameMode>(AAHChapterOneGameMode::StaticClass());
			}
			else
			{
				Director = World->SpawnActor<AAHChapterOneDirector>(AAHChapterOneDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
			}
			if (AWorldSettings* Settings = World->GetWorldSettings())
			{
				Settings->NotifyBeginPlay();
			}
			World->BeginPlay();
			if (bViaGameMode && GameMode)
			{
				// The real boot restore: reads the save, decides the chapter state, and spawns
				// the director. This is the code path a relaunch runs.
				GameMode->DispatchBeginPlay();
				TActorIterator<AAHChapterOneDirector> DirectorIt(World);
				Director = DirectorIt ? *DirectorIt : nullptr;
			}

			const FAHStageSpatialDefinition& Opening = AHChapterSpatial::GetStageDefinition(EAHChapterStage::OpeningBlack);
			Controller = World->SpawnActor<AAHCombatPlayerController>();
			Player = World->SpawnActor<AAHCombatPlayerCharacter>(AAHCombatPlayerCharacter::StaticClass(), Opening.SafePlayerLocation, Opening.SafePlayerRotation);
			if (Controller && Player)
			{
				Controller->Possess(Player);
			}
			return Director != nullptr && Controller != nullptr && Player != nullptr;
		}

		void Teardown()
		{
			if (World)
			{
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
			if (GameInstance)
			{
				// Deinitialize subsystems while their referenced objects are still alive;
				// leaving it to the GC purge asserts (UObjectArray.h Index >= 0).
				GameInstance->Shutdown();
				GameInstance = nullptr;
			}
			Director = nullptr;
			GameMode = nullptr;
			Controller = nullptr;
			Player = nullptr;
		}

		UAHChapterSubsystem* Chapter() const { return GameInstance ? GameInstance->GetSubsystem<UAHChapterSubsystem>() : nullptr; }
		UAHPlatformSaveSubsystem* Save() const { return GameInstance ? GameInstance->GetSubsystem<UAHPlatformSaveSubsystem>() : nullptr; }
		UAHObjectiveSubsystem* Objectives() const { return World ? World->GetSubsystem<UAHObjectiveSubsystem>() : nullptr; }
		UAHCheckpointSubsystem* Checkpoints() const { return World ? World->GetSubsystem<UAHCheckpointSubsystem>() : nullptr; }

		/**
		 * Advances the director without running the timer manager. Delayed validators and
		 * dialogue line timers are deliberately left un-ticked: a settled-state check queued
		 * for an earlier stage would fire against a later one and log an Error, which fails
		 * the whole automation test.
		 */
		void TickDirector(int32 Frames, float DeltaSeconds)
		{
			for (int32 Frame = 0; Frame < Frames && Director; ++Frame)
			{
				Director->Tick(DeltaSeconds);
			}
		}

		AAHManticoreVehicle* FindManticore() const
		{
			TActorIterator<AAHManticoreVehicle> It(World);
			return It ? *It : nullptr;
		}

		AAHChapterTerminal* FindTerminal() const
		{
			TActorIterator<AAHChapterTerminal> It(World);
			return It ? *It : nullptr;
		}

		/** Completes whatever objective is current, the way every in-world completer does. */
		bool CompleteCurrentObjective()
		{
			UAHObjectiveSubsystem* Objective = Objectives();
			return Objective && Objective->CompleteObjective(Objective->GetCurrentObjective().Id);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneCampaignPlaythroughTest, "AshesOfHeaven.LevelOne.CampaignE2E.PlaythroughPersistsCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneCampaignPlaythroughTest::RunTest(const FString& Parameters)
{
	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_Playthrough"));

	FLevelOneSession Session;
	if (!Session.Boot(/*bViaGameMode=*/false))
	{
		AddError(TEXT("Level One session failed to boot"));
		Session.Teardown();
		return false;
	}

	UAHChapterSubsystem* Chapter = Session.Chapter();
	UAHObjectiveSubsystem* Objectives = Session.Objectives();
	UAHCheckpointSubsystem* Checkpoints = Session.Checkpoints();
	UAHPlatformSaveSubsystem* Save = Session.Save();
	if (!Chapter || !Objectives || !Checkpoints || !Save)
	{
		AddError(TEXT("Level One subsystems are missing from the booted session"));
		Session.Teardown();
		return false;
	}

	TestEqual(TEXT("Level One configures exactly twelve objectives"), Objectives->GetObjectiveCount(), AHChapterStateConstants::ObjectiveCount);
	TestFalse(TEXT("a fresh session has not completed the chapter"), Save->IsChapterComplete(AHChapterIds::ChapterOne()));

	// Walk the twelve objectives in order on the real director. Each completion routes through
	// UAHObjectiveSubsystem::CompleteObjective -> AAHChapterOneDirector::HandleObjectiveCompleted
	// -> StartStage, which is what a trigger, an encounter or an interaction does in game.
	bool bManticoreBoarded = false;
	bool bManticoreExited = false;
	bool bTerminalConfirmed = false;
	TArray<EAHChapterStage> VisitedStages;

	for (int32 Objective = 0; Objective < AHChapterStateConstants::ObjectiveCount; ++Objective)
	{
		const int32 IndexBefore = Objectives->GetCurrentObjectiveIndex();
		TestEqual(*FString::Printf(TEXT("objective cursor is at %d before completing it"), Objective), IndexBefore, Objective);

		// The two interactive set pieces are driven through their own actors rather than by
		// completing the objective outright, so this run really does board the Manticore and
		// authorise the failsafe at the terminal.
		if (Chapter->GetStage() == EAHChapterStage::ManticoreSection)
		{
			if (AAHManticoreVehicle* Manticore = Session.FindManticore())
			{
				bManticoreBoarded = Manticore->EnterVehicle(Session.Player);
				Manticore->FireMountedWeapon();
				Manticore->ExitVehicle();
				bManticoreExited = Manticore->GetDriver() == nullptr;
			}
		}
		if (Chapter->GetStage() == EAHChapterStage::FailsafeTerminal)
		{
			if (AAHChapterTerminal* Terminal = Session.FindTerminal())
			{
				// First interaction inspects, second confirms.
				IAHInteractable::Execute_Interact(Terminal, Session.Player);
				TestTrue(TEXT("the terminal reports being inspected before it can be confirmed"), Terminal->IsInspected());
				IAHInteractable::Execute_Interact(Terminal, Session.Player);
				bTerminalConfirmed = Terminal->IsConfirmed();
			}
		}

		if (Objectives->GetCurrentObjectiveIndex() == IndexBefore)
		{
			TestTrue(*FString::Printf(TEXT("objective %d completes"), Objective), Session.CompleteCurrentObjective());
		}
		Session.TickDirector(2, 0.05f);
		VisitedStages.AddUnique(Chapter->GetStage());

		// Capture a checkpoint wherever the chapter authored one for the stage just reached.
		const FAHStageSpatialDefinition& Reached = AHChapterSpatial::GetStageDefinition(Chapter->GetStage());
		if (Reached.CheckpointId != NAME_None)
		{
			Checkpoints->CaptureCheckpoint(Reached.CheckpointId);
		}
	}

	TestTrue(TEXT("the Manticore was boarded during the run"), bManticoreBoarded);
	TestTrue(TEXT("the Manticore was exited during the run"), bManticoreExited);
	TestTrue(TEXT("the failsafe terminal was confirmed during the run"), bTerminalConfirmed);
	TestTrue(TEXT("the failsafe order was recorded"), Chapter->GetState().bFailsafeConfirmed);
	TestTrue(TEXT("the run passed through the Manticore section"), VisitedStages.Contains(EAHChapterStage::ManticoreSection));
	TestTrue(TEXT("the run passed through the Cathedral interior"), VisitedStages.Contains(EAHChapterStage::CathedralInterior));
	TestTrue(TEXT("the run passed through the escape"), VisitedStages.Contains(EAHChapterStage::Escape));
	TestTrue(TEXT("the run reached the destruction of Erebus"), VisitedStages.Contains(EAHChapterStage::ErebusDestruction));

	TestTrue(TEXT("the mission reports complete after twelve objectives"), Objectives->IsMissionComplete());
	TestEqual(TEXT("the chapter ends on ChapterComplete"), Chapter->GetStage(), EAHChapterStage::ChapterComplete);
	TestTrue(TEXT("live chapter state reports completion"), Chapter->IsChapterComplete());
	TestTrue(TEXT("the closing title beat is recorded"), Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_TitleReveal"))));

	// The point of the whole test: completion reached the disk, and did so without depending
	// on the spatial checkpoint that CaptureCheckpoint writes.
	TestTrue(TEXT("completing Level One writes campaign completion to disk"), Save->IsChapterComplete(AHChapterIds::ChapterOne()));

	Session.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneCampaignRelaunchTest, "AshesOfHeaven.LevelOne.CampaignE2E.RelaunchKeepsCompletion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneCampaignRelaunchTest::RunTest(const FString& Parameters)
{
	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_Relaunch"));

	// First session: finish the chapter, and leave behind the mid-level checkpoint a real run
	// would have written last. Escape is the stage the old build restored into after a
	// completed playthrough, so it is exactly the state that must NOT win on the next boot.
	{
		FLevelOneSession Session;
		if (!Session.Boot(/*bViaGameMode=*/false))
		{
			AddError(TEXT("first Level One session failed to boot"));
			Session.Teardown();
			return false;
		}
		UAHChapterSubsystem* Chapter = Session.Chapter();
		UAHObjectiveSubsystem* Objectives = Session.Objectives();
		UAHCheckpointSubsystem* Checkpoints = Session.Checkpoints();
		if (!Chapter || !Objectives || !Checkpoints)
		{
			AddError(TEXT("first session is missing Level One subsystems"));
			Session.Teardown();
			return false;
		}

		while (!Objectives->IsMissionComplete())
		{
			if (!Session.CompleteCurrentObjective())
			{
				break;
			}
			Session.TickDirector(1, 0.05f);
			if (Chapter->GetStage() == EAHChapterStage::Escape)
			{
				const FAHStageSpatialDefinition& Escape = AHChapterSpatial::GetStageDefinition(EAHChapterStage::Escape);
				Checkpoints->CaptureCheckpoint(Escape.CheckpointId);
			}
		}
		TestTrue(TEXT("first session finishes the chapter"), Chapter->IsChapterComplete());
		Session.Teardown();
	}

	// Everything in memory is gone: world destroyed, game instance shut down, subsystems
	// deinitialized. Only the .sav on disk survives, which is what a process restart leaves.
	{
		FLevelOneSession Session;
		if (!Session.Boot(/*bViaGameMode=*/true))
		{
			AddError(TEXT("relaunched Level One session failed to boot"));
			Session.Teardown();
			return false;
		}
		UAHChapterSubsystem* Chapter = Session.Chapter();
		UAHPlatformSaveSubsystem* Save = Session.Save();
		if (!Chapter || !Save)
		{
			AddError(TEXT("relaunched session is missing Level One subsystems"));
			Session.Teardown();
			return false;
		}

		TestTrue(TEXT("the save still records Level One as complete"), Save->IsChapterComplete(AHChapterIds::ChapterOne()));
		TestEqual(TEXT("the relaunched boot restores ChapterComplete, not the last checkpoint"), Chapter->GetStage(), EAHChapterStage::ChapterComplete);
		TestTrue(TEXT("the relaunched chapter reports completion"), Chapter->IsChapterComplete());
		TestEqual(TEXT("the relaunched objective index is terminal"), Chapter->GetState().ObjectiveIndex, AHChapterStateConstants::ObjectiveCount);
		TestFalse(TEXT("the failsafe clock is not running in a completed chapter"), Chapter->IsCountdownActive());
		Session.Teardown();
	}

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneCheckpointReloadTest, "AshesOfHeaven.LevelOne.CampaignE2E.DeathReloadRestoresRunState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneCheckpointReloadTest::RunTest(const FString& Parameters)
{
	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_Reload"));

	FLevelOneSession Session;
	if (!Session.Boot(/*bViaGameMode=*/false))
	{
		AddError(TEXT("Level One session failed to boot"));
		Session.Teardown();
		return false;
	}
	UAHChapterSubsystem* Chapter = Session.Chapter();
	UAHObjectiveSubsystem* Objectives = Session.Objectives();
	UAHCheckpointSubsystem* Checkpoints = Session.Checkpoints();
	if (!Chapter || !Objectives || !Checkpoints || !Session.Player)
	{
		AddError(TEXT("session is missing Level One subsystems or a player"));
		Session.Teardown();
		return false;
	}

	// Play forward to the Manticore section so the checkpoint carries real progress: the
	// restore path deliberately rejects an opening capture that has nothing worth restoring.
	while (Chapter->GetStage() != EAHChapterStage::ManticoreSection && !Objectives->IsMissionComplete())
	{
		if (!Session.CompleteCurrentObjective())
		{
			break;
		}
		Session.TickDirector(1, 0.05f);
	}
	TestEqual(TEXT("the run reaches the Manticore section"), Chapter->GetStage(), EAHChapterStage::ManticoreSection);

	const FAHStageSpatialDefinition& Manticore = AHChapterSpatial::GetStageDefinition(EAHChapterStage::ManticoreSection);
	Session.Player->SetActorLocation(Manticore.SafePlayerLocation);
	Checkpoints->MarkEncounterCompleted(FName(TEXT("Ch01_E2E_Encounter")));

	FAHAmmoState CheckpointAmmo;
	CheckpointAmmo.Magazine = 21;
	CheckpointAmmo.Reserve = 96;
	const float CheckpointHealth = 63.0f;
	const float CheckpointArmor = 41.0f;
	const int32 CheckpointGrenades = 3;
	if (UAHInventoryComponent* Inventory = Session.Player->GetInventoryComponent())
	{
		Inventory->SetSavedAmmo(CheckpointAmmo);
		Inventory->AddGrenades(CheckpointGrenades - Inventory->GetGrenades());
	}
	if (UAHHealthComponent* Health = Session.Player->GetHealthComponent())
	{
		Health->SetHealth(CheckpointHealth);
	}
	if (UAHArmorComponent* Armor = Session.Player->GetArmorComponent())
	{
		Armor->SetArmor(CheckpointArmor);
	}
	// The Manticore only has to exist for its state to reach the checkpoint. Boarding it is
	// deliberately left to the playthrough test: AAHChapterOneDirector::HandleVehicleEntered
	// completes the objective, which would move the chapter off ManticoreSection and make the
	// stage-matched capture below invalid before it is taken.
	TestNotNull(TEXT("the Manticore exists at its own stage"), Session.FindManticore());

	const int32 CheckpointObjective = Objectives->GetCurrentObjectiveIndex();
	TestTrue(TEXT("a mid-run checkpoint captures"), Checkpoints->CaptureCheckpoint(Manticore.CheckpointId));

	// Die: spend the run's state the way a failed attempt does before the reload.
	if (UAHHealthComponent* Health = Session.Player->GetHealthComponent())
	{
		Health->SetHealth(1.0f);
	}
	if (UAHArmorComponent* Armor = Session.Player->GetArmorComponent())
	{
		Armor->SetArmor(0.0f);
	}
	if (UAHInventoryComponent* Inventory = Session.Player->GetInventoryComponent())
	{
		Inventory->SetSavedAmmo(FAHAmmoState());
		Inventory->AddGrenades(-Inventory->GetGrenades());
	}
	Session.Player->SetActorLocation(Manticore.SafePlayerLocation + FVector(900.0f, 0.0f, 0.0f));
	Objectives->RestoreState(CheckpointObjective + 1);

	// AAHCombatPlayerController::FinishDeathRestart reopens the level and the game mode
	// restores on the far side; in-process the restore itself is the reachable half, so this
	// exercises UAHCheckpointSubsystem::RestoreLatestCheckpoint directly.
	TestTrue(TEXT("the latest checkpoint restores after death"), Checkpoints->RestoreLatestCheckpoint());

	if (UAHHealthComponent* Health = Session.Player->GetHealthComponent())
	{
		TestEqual(TEXT("health restores to the checkpoint value"), Health->GetHealth(), CheckpointHealth);
	}
	if (UAHArmorComponent* Armor = Session.Player->GetArmorComponent())
	{
		TestEqual(TEXT("armor restores to the checkpoint value"), Armor->GetArmor(), CheckpointArmor);
	}
	if (UAHInventoryComponent* Inventory = Session.Player->GetInventoryComponent())
	{
		TestEqual(TEXT("magazine ammo restores to the checkpoint value"), Inventory->GetSavedAmmo().Magazine, CheckpointAmmo.Magazine);
		TestEqual(TEXT("reserve ammo restores to the checkpoint value"), Inventory->GetSavedAmmo().Reserve, CheckpointAmmo.Reserve);
		TestEqual(TEXT("grenades restore to the checkpoint value"), Inventory->GetGrenades(), CheckpointGrenades);
	}
	TestEqual(TEXT("the objective cursor rewinds to the checkpoint"), Objectives->GetCurrentObjectiveIndex(), CheckpointObjective);
	TestEqual(TEXT("the chapter stage rewinds to the checkpoint"), Chapter->GetStage(), EAHChapterStage::ManticoreSection);
	TestTrue(TEXT("encounters completed before the checkpoint stay completed"), Checkpoints->IsEncounterCompleted(FName(TEXT("Ch01_E2E_Encounter"))));
	TestTrue(TEXT("the Manticore state is carried through the reload"), Chapter->GetState().Vehicle.bSpawned);

	// The run is still finishable after the reload: this is what makes the death cycle part of
	// the same end-to-end pass rather than a separate scenario.
	while (!Objectives->IsMissionComplete())
	{
		if (!Session.CompleteCurrentObjective())
		{
			break;
		}
		Session.TickDirector(1, 0.05f);
	}
	TestTrue(TEXT("the chapter still completes after a death and reload"), Chapter->IsChapterComplete());
	if (UAHPlatformSaveSubsystem* Save = Session.Save())
	{
		TestTrue(TEXT("completion after a reload is persisted"), Save->IsChapterComplete(AHChapterIds::ChapterOne()));
	}

	Session.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneFailsafeExpiryTest, "AshesOfHeaven.LevelOne.CampaignE2E.FailsafeExpiryFailsTheMission", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneFailsafeExpiryTest::RunTest(const FString& Parameters)
{
	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_Failsafe"));

	UGameInstance* GameInstance = NewObject<UGameInstance>(GetTransientPackage());
	UAHChapterSubsystem* Chapter = NewObject<UAHChapterSubsystem>(GameInstance);

	Chapter->StartCountdown(AHChapterStateConstants::FailsafeCountdownSeconds);
	TestTrue(TEXT("the failsafe clock starts active"), Chapter->IsCountdownActive());
	TestFalse(TEXT("a normal tick does not expire the clock"), Chapter->TickCountdown(30.0f));

	// Run it out. Expiry has to be reported exactly once, on the tick that reaches zero.
	bool bExpired = false;
	int32 ExpiryReports = 0;
	for (int32 Step = 0; Step < 40; ++Step)
	{
		if (Chapter->TickCountdown(30.0f))
		{
			bExpired = true;
			++ExpiryReports;
		}
	}
	TestTrue(TEXT("running the failsafe clock to zero reports expiry"), bExpired);
	TestEqual(TEXT("expiry is reported exactly once"), ExpiryReports, 1);
	TestFalse(TEXT("the clock is no longer active after expiry"), Chapter->IsCountdownActive());
	TestEqual(TEXT("the clock clamps at zero"), Chapter->GetCountdownSeconds(), 0.0f);

	// A completed chapter must never raise the failure: TickCountdown short-circuits on it.
	UAHChapterSubsystem* Completed = NewObject<UAHChapterSubsystem>(GameInstance);
	FAHChapterState CompletedState;
	CompletedState.ObjectiveIndex = AHChapterStateConstants::ObjectiveCount;
	CompletedState.CountdownSeconds = 1.0f;
	CompletedState.bCountdownActive = true;
	Completed->RestoreState(CompletedState);
	TestFalse(TEXT("a completed chapter never fails on the failsafe clock"), Completed->TickCountdown(30.0f));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneFailsafeCheckpointWindowTest, "AshesOfHeaven.LevelOne.CampaignE2E.FailsafeClockIsPerAttempt", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneFailsafeCheckpointWindowTest::RunTest(const FString& Parameters)
{
	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_FailsafeWindow"));

	FLevelOneSession Session;
	if (!Session.Boot(/*bViaGameMode=*/false))
	{
		AddError(TEXT("Level One session failed to boot"));
		Session.Teardown();
		return false;
	}
	UAHChapterSubsystem* Chapter = Session.Chapter();
	UAHObjectiveSubsystem* Objectives = Session.Objectives();
	UAHCheckpointSubsystem* Checkpoints = Session.Checkpoints();
	if (!Chapter || !Objectives || !Checkpoints || !Session.Player)
	{
		AddError(TEXT("session is missing Level One subsystems or a player"));
		Session.Teardown();
		return false;
	}

	// Reach the first timed stage that owns a checkpoint of its own. The clock starts at
	// FailsafeOrder, whose stage definition points back at the CathedralApproach checkpoint,
	// so the Cathedral interior is the first capture inside the timed window. The player is
	// deliberately not teleported: the authored safe transforms sit on the stage triggers,
	// and moving onto one completes the objective out from under the test.
	while (Chapter->GetStage() != EAHChapterStage::CathedralInterior && !Objectives->IsMissionComplete())
	{
		if (!Session.CompleteCurrentObjective())
		{
			break;
		}
		Session.TickDirector(1, 0.05f);
	}
	TestEqual(TEXT("the run reaches the Cathedral interior"), Chapter->GetStage(), EAHChapterStage::CathedralInterior);
	TestTrue(TEXT("the failsafe clock is still running inside the Cathedral"), Chapter->IsCountdownActive());

	Chapter->TickCountdown(AHChapterStateConstants::FailsafeCountdownSeconds - 5.0f);
	TestTrue(TEXT("the live clock is nearly out"), Chapter->GetCountdownSeconds() < 10.0f);

	const FAHStageSpatialDefinition& Interior = AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralInterior);
	TestTrue(TEXT("a checkpoint captures inside the timed section"), Checkpoints->CaptureCheckpoint(Interior.CheckpointId));

	FAHCombatCheckpointState Saved;
	if (UAHPlatformSaveSubsystem* Save = Session.Save())
	{
		TestTrue(TEXT("the checkpoint is on disk"), Save->LoadCombatCheckpoint(Saved));
	}
	TestEqual(TEXT("the checkpoint stores the full failsafe window, not the live remainder"), Saved.ChapterState.CountdownSeconds, AHChapterStateConstants::FailsafeCountdownSeconds);

	Session.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneStaleStageDialogueTest, "AshesOfHeaven.LevelOne.CampaignE2E.StaleStageDialogueIsDropped", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneStaleStageDialogueTest::RunTest(const FString& Parameters)
{
	// The window is the objective index, so stages that share one are the same beat's window
	// and a queued beat from the current window still plays.
	TestTrue(TEXT("CathedralInterior and SaelTransmission share one objective window"),
		UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::CathedralInterior)
			== UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::SaelTransmission));
	TestTrue(TEXT("Escape and OtherLucian share one objective window"),
		UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::Escape)
			== UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::OtherLucian));
	TestTrue(TEXT("the Escape window is later than the Cathedral interior window"),
		UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::Escape)
			> UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::CathedralInterior));

	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_Dialogue"));

	FLevelOneSession Session;
	if (!Session.Boot(/*bViaGameMode=*/false))
	{
		AddError(TEXT("Level One session failed to boot"));
		Session.Teardown();
		return false;
	}
	UAHChapterSubsystem* Chapter = Session.Chapter();
	UAHObjectiveSubsystem* Objectives = Session.Objectives();
	UAHDialogueSubsystem* Dialogue = Session.World ? Session.World->GetSubsystem<UAHDialogueSubsystem>() : nullptr;
	if (!Chapter || !Objectives || !Dialogue)
	{
		AddError(TEXT("session is missing the chapter or dialogue subsystem"));
		Session.Teardown();
		return false;
	}

	// Drive to the Cathedral interior, which is where the long terminal sequence holds the
	// dialogue channel and stage beats start stacking up behind it.
	while (Chapter->GetStage() != EAHChapterStage::CathedralInterior && !Objectives->IsMissionComplete())
	{
		if (!Session.CompleteCurrentObjective())
		{
			break;
		}
		Session.TickDirector(1, 0.05f);
	}
	TestEqual(TEXT("the run reaches the Cathedral interior"), Chapter->GetStage(), EAHChapterStage::CathedralInterior);

	// Occupy the channel, queue a beat for this stage, then move the chapter past it. Draining
	// the queue afterwards must discard the beat instead of playing it a stage late.
	TArray<FAHDialogueLine> HoldingLines;
	FAHDialogueLine Holding;
	Holding.Speaker = FName(TEXT("TEST"));
	Holding.Subtitle = FText::FromString(TEXT("holding the dialogue channel"));
	Holding.Duration = 600.0f;
	HoldingLines.Add(Holding);
	Dialogue->StartSequence(FName(TEXT("Ch01_E2E_ChannelHold")), HoldingLines, false);
	TestTrue(TEXT("the test sequence holds the dialogue channel"), Dialogue->HasActiveDialogue());

	const int32 PendingBefore = Dialogue->GetPendingStageEntryCount();
	Chapter->SetStage(EAHChapterStage::SaelTransmission);
	Session.TickDirector(1, 0.05f);

	// Advance the chapter well past the queued beat's objective window.
	while (Chapter->GetStage() != EAHChapterStage::ErebusDestruction && !Objectives->IsMissionComplete())
	{
		if (!Session.CompleteCurrentObjective())
		{
			break;
		}
		Session.TickDirector(1, 0.05f);
	}

	// Releasing the channel drains the queue. Nothing queued for an earlier objective window
	// may take it.
	Dialogue->SkipCurrentSequence();
	TestEqual(TEXT("no stage beat from a passed objective window survives the drain"), Dialogue->GetPendingStageEntryCount(), 0);
	const bool bPlayingStaleBeat = Dialogue->HasActiveDialogue()
		&& UAHChapterSubsystem::ObjectiveIndexForStage(Chapter->GetStage()) > UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::CathedralInterior)
		&& Dialogue->GetCurrentSequence() == FName(TEXT("Ch01_TerminalDecision"));
	TestFalse(TEXT("a beat from a passed objective window does not start playing"), bPlayingStaleBeat);
	TestTrue(TEXT("the queue did not grow while the channel was held"), Dialogue->GetPendingStageEntryCount() <= PendingBefore);

	Session.Teardown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneAuthoredZonesTest, "AshesOfHeaven.LevelOne.CampaignE2E.AuthoredPresentationZones", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneAuthoredZonesTest::RunTest(const FString& Parameters)
{
	using namespace AHLevelOneE2ESupport;
	FScopedCampaignSaveSlot SaveSlot(TEXT("AshesOfHeaven_E2E_Zones"));

	FLevelOneSession Session;
	if (!Session.Boot(/*bViaGameMode=*/false))
	{
		AddError(TEXT("Level One session failed to boot"));
		Session.Teardown();
		return false;
	}

	// Every authored zone that has a level must actually stream in. A zone that silently
	// fell back to the runtime primitives is the failure this guards: the section still
	// renders, so nothing looks broken, and the art quietly stops shipping.
	TMap<FName, int32> ActorsByZone;
	for (TActorIterator<AActor> It(Session.World); It; ++It)
	{
		for (const FName Tag : It->Tags)
		{
			const FString TagString = Tag.ToString();
			if (TagString.StartsWith(TEXT("AH.Zone.")))
			{
				ActorsByZone.FindOrAdd(FName(*TagString.RightChop(8))) += 1;
			}
		}
	}

	// The sections that have authored levels in the repository today. A zone added to the
	// director's table without art is expected to be absent here and fall back.
	const TArray<FName> AuthoredToday = {FName(TEXT("Erebus")), FName(TEXT("Transit")), FName(TEXT("Cathedral"))};
	for (const FAHAuthoredPresentationZone& Zone : AHAuthoredPresentationZones::Get())
	{
		const FName ZoneId(Zone.ZoneId);
		const int32* Count = ActorsByZone.Find(ZoneId);
		if (!AuthoredToday.Contains(ZoneId))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("authored zone %s streamed in"), Zone.ZoneId), Count != nullptr);
		if (Count)
		{
			TestTrue(*FString::Printf(TEXT("authored zone %s has at least %d actors (has %d)"), Zone.ZoneId, Zone.MinimumActors, *Count),
				*Count >= Zone.MinimumActors);
		}
	}

	// Mesh-level audit: no VISIBLE component in a corridor that now has authored art may
	// still reference a debug primitive from /Game/Ashes/Presentation/Meshes/SM_AH_*. The
	// hidden greybox collision layer underneath is exempt - it is gameplay, not presentation.
	struct FCorridor
	{
		const TCHAR* Name;
		float MinX;
		float MaxX;
	};
	const FCorridor Corridors[] = {
		{TEXT("Transit"), 2500.0f, 5600.0f},
		{TEXT("Cathedral"), 15600.0f, 19800.0f}
	};
	for (const FCorridor& Corridor : Corridors)
	{
		int32 VisibleDebugPrimitives = 0;
		for (TActorIterator<AActor> It(Session.World); It; ++It)
		{
			const float X = It->GetActorLocation().X;
			if (It->IsHidden() || X < Corridor.MinX || X > Corridor.MaxX)
			{
				continue;
			}
			// A glyph stroke is a thin emissive bar by design - authored symbol geometry, not a
			// placeholder standing in for a modelled asset. Architecture built from the same
			// primitive is still caught, because only the glyph carries this tag.
			if (It->Tags.Contains(FAHPresentationTags::Glyph))
			{
				continue;
			}
			TArray<UStaticMeshComponent*> MeshComponents;
			It->GetComponents(MeshComponents);
			for (const UStaticMeshComponent* MeshComponent : MeshComponents)
			{
				if (!MeshComponent || MeshComponent->bHiddenInGame || !MeshComponent->IsVisible() || !MeshComponent->GetStaticMesh())
				{
					continue;
				}
				if (MeshComponent->GetStaticMesh()->GetPathName().StartsWith(TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_")))
				{
					++VisibleDebugPrimitives;
					UE_LOG(LogTemp, Display, TEXT("[ZoneAudit][%s] %s uses %s at %s"), Corridor.Name,
						*It->GetName(), *MeshComponent->GetStaticMesh()->GetName(), *It->GetActorLocation().ToCompactString());
				}
			}
		}
		TestEqual(*FString::Printf(TEXT("no visible debug primitive mesh remains in the %s corridor"), Corridor.Name), VisibleDebugPrimitives, 0);
	}

	Session.Teardown();
	return true;
}

#endif
