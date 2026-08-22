#include "Gameplay/Game/AHChapterOneGameMode.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTrigger.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Camera/PlayerCameraManager.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/Level/AHChapterOneDirector.h"
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
	const bool bFreshChapter = FParse::Param(FCommandLine::Get(), TEXT("freshchapter"))
		|| FParse::Param(FCommandLine::Get(), TEXT("resetprogress"));
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
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
	if (!Starts.IsEmpty())
	{
		return Starts[0];
	}
	return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(-1400.0f, 0.0f, 120.0f), FRotator::ZeroRotator);
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
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (!Checkpoints->RestoreLatestCheckpoint())
		{
			Checkpoints->CaptureCheckpoint(FName(TEXT("Ch01_Opening")));
		}
	}
	LogObjective01SpatialState();
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
