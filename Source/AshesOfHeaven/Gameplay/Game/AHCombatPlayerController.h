#pragma once

#include "CoreMinimal.h"
#include "AshesOfHeavenPlayerController.h"
#include "Platform/AHPlatformTypes.h"
#include "AHCombatPlayerController.generated.h"

class AAHCombatPlayerCharacter;
class AAHCombatHUD;
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

protected:
	UFUNCTION()
	void HandleMobilePressed(EAHMobileTouchAction Action);

	UFUNCTION()
	void HandleMobileReleased(EAHMobileTouchAction Action);

	UFUNCTION()
	void HandlePlayerDeath();

	UFUNCTION()
	void HandleDamageFeedback(float Damage, bool bHeadshot, bool bArmorHit, bool bArmorBroken, float DirectionAngle);

	void HandleWeaponShot(const FHitResult& Hit, bool bHit);
	void HandleObjectiveChanged(FText Objective, int32 Index, int32 Count);
	void HandleMissionComplete();
	void FinishDeathRestart();

	AAHCombatHUD* GetCombatHUD() const;

	bool bGodMode = false;
	bool bInfiniteAmmo = false;
	FTimerHandle DeathRestartTimer;
};
