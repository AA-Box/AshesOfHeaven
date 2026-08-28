#include "Gameplay/Game/AHCombatPlayerController.h"
#include "AshesOfHeaven.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Level/AHChapterOneDirector.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Gameplay/UI/AHHUDRootWidget.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Gameplay/UI/AHGameMenuWidget.h"
#include "Platform/AHMobileControlsWidget.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

namespace
{
	// Suppresses the boot front end for the in-session reload triggered by "NEW EXPEDITION".
	bool GAHSkipFrontEndOnce = false;
}

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
		Mobile->OnVehicleAxesChanged.AddDynamic(this, &AAHCombatPlayerController::HandleMobileVehicleAxes);
	}
	if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
	{
		BindObjectiveEvents(Objectives);
	}
	MaybeOpenFrontEndMenu();
}

void AAHCombatPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		FInputKeyBinding& EscapeBinding = InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AAHCombatPlayerController::TogglePauseMenu);
		EscapeBinding.bExecuteWhenPaused = true;
		EscapeBinding.bConsumeInput = true;
		FInputKeyBinding& StartBinding = InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed, this, &AAHCombatPlayerController::TogglePauseMenu);
		StartBinding.bExecuteWhenPaused = true;
	}
}

void AAHCombatPlayerController::MaybeOpenFrontEndMenu()
{
	// The front end is a real product surface, never a test surface: automation,
	// commandlets and evidence captures must keep booting straight into the field.
	if (GIsAutomationTesting || IsRunningCommandlet() || !IsLocalPlayerController())
	{
		return;
	}
	// -LevelOneAutoplay implies -NoFrontEnd. The front end opens paused and waits for input,
	// and an unattended run has none, so leaving it up parks the world paused forever: no
	// timers, no dialogue, no autoplay, and a packaged E2E that looks like a hang.
	if (FParse::Param(FCommandLine::Get(), TEXT("NoFrontEnd")) ||
		FParse::Param(FCommandLine::Get(), TEXT("LevelOneAutoplay")) ||
		FCString::Strifind(FCommandLine::Get(), TEXT("-ArtTarget=")) != nullptr)
	{
		return;
	}
	if (GAHSkipFrontEndOnce)
	{
		GAHSkipFrontEndOnce = false;
		return;
	}

	FString MenuCapture;
	const bool bHasCapture = FParse::Value(FCommandLine::Get(), TEXT("MenuCapture="), MenuCapture);
	if (bHasCapture && MenuCapture.Equals(TEXT("Pause"), ESearchCase::IgnoreCase))
	{
		OpenGameMenu(EAHMenuMode::Pause);
		return;
	}
	OpenGameMenu(EAHMenuMode::FrontEnd);
	if (bHasCapture && GameMenu)
	{
		if (MenuCapture.Equals(TEXT("Controls"), ESearchCase::IgnoreCase))
		{
			GameMenu->ShowPage(EAHMenuPage::Controls);
		}
		else if (MenuCapture.Equals(TEXT("Options"), ESearchCase::IgnoreCase))
		{
			GameMenu->ShowPage(EAHMenuPage::Options);
		}
	}
}

void AAHCombatPlayerController::OpenGameMenu(EAHMenuMode MenuMode)
{
	if (GameMenu)
	{
		GameMenu->RemoveFromParent();
		GameMenu = nullptr;
	}
	GameMenu = CreateWidget<UAHGameMenuWidget>(this, UAHGameMenuWidget::StaticClass(), TEXT("GameMenu"));
	if (!GameMenu)
	{
		return;
	}
	GameMenu->InitializeMenu(MenuMode);
	GameMenu->AddToViewport(120);
	SetPause(true);
	SetShowMouseCursor(true);
	FInputModeUIOnly InputMode;
	InputMode.SetWidgetToFocus(GameMenu->TakeWidget());
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Menu] opened mode=%s"),
		MenuMode == EAHMenuMode::FrontEnd ? TEXT("FrontEnd") : TEXT("Pause"));
}

