#pragma once

#include "CoreMinimal.h"
#include "AshesOfHeavenPlayerController.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Platform/AHPlatformTypes.h"
#include "AHCombatPlayerController.generated.h"

class AAHCombatPlayerCharacter;
class AAHCombatHUD;
class UAHHUDRootWidget;
class AAHManticoreVehicle;
class AAHChapterOneDirector;
class UAHObjectiveSubsystem;
class UAHObjectiveHUDDelegateTestReceiver;
class UAHGameMenuWidget;
enum class EAHMenuMode : uint8;
enum class EAHMobileTouchAction : uint8;

UCLASS()
class ASHESOFHEAVEN_API AAHCombatPlayerController : public AAshesOfHeavenPlayerController
{
	GENERATED_BODY()

public:
	AAHCombatPlayerController();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void SetupInputComponent() override;

	// --- Game menu (front end at boot, pause menu on ESC) -------------------
	void OpenGameMenu(EAHMenuMode MenuMode);
	void CloseGameMenu();
	void TogglePauseMenu();
	void MenuStartNewGame();
	void MenuExitGame();
	bool IsGameMenuOpen() const;
	UAHGameMenuWidget* GetGameMenu() const { return GameMenu; }

	/** Console/test entry into the same menu handlers (Primary/Continue/Restart/Controls/Options/Back/Exit). */
	UFUNCTION(Exec)
	void MenuAction(const FString& ActionName);

	UFUNCTION(Exec)
	void SetGodMode(bool bEnabled = true);

	UFUNCTION(Exec)
	void SetInfiniteAmmo(bool bEnabled = true);

	UFUNCTION(Exec)
	void ReloadCheckpoint();

	UFUNCTION(Exec)
	void RestartEncounter();

	UFUNCTION(Exec)
	void KillAllEnemies();

	UFUNCTION(Exec)
	void AIDebug();

	UFUNCTION(Exec)
	void ObjectiveDebug();

	UFUNCTION(Exec)
	void SpawnPilgrim();

	UFUNCTION(Exec)
	void TeleportToEncounter(int32 EncounterIndex = 1);

	UFUNCTION(Exec)
	void ChapterSkipStage(int32 StageIndex = 0);

	UFUNCTION(Exec)
	void ChapterLoadCheckpoint(FName CheckpointId = NAME_None);

	UFUNCTION(Exec)
	void ChapterReset();

	UFUNCTION(Exec)
	void ChapterCompleteEncounter();

	UFUNCTION(Exec)
	void ChapterSetCountdown(float Seconds = 522.0f);

	UFUNCTION(Exec)
	void ChapterSpawnManticore();

	UFUNCTION(Exec)
	void ChapterTeleportCathedral();

	UFUNCTION(Exec)
	void ChapterTeleportPresent();

protected:
	UFUNCTION()
	void HandleObjectiveChanged(FText Objective, int32 Index, int32 Count);

	UFUNCTION()
	void HandleMissionComplete();

	UFUNCTION()
	void HandleMobilePressed(EAHMobileTouchAction Action);

	UFUNCTION()
	void HandleMobileReleased(EAHMobileTouchAction Action);

	UFUNCTION()
	void HandleMobileVehicleAxes(float Steering, float Camera);

	UFUNCTION()
	void HandlePlayerDeath();

	UFUNCTION()
	void HandleDamageFeedback(float Damage, bool bHeadshot, bool bArmorHit, bool bArmorBroken, float DirectionAngle);

	void HandleWeaponShot(const FHitResult& Hit, bool bHit);
	void FinishDeathRestart();
	void BindObjectiveEvents(UAHObjectiveSubsystem* Objectives);
	void UpdatePresentationState();

	AAHCombatHUD* GetCombatHUD() const;

	friend class UAHObjectiveHUDDelegateTestReceiver;

	void MaybeOpenFrontEndMenu();

	bool bGodMode = false;
	bool bInfiniteAmmo = false;
	FTimerHandle DeathRestartTimer;
	TWeakObjectPtr<UAHHUDRootWidget> BoundPresentationRoot;
	bool bOpeningPresentationState = false;

	UPROPERTY(Transient)
	TObjectPtr<UAHGameMenuWidget> GameMenu;
};
