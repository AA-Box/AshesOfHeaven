#include "Gameplay/Game/AHChapterOneGameMode.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTrigger.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/Level/AHChapterOneDirector.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Parse.h"

AAHChapterOneGameMode::AAHChapterOneGameMode()
{
	DefaultPawnClass = AAHCombatPlayerCharacter::StaticClass();
	PlayerControllerClass = AAHCombatPlayerController::StaticClass();
	HUDClass = AAHCombatHUD::StaticClass();
}

void AAHChapterOneGameMode::BeginPlay()
{
	Super::BeginPlay();
	// The explicit -freshchapter/-resetprogress launch arguments work in every build
	// configuration, including Shipping: without them a Shipping install can never clear a
	// stale save. Normal launches (no argument) still restore the player's checkpoint.
	// One-shot, per process. The command line survives UGameplayStatics::OpenLevel, so without
	// this latch every death or failsafe reload re-ran ResetProgress() and deleted the save the
	// player had just earned: die once after launching with -freshchapter and the run silently
	// restarts from the opening checkpoint instead of the last one. "Start this session fresh"
	// must not mean "wipe my progress every time the level reloads".
	static bool GAHFreshChapterConsumed = false;
	const bool bFreshChapter = !GAHFreshChapterConsumed
		&& (FParse::Param(FCommandLine::Get(), TEXT("freshchapter"))
			|| FParse::Param(FCommandLine::Get(), TEXT("resetprogress")));
	GAHFreshChapterConsumed = true;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UAHChapterSubsystem* Chapter = GameInstance->GetSubsystem<UAHChapterSubsystem>())
		{
			if (UAHPlatformSaveSubsystem* Save = GameInstance->GetSubsystem<UAHPlatformSaveSubsystem>())
			{
				FAHCombatCheckpointState SavedState;
				if (bFreshChapter)
				{
					Save->ResetProgress();
					Chapter->RestoreState(FAHChapterState());
				}
				else if (Save->LoadCombatCheckpoint(SavedState)
					&& SavedState.ChapterState.SaveVersion > 0
					&& (SavedState.MapName.IsEmpty() || SavedState.MapName.Contains(TEXT("ChapterOne"), ESearchCase::IgnoreCase)))
				{
					Chapter->RestoreState(SavedState.ChapterState);
				}
				else
				{
					Chapter->RestoreState(FAHChapterState());
				}
				// Campaign completion outranks the checkpoint. The last checkpoint of a finished
				// run is a mid-level one (Escape), so restoring it alone is what made a completed
				// level report as unfinished after a relaunch. Applied here rather than after the
				// pawn spawns so the director builds the completed chapter on its first frame.
				if (!bFreshChapter && Save->IsChapterComplete(AHChapterIds::ChapterOne()))
				{
					FAHChapterState Completed = Chapter->GetState();
					Completed.ObjectiveIndex = AHChapterStateConstants::ObjectiveCount;
					// NormalizeState turns a final objective index into Stage=ChapterComplete
					// and clears the failsafe clock, so no second source of truth is introduced.
					Chapter->RestoreState(Completed);
				}
				UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Runtime] Map=%s Stage=%s ObjectiveIndex=%d Objective=%s ChapterComplete=%s CompletionWidgetVisible=false Checkpoint=%s SaveLoaded=%s FreshChapter=%s"),
					*GetWorld()->GetMapName(),
					*UEnum::GetValueAsString(Chapter->GetStage()),
					Chapter->GetState().ObjectiveIndex,
					*UEnum::GetValueAsString(Chapter->GetStage()),
					Chapter->IsChapterComplete() ? TEXT("true") : TEXT("false"),
					*Chapter->GetState().CheckpointId.ToString(),
					(!bFreshChapter && Save->HasSave()) ? TEXT("true") : TEXT("false"),
					bFreshChapter ? TEXT("true") : TEXT("false"));
			}
		}
	}
	GetWorld()->SpawnActor<AAHChapterOneDirector>(AAHChapterOneDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	GetWorldTimerManager().SetTimer(RestoreTimer, this, &AAHChapterOneGameMode::RestoreCheckpointAfterSpawn, 0.45f, false);
}