void AAHCombatPlayerController::CloseGameMenu()
{
	if (!GameMenu)
	{
		return;
	}
	GameMenu->RemoveFromParent();
	GameMenu = nullptr;
	SetPause(false);
	SetShowMouseCursor(false);
	SetInputMode(FInputModeGameOnly());
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Menu] closed"));
}

bool AAHCombatPlayerController::IsGameMenuOpen() const
{
	return GameMenu != nullptr && GameMenu->IsInViewport();
}

void AAHCombatPlayerController::TogglePauseMenu()
{
	if (IsGameMenuOpen())
	{
		// The front end never closes on ESC alone; the field starts on an explicit choice.
		if (GameMenu->GetMode() == EAHMenuMode::Pause)
		{
			CloseGameMenu();
		}
		return;
	}
	OpenGameMenu(EAHMenuMode::Pause);
}

void AAHCombatPlayerController::MenuStartNewGame()
{
	UAHPlatformSaveSubsystem* Save = GetGameInstance() ? GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>() : nullptr;
	const bool bHadSave = Save && Save->HasSave();
	if (bHadSave)
	{
		Save->ResetProgress();
		GAHSkipFrontEndOnce = true;
		CloseGameMenu();
		UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this)));
		return;
	}
	CloseGameMenu();
}

void AAHCombatPlayerController::MenuExitGame()
{
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Menu] exit requested"));
	ConsoleCommand(TEXT("quit"));
}

void AAHCombatPlayerController::MenuAction(const FString& ActionName)
{
	if (ActionName.Equals(TEXT("OpenPause"), ESearchCase::IgnoreCase))
	{
		OpenGameMenu(EAHMenuMode::Pause);
		return;
	}
	if (ActionName.Equals(TEXT("OpenFrontEnd"), ESearchCase::IgnoreCase))
	{
		OpenGameMenu(EAHMenuMode::FrontEnd);
		return;
	}
	if (GameMenu && !GameMenu->ActivateAction(ActionName))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Menu] unknown action %s"), *ActionName);
	}
}

void AAHCombatPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdatePresentationState();
}

void AAHCombatPlayerController::UpdatePresentationState()
{
	AAHCombatHUD* HUD = GetCombatHUD();
	UAHHUDRootWidget* Root = HUD ? HUD->GetRootWidget() : nullptr;
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetPawn());
	AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(GetPawn());
	AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass()));
	const bool bOpening = Director && Director->IsOpeningPresentationActive();
	if (Root && (Root != BoundPresentationRoot || bOpening != bOpeningPresentationState))
	{
		BoundPresentationRoot = Root;
		bOpeningPresentationState = bOpening;
		Root->SetGameplayPresentationVisible(!bOpening);
		if (Player)
		{
			Player->SetFirstPersonPresentationVisible(!bOpening);
		}
		SetIgnoreMoveInput(bOpening);
		SetIgnoreLookInput(bOpening);
	}
	if (!Root || bOpening)
	{
		return;
	}
	if (Vehicle)
	{
		Root->SetCrosshairState(false, 0.0f, false, false, false, true);
		return;
	}
	if (Player)
	{
		AAHWeaponBase* Weapon = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetCurrentWeapon() : nullptr;
		const bool bADS = Player->IsAimingDownSights();
		const float Spread = Weapon ? (bADS ? Weapon->ADSSpreadDegrees : Weapon->HipSpreadDegrees) : 0.0f;
		const bool bInteraction = Player->GetInteractionComponent() && Player->GetInteractionComponent()->GetCurrentTarget() != nullptr;
		Root->SetCrosshairState(bADS, Spread, false, false, bInteraction, false);
	}
}

void AAHCombatPlayerController::BindObjectiveEvents(UAHObjectiveSubsystem* Objectives)
{
	if (!Objectives)
	{
		return;
	}
	Objectives->OnObjectiveChanged.AddDynamic(this, &AAHCombatPlayerController::HandleObjectiveChanged);
	Objectives->OnMissionComplete.AddDynamic(this, &AAHCombatPlayerController::HandleMissionComplete);
	HandleObjectiveChanged(Objectives->GetCurrentObjective().DisplayText, Objectives->GetCurrentObjectiveIndex(), Objectives->GetObjectiveCount());
}

void AAHCombatPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	if (AAHCombatHUD* HUD = GetCombatHUD())
	{
		HUD->SetPossessedPawn(InPawn);
	}
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
	if (AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(GetPawn()))
	{
		switch (Action)
		{
		case EAHMobileTouchAction::Fire: Vehicle->FireMountedWeapon(); break;
		case EAHMobileTouchAction::VehicleAccelerate: Vehicle->SetMobileThrottle(1.0f); break;
		case EAHMobileTouchAction::VehicleBrake: Vehicle->SetMobileThrottle(-1.0f); break;
		case EAHMobileTouchAction::VehicleExit: Vehicle->ExitVehicle(); break;
		case EAHMobileTouchAction::VehicleSwitchSeat: Vehicle->FireMountedWeapon(); break;
		default: break;
		}
		return;
	}

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
	if (AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(GetPawn()))
	{
		if (Action == EAHMobileTouchAction::VehicleAccelerate || Action == EAHMobileTouchAction::VehicleBrake)
		{
			Vehicle->SetMobileThrottle(0.0f);
		}
		return;
	}

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

void AAHCombatPlayerController::HandleMobileVehicleAxes(float Steering, float Camera)
{
	if (AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(GetPawn()))
	{
		Vehicle->SetMobileSteering(Steering);
		AddYawInput(Camera);
	}
}

void AAHCombatPlayerController::HandlePlayerDeath()
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase3.2][Player] death restart_scheduled=true"));
	#endif
	GetWorldTimerManager().SetTimer(DeathRestartTimer, this, &AAHCombatPlayerController::FinishDeathRestart, 1.35f, false);
}

void AAHCombatPlayerController::FinishDeathRestart()
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Player] death_restart_execute"));
	#endif
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
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][HUD] objective index=%d/%d text=%s"), Index, Count, *Objective.ToString());
	#endif
	if (AAHCombatHUD* HUD = GetCombatHUD())
	{
		if (HUD->IsMissionCompleteDisplayed())
		{
			HUD->HideMissionComplete();
		}
		HUD->SetObjective(Objective, Index, Count);
	}
}

void AAHCombatPlayerController::HandleMissionComplete()
{
	if (UAHChapterSubsystem* Chapter = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>()
		: nullptr)
	{
		if (!Chapter->IsChapterComplete())
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.4][HUD] ignored objective completion before ChapterComplete stage=%s objective=%d"),
				*UEnum::GetValueAsString(Chapter->GetStage()), Chapter->GetState().ObjectiveIndex);
			if (AAHCombatHUD* HUD = GetCombatHUD())
			{
				HUD->HideMissionComplete();
			}
			return;
		}
	}
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][HUD] mission_complete_display stage=ChapterComplete"));
	#endif
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

void AAHCombatPlayerController::ChapterSkipStage(int32 StageIndex)
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		const int32 MaxStage = static_cast<int32>(EAHChapterStage::ChapterComplete);
		Director->DebugSkipToStage(static_cast<EAHChapterStage>(FMath::Clamp(StageIndex, 0, MaxStage)));
	}
#endif
}

void AAHCombatPlayerController::ChapterLoadCheckpoint(FName CheckpointId)
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugLoadCheckpoint(CheckpointId);
	}
#endif
}

void AAHCombatPlayerController::ChapterReset()
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugResetChapter();
	}
#endif
}

void AAHCombatPlayerController::ChapterCompleteEncounter()
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugCompleteCurrentEncounter();
	}
#endif
}

void AAHCombatPlayerController::ChapterSetCountdown(float Seconds)
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugSetCountdown(Seconds);
	}
#endif
}

void AAHCombatPlayerController::ChapterSpawnManticore()
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugSpawnManticore();
	}
#endif
}

void AAHCombatPlayerController::ChapterTeleportCathedral()
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugTeleportToCathedral();
	}
#endif
}

void AAHCombatPlayerController::ChapterTeleportPresent()
{
#if !UE_BUILD_SHIPPING
	if (AAHChapterOneDirector* Director = Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHChapterOneDirector::StaticClass())))
	{
		Director->DebugTeleportToPresentDay();
	}
#endif
}
