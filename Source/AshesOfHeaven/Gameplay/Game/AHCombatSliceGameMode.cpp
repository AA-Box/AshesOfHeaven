#include "Gameplay/Game/AHCombatSliceGameMode.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Gameplay/Level/AHErebusCombatSliceDirector.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

AAHCombatSliceGameMode::AAHCombatSliceGameMode()
{
	DefaultPawnClass = AAHCombatPlayerCharacter::StaticClass();
	PlayerControllerClass = AAHCombatPlayerController::StaticClass();
	HUDClass = AAHCombatHUD::StaticClass();
}

void AAHCombatSliceGameMode::BeginPlay()
{
	Super::BeginPlay();
	GetWorld()->SpawnActor<AAHErebusCombatSliceDirector>(AAHErebusCombatSliceDirector::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	GetWorldTimerManager().SetTimer(RestoreTimer, this, &AAHCombatSliceGameMode::RestoreCheckpointAfterSpawn, 0.45f, false);
}

AActor* AAHCombatSliceGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	TArray<AActor*> Starts;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
	if (!Starts.IsEmpty())
	{
		return Starts[0];
	}

	return GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), FVector(-900.0f, 0.0f, 110.0f), FRotator::ZeroRotator);
}

void AAHCombatSliceGameMode::RestoreCheckpointAfterSpawn()
{
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (!Checkpoints->RestoreLatestCheckpoint())
		{
			Checkpoints->CaptureCheckpoint(FName(TEXT("Checkpoint_1")));
		}
	}
}
