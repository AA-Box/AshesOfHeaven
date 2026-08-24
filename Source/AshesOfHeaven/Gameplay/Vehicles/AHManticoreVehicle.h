#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "AHManticoreVehicle.generated.h"

class UCameraComponent;
class UInputAction;
class USpringArmComponent;
class UStaticMeshComponent;
class AAHCombatPlayerCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHManticoreDriverDelegate, APawn*, Driver);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHManticoreDestroyedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHManticorePresentationChangedDelegate);

UCLASS()
class ASHESOFHEAVEN_API AAHManticoreVehicle : public APawn, public IAHInteractable
{
	GENERATED_BODY()

public:
	AAHManticoreVehicle();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> VehicleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> HullArmor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> TurretAssembly;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> MountedWeapon;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TArray<TObjectPtr<UStaticMeshComponent>> WheelVisuals;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UCameraComponent> VehicleCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Manticore", meta=(ClampMin=1.0))
	float MaxHealth = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Manticore", meta=(ClampMin=100.0))
	float MaxSpeed = 1450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Manticore", meta=(ClampMin=100.0))
	float Acceleration = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Manticore", meta=(ClampMin=0.0))
	float BrakeStrength = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Manticore", meta=(ClampMin=0.0))
	float TurnRate = 74.0f;

	UPROPERTY(BlueprintAssignable, Category="Manticore")
	FAHManticoreDriverDelegate OnDriverEntered;

	UPROPERTY(BlueprintAssignable, Category="Manticore")
	FAHManticoreDriverDelegate OnDriverExited;

	UPROPERTY(BlueprintAssignable, Category="Manticore")
	FAHManticoreDestroyedDelegate OnVehicleDestroyed;

	/** Low-rate presentation event; HUDs never need to poll vehicle simulation every frame. */
	UPROPERTY(BlueprintAssignable, Category="Manticore")
	FAHManticorePresentationChangedDelegate OnPresentationStateChanged;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual float GetInteractionPriority_Implementation() const override;

	bool EnterVehicle(AAHCombatPlayerCharacter* Player);
	void ExitVehicle();
	void FireMountedWeapon();
	void SetMobileThrottle(float Value);
	void SetMobileSteering(float Value);

	UFUNCTION(BlueprintPure, Category="Manticore")
	float GetHealthPercent() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }

	UFUNCTION(BlueprintPure, Category="Manticore")
	float GetSpeed() const { return CurrentSpeed; }

	UFUNCTION(BlueprintPure, Category="Manticore")
	float GetSuspensionCompression() const { return SuspensionCompression; }

	UFUNCTION(BlueprintPure, Category="Manticore")
	bool IsDestroyed() const { return bDestroyed; }

	UFUNCTION(BlueprintPure, Category="Manticore")
	AAHCombatPlayerCharacter* GetDriver() const { return Driver.Get(); }

	FAHVehicleState GetVehicleState() const;
	void RestoreVehicleState(const FAHVehicleState& State);

protected:
	void HandleMoveInput(const struct FInputActionValue& Value);
	void HandleLookInput(const struct FInputActionValue& Value);
	void HandleFireStarted();
	void HandleExitStarted();
	void HandleSeatSwitchStarted();
	void DestroyVehicle();

	TWeakObjectPtr<AAHCombatPlayerCharacter> Driver;
	float Health = 500.0f;
	float CurrentSpeed = 0.0f;
	float Throttle = 0.0f;
	float Steering = 0.0f;
	float SuspensionCompression = 0.0f;
	bool bDestroyed = false;
	float PresentationEventTime = 0.0f;
};
