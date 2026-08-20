#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AHCombatPlayerCharacter.generated.h"

class UInputComponent;
class UInputAction;
class AAHWeaponBase;

UCLASS()
class ASHESOFHEAVEN_API AAHCombatPlayerCharacter : public AAHCombatantCharacter
{
	GENERATED_BODY()

public:
	AAHCombatPlayerCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin=100.0))
	float WalkSpeed = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin=100.0))
	float SprintSpeed = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin=100.0))
	float CrouchSpeed = 230.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin=0.0, ClampMax=1.0))
	float MilitaryAirControl = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement", meta=(ClampMin=0.0))
	float MantleDistance = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float HipFOV = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ADSFOV = 67.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapons")
	TSubclassOf<AAHWeaponBase> StartingWeaponClass;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void DoMove(float Right, float Forward) override;
	virtual void DoJumpStart() override;
	virtual void DoJumpEnd() override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	void StartFire();
	void StopFire();
	void StartADS();
	void StopADS();
	void Reload();
	void StartSprint();
	void StopSprint();
	void StartCrouch();
	void StopCrouch();
	void Melee();
	void ThrowGrenade();
	void Interact();
	void NextWeapon();
	void PreviousWeapon();

	virtual void OnDeathStarted() override;

	bool IsSprinting() const { return bSprinting; }
	bool IsCrouchedForCombat() const { return bCrouched; }
	void SetGodMode(bool bEnabled) { bGodMode = bEnabled; }

protected:
	bool TryMantle();
	void RefreshMovementSpeed();

	bool bSprinting = false;
	bool bCrouched = false;
	bool bGodMode = false;
};
