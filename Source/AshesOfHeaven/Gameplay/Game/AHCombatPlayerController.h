#pragma once

#include "CoreMinimal.h"
#include "AshesOfHeavenPlayerController.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Platform/AHPlatformTypes.h"
#include "AHCombatPlayerController.generated.h"

class AAHCombatPlayerCharacter;
class AAHCombatHUD;
class AAHManticoreVehicle;
class AAHChapterOneDirector;
class UAHObjectiveSubsystem;
class UAHObjectiveHUDDelegateTestReceiver;
enum class EAHMobileTouchAction : uint8;

UCLASS()
class ASHESOFHEAVEN_API AAHCombatPlayerController : public AAshesOfHeavenPlayerController
{
	GENERATED_BODY()

public:
	AAHCombatPlayerController();

	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;

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

	AAHCombatHUD* GetCombatHUD() const;

	friend class UAHObjectiveHUDDelegateTestReceiver;

	bool bGodMode = false;
	bool bInfiniteAmmo = false;
	FTimerHandle DeathRestartTimer;
};