AActor* AAHChapterOneGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	const FAHStageSpatialDefinition& Opening = AHChapterSpatial::GetStageDefinition(EAHChapterStage::OpeningBlack);
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
	if (!Starts.IsEmpty())
	{
		Starts[0]->SetActorLocationAndRotation(Opening.SafePlayerLocation, Opening.SafePlayerRotation);
		return Starts[0];
	}
	return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), Opening.SafePlayerLocation, Opening.SafePlayerRotation);
}

void AAHChapterOneGameMode::RestoreCheckpointAfterSpawn()
{
	// The timer can fire before the pawn is possessed on a slow packaged boot; retry
	// instead of silently skipping both restore and the fresh-opening capture.
	if (!UGameplayStatics::GetPlayerCharacter(GetWorld(), 0) && RestoreAttempts++ < 8)
	{
		GetWorldTimerManager().SetTimer(RestoreTimer, this, &AAHChapterOneGameMode::RestoreCheckpointAfterSpawn, 0.25f, false);
		return;
	}
	// A completed chapter has nothing to restore: RestoreFromState would overwrite the
	// completed state with the last mid-level checkpoint it captured.
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (const UAHChapterSubsystem* Chapter = GameInstance->GetSubsystem<UAHChapterSubsystem>())
		{
			if (Chapter->IsChapterComplete())
			{
				LogObjective01SpatialState();
				return;
			}
		}
	}
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (!Checkpoints->RestoreLatestCheckpoint())
		{
			Checkpoints->RecoverToCanonicalStage();
			if (UGameInstance* GameInstance = GetGameInstance())
			{
				if (UAHChapterSubsystem* Chapter = GameInstance->GetSubsystem<UAHChapterSubsystem>())
				{
					Checkpoints->CaptureCheckpoint(AHChapterSpatial::GetStageDefinition(Chapter->GetStage()).CheckpointId);
				}
			}
		}
	}
	LogObjective01SpatialState();
	LogRestoredRunState();
}

void AAHChapterOneGameMode::LogRestoredRunState()
{
#if !UE_BUILD_SHIPPING
	// Emitted after the restore settles, on the far side of a real UGameplayStatics::OpenLevel.
	// The packaged E2E asserts against this line because it is the only place that reports what
	// actually survived the level reopen into brand-new actors, rather than what an in-process
	// test handed straight back to itself.
	UWorld* World = GetWorld();
	const AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	const UAHCheckpointSubsystem* Checkpoints = World ? World->GetSubsystem<UAHCheckpointSubsystem>() : nullptr;
	const UAHChapterSubsystem* Chapter = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr;
	if (!Player || !Checkpoints || !Chapter)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[LevelOneE2E] restored_state unavailable player=%d checkpoints=%d chapter=%d"), Player != nullptr, Checkpoints != nullptr, Chapter != nullptr);
		return;
	}
	const FAHCombatCheckpointState& Runtime = Checkpoints->GetRuntimeState();
	const FAHChapterState& State = Chapter->GetState();
	const FAHAmmoState Ammo = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetSavedAmmo() : FAHAmmoState();
	UE_LOG(LogAshesOfHeaven, Display,
		TEXT("[LevelOneE2E] restored_state checkpoint=%s stage=%s objective=%d health=%.0f armor=%.0f ammo=%d/%d grenades=%d encounters=%d vehicleSpawned=%s vehicleHealth=%.0f vehicleDestroyed=%s countdown=%.1f"),
		*Runtime.CheckpointId.ToString(),
		*UEnum::GetValueAsString(State.Stage),
		State.ObjectiveIndex,
		Player->GetHealthComponent() ? Player->GetHealthComponent()->GetHealth() : -1.0f,
		Player->GetArmorComponent() ? Player->GetArmorComponent()->GetArmor() : -1.0f,
		Ammo.Magazine, Ammo.Reserve,
		Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetGrenades() : -1,
		State.CompletedEncounters.Num(),
		State.Vehicle.bSpawned ? TEXT("true") : TEXT("false"),
		State.Vehicle.Health,
		State.Vehicle.bDestroyed ? TEXT("true") : TEXT("false"),
		State.CountdownSeconds);
