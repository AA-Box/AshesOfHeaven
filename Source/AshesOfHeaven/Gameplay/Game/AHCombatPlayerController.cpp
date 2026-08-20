#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Platform/AHMobileControlsWidget.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

AAHCombatPlayerController::AAHCombatPlayerController()
{
}

void AAHCombatPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UAHMobileControlsWidget* Mobile = Cast<UAHMobileControlsWidget>(MobileControlsWidget))
	{
		Mobile->OnActionPressed.AddDynamic(this, &AAHCombatPlayerController::HandleMobilePressed);
		Mobile->OnActionReleased.AddDynamic(this, &AAHCombatPlayerController::HandleMobileReleased);
	}
	if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
	{
		Objectives->OnObjectiveChanged.AddDynamic(this, &AAHCombatPlayerController::HandleObjectiveChanged);
		Objectives->OnMissionComplete.AddDynamic(this, &AAHCombatPlayerController::HandleMissionComplete);
		HandleObjectiveChanged(Objectives->GetCurrentObjective().DisplayText, Objectives->GetCurrentObjectiveIndex(), Objectives->GetObjectiveCount());
	}
}

void AAHCombatPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(InPawn))
	{
		Player->OnCombatantDeath.AddDynamic(this, &AAHCombatPlayerController::HandlePlayerDeath);
		Player->OnDamageFeedback.AddDynamic(this, &AAHCombatPlayerController::HandleDamageFeedback);
		Player->OnWeaponShot.AddUObject(this, &AAHCombatPlayerController::HandleWeaponShot);
	}
}

AAHCombatHUD* AAHCombatPlayerController::GetCombatHUD() const
{
	return Cast<AAHCombatHUD>(GetHUD());
}

void AAHCombatPlayerController::HandleMobilePressed(EAHMobileTouchAction Action)
{
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn());
	if (!Player)
	{
		return;
	}
	switch (Action)
	{
	case EAHMobileTouchAction::Fire: Player->StartFire(); break;
	case EAHMobileTouchAction::ADS: Player->StartADS(); break;
	case EAHMobileTouchAction::Jump: Player->DoJumpStart(); break;
	case EAHMobileTouchAction::Crouch: Player->StartCrouch(); break;
	case EAHMobileTouchAction::Reload: Player->Reload(); break;
	case EAHMobileTouchAction::Interact: Player->Interact(); break;
	case EAHMobileTouchAction::Grenade: Player->ThrowGrenade(); break;
	case EAHMobileTouchAction::Melee: Player->Melee(); break;
	case EAHMobileTouchAction::Sprint: Player->StartSprint(); break;
	case EAHMobileTouchAction::WeaponNext: Player->NextWeapon(); break;
	case EAHMobileTouchAction::WeaponPrevious: Player->PreviousWeapon(); break;
	default: break;
	}
}

void AAHCombatPlayerController::HandleMobileReleased(EAHMobileTouchAction Action)
{
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn());
	if (!Player)
	{
		return;
	}
	switch (Action)
	{
	case EAHMobileTouchAction::Fire: Player->StopFire(); break;
	case EAHMobileTouchAction::ADS: Player->StopADS(); break;
	case EAHMobileTouchAction::Jump: Player->DoJumpEnd(); break;
	case EAHMobileTouchAction::Crouch: Player->StopCrouch(); break;
	case EAHMobileTouchAction::Sprint: Player->StopSprint(); break;
	default: break;
	}
}

void AAHCombatPlayerController::HandlePlayerDeath()
{
	GetWorldTimerManager().SetTimer(DeathRestartTimer, this, &AAHCombatPlayerController::FinishDeathRestart, 1.35f, false);
}

void AAHCombatPlayerController::FinishDeathRestart()
{
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		Checkpoints->ReloadLatestCheckpoint();
	}
}

void AAHCombatPlayerController::HandleDamageFeedback(float Damage, bool bHeadshot, bool bArmorHit, bool bArmorBroken, float DirectionAngle)
{
	if (bGodMode)
	{
		return;
	}
	if (AAHCombatHUD* HUD = GetCombatHUD())
	{
		HUD->ShowDamageFeedback(bArmorBroken || bArmorHit, DirectionAngle);
	}
}

void AAHCombatPlayerController::HandleWeaponShot(const FHitResult& Hit, bool bHit)
{
	if (bHit)
	{
		if (AAHCombatHUD* HUD = GetCombatHUD())
		{
			HUD->ShowHitMarker(Hit.BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase));
		}
	}
	if (bInfiniteAmmo)
	{
		if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn()))
		{
			if (Player->GetInventoryComponent() && Player->GetInventoryComponent()->GetCurrentWeapon())
			{
				Player->GetInventoryComponent()->GetCurrentWeapon()->AddReserveAmmo(1);
			}
		}
	}
}

void AAHCombatPlayerController::HandleObjectiveChanged(FText Objective, int32 Index, int32 Count)
{
	if (AAHCombatHUD* HUD = GetCombatHUD())
	{
		HUD->SetObjective(Objective, Index, Count);
	}
}

void AAHCombatPlayerController::HandleMissionComplete()
{
	if (AAHCombatHUD* HUD = GetCombatHUD())
	{
		HUD->ShowMissionComplete();
	}
}

void AAHCombatPlayerController::SetGodMode(bool bEnabled)
{
	bGodMode = bEnabled;
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn()))
	{
		Player->SetGodMode(bEnabled);
	}
}

void AAHCombatPlayerController::SetInfiniteAmmo(bool bEnabled)
{
	bInfiniteAmmo = bEnabled;
}

void AAHCombatPlayerController::ReloadCheckpoint()
{
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		Checkpoints->ReloadLatestCheckpoint();
	}
}

void AAHCombatPlayerController::RestartEncounter()
{
	ReloadCheckpoint();
}

void AAHCombatPlayerController::KillAllEnemies()
{
	for (TActorIterator<AAHCombatantCharacter> It(GetWorld()); It; ++It)
	{
		if (It->GetFaction() == EAHFaction::Veil && !It->IsCombatantDead())
		{
			UGameplayStatics::ApplyDamage(*It, 99999.0f, this, this, nullptr);
		}
	}
}

void AAHCombatPlayerController::AIDebug()
{
	for (TActorIterator<AAHCombatAIController> It(GetWorld()); It; ++It)
	{
		It->DebugDrawAI();
	}
}

void AAHCombatPlayerController::ObjectiveDebug()
{
	if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
	{
		Objectives->DebugAdvanceObjective();
	}
}

void AAHCombatPlayerController::SpawnPilgrim()
{
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn()))
	{
		GetWorld()->SpawnActor<AAHVeilPilgrimCharacter>(AAHVeilPilgrimCharacter::StaticClass(), Player->GetActorLocation() + Player->GetActorForwardVector() * 450.0f, Player->GetActorRotation());
	}
}

void AAHCombatPlayerController::TeleportToEncounter(int32 EncounterIndex)
{
	TArray<AActor*> Encounters;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAHCombatEncounter::StaticClass(), Encounters);
	if (Encounters.IsValidIndex(EncounterIndex - 1))
	{
		if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn()))
		{
			Player->SetActorLocation(Encounters[EncounterIndex - 1]->GetActorLocation());
		}
	}
}
