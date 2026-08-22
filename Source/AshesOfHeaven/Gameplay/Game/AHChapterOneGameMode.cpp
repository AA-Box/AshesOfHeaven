#include "Gameplay/Game/AHChapterOneGameMode.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
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
	bool bFreshChapter = false;
#if !UE_BUILD_SHIPPING
	bFreshChapter = FParse::Param(FCommandLine::Get(), TEXT("freshchapter"))
		|| FParse::Param(FCommandLine::Get(), TEXT("resetprogress"));
#endif
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
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (!Checkpoints->RestoreLatestCheckpoint())
		{
			Checkpoints->CaptureCheckpoint(FName(TEXT("Ch01_Opening")));
		}
	}
}
