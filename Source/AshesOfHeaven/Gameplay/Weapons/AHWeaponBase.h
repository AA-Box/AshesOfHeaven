#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "NiagaraSystem.h"
#include "AHWeaponBase.generated.h"

class USceneComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USoundBase;
class AAHCombatantCharacter;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHWeaponAmmoChangedDelegate, const FAHAmmoState&, Ammo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHWeaponEventDelegate);

UCLASS(Blueprintable)
class ASHESOFHEAVEN_API AAHWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	AAHWeaponBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> CapacitorMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FName WeaponId = FName(TEXT("M91_Revenant"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon")
	FText DisplayName = FText::FromString(TEXT("M91 REVENANT"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=1))
	int32 MagazineCapacity = 36;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0))
	int32 ReserveCapacity = 180;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=1.0))
	float RoundsPerMinute = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float BodyDamage = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=1.0))
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float MaxRange = 20000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float FalloffStart = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float FalloffEnd = 16000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float HipSpreadDegrees = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float ADSSpreadDegrees = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float VerticalRecoil = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float HorizontalRecoil = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Weapon", meta=(ClampMin=0.0))
	float ReloadDuration = 1.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> ShotSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> EmptySound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	TObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	TObjectPtr<UNiagaraSystem> TracerEffect;

	UPROPERTY(BlueprintAssignable, Category="Weapon")
	FAHWeaponAmmoChangedDelegate OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category="Weapon")
	FAHWeaponEventDelegate OnShot;

	UPROPERTY(BlueprintAssignable, Category="Weapon")
	FAHWeaponEventDelegate OnReloaded;

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	void SetWeaponActive(bool bActive);
	/** Hides only the local first-person mesh while retaining weapon/gameplay state. */
	void SetLocalPresentationVisible(bool bVisible);
	void StartFire();
	void StopFire();
	void Reload();

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsReloading() const { return bIsReloading; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	bool IsFiring() const { return bWantsToFire; }

	UFUNCTION(BlueprintPure, Category="Weapon")
	const FAHAmmoState& GetAmmoState() const { return Ammo; }

	void SetAmmoState(const FAHAmmoState& SavedAmmo);
	void AddReserveAmmo(int32 Amount);
	AAHCombatantCharacter* GetCombatantOwner() const;

protected:
	/** Where the weapon sits relative to the camera while no hand socket exists (greybox). */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FVector FirstPersonHoldOffset = FVector(45.0f, 24.0f, -28.0f);

	/** SKM_Rifle is authored with its length along Y, so it needs a quarter turn to aim down X. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FRotator FirstPersonHoldRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** Greybox fallback pose used when a third-person hand socket is unavailable. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FVector ThirdPersonHoldOffset = FVector(20.0f, 24.0f, 42.0f);

	/** The greybox rifle is authored along Y and must face the character's forward axis. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FRotator ThirdPersonHoldRotation = FRotator(0.0f, -90.0f, 0.0f);

	/** Keep the prototype rifle readable without occupying the lower-right third of the view. */
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FVector FirstPersonHoldScale = FVector(0.52f);

	USceneComponent* GetFirstPersonHoldParent(AAHCombatantCharacter* Combatant, USkeletalMeshComponent* AttachTarget) const;

	bool bUsingFirstPersonHold = false;
	bool bUsingThirdPersonHold = false;
	bool bLocalPresentationVisible = true;

	FRotator GetRestRotation() const;

	virtual void FireShot();
	void FinishReload();
	float GetDamageAtDistance(float Distance) const;

	FAHAmmoState Ammo;
	FTimerHandle FireTimer;
	FTimerHandle ReloadTimer;
	bool bWantsToFire = false;
	bool bIsReloading = false;
	bool bWeaponActive = false;
};