#endif
}

void AAHChapterOneGameMode::LogObjective01SpatialState()
{
	UWorld* World = GetWorld();
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0);
	APlayerController* Controller = UGameplayStatics::GetPlayerController(World, 0);
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const FRotator PlayerRotation = Player ? Player->GetActorRotation() : FRotator::ZeroRotator;
	const FVector CameraLocation = Controller && Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetCameraLocation() : FVector::ZeroVector;
	const FRotator CameraRotation = Controller && Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetCameraRotation() : FRotator::ZeroRotator;

	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(World, APlayerStart::StaticClass(), Starts);
	const FVector StartLocation = Starts.IsEmpty() ? FVector::ZeroVector : Starts[0]->GetActorLocation();

	FVector ObjectiveTarget = FVector::ZeroVector;
	for (TActorIterator<AAHChapterTrigger> It(World); It; ++It)
	{
		if (It->TriggerId == FName(TEXT("ReachDefensiveLine")))
		{
			ObjectiveTarget = It->GetActorLocation();
			break;
		}
	}

	FHitResult Ground;
	const bool bGroundHit = Player && World->LineTraceSingleByChannel(Ground, PlayerLocation, PlayerLocation - FVector(0.0f, 0.0f, 1500.0f), ECC_Visibility);

	int32 NearbyPresentation = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("Phase4Presentation"))) && FVector::Dist2D(It->GetActorLocation(), PlayerLocation) < 2500.0f)
		{
			++NearbyPresentation;
		}
	}

	EAHChapterStage Stage = EAHChapterStage::OpeningBlack;
	int32 ObjectiveIndex = INDEX_NONE;
	FName CheckpointId = NAME_None;
	FVector CheckpointLocation = FVector::ZeroVector;
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UAHChapterSubsystem* Chapter = GameInstance->GetSubsystem<UAHChapterSubsystem>())
		{
			Stage = Chapter->GetStage();
			ObjectiveIndex = Chapter->GetState().ObjectiveIndex;
			CheckpointId = Chapter->GetState().CheckpointId;
		}
	}
	if (UAHCheckpointSubsystem* Checkpoints = World->GetSubsystem<UAHCheckpointSubsystem>())
	{
		CheckpointLocation = Checkpoints->GetRuntimeState().PlayerLocation;
	}

	UE_LOG(LogAshesOfHeaven, Display,
		TEXT("[Phase4.4.2][SpatialState] Map=%s Stage=%s ObjectiveIndex=%d Player=%s Rot=%s Camera=%s CamRot=%s PlayerStart=%s Checkpoint=%s CheckpointLoc=%s GroundHit=%s GroundZ=%.1f GroundDist=%.1f Objective01Target=%s NearbyPresentationActors=%d"),
		*World->GetMapName(),
		*UEnum::GetValueAsString(Stage),
		ObjectiveIndex,
		*PlayerLocation.ToCompactString(),
		*PlayerRotation.ToCompactString(),
		*CameraLocation.ToCompactString(),
		*CameraRotation.ToCompactString(),
		*StartLocation.ToCompactString(),
		*CheckpointId.ToString(),
		*CheckpointLocation.ToCompactString(),
		bGroundHit ? TEXT("true") : TEXT("false"),
		bGroundHit ? Ground.ImpactPoint.Z : -9999.0f,
		bGroundHit ? PlayerLocation.Z - Ground.ImpactPoint.Z : -1.0f,
		*ObjectiveTarget.ToCompactString(),
		NearbyPresentation);
}
