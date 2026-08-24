#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AHCombatPlayerCharacter.generated.h"

class UInputComponent;
class UInputAction;
class AAHWeaponBase;
class UStaticMeshComponent;

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

	/** Stride length per stance, in centimetres. Cadence is stride over current speed, so these
	 * are the numbers that decide whether the steps match the body: 145cm at the 420cm/s walk is
	 * 2.9 steps a second, which is a jog, which is what 420cm/s is. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=20.0))
	float WalkStride = 145.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=20.0))
	float SprintStride = 196.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=20.0))
	float CrouchStride = 62.0f;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Visual Target")
	TObjectPtr<UStaticMeshComponent> FirstPersonLeftGauntlet;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Visual Target")
	TObjectPtr<UStaticMeshComponent> FirstPersonRightGauntlet;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	virtual void DoMove(float Right, float Forward) override;
	virtual void DoAim(float Yaw, float Pitch) override;
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
	/** Shows the local weapon presentation after the opening curtain without exposing placeholder arms. */
	void SetFirstPersonPresentationVisible(bool bVisible);

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
	float LookInputEnableTime = 0.0f;
	int32 TicksSinceBeginPlay = 0;
};
