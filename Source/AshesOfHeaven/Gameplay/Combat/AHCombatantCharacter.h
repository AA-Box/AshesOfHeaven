#pragma once

#include "CoreMinimal.h"
#include "AshesOfHeavenCharacter.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "AHCombatantCharacter.generated.h"

class UAHHealthComponent;
class UAHArmorComponent;
class UAHCombatComponent;
class UAHInteractionComponent;
class UAHInventoryComponent;
class USoundBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FAHCombatDamageFeedbackDelegate, float, Damage, bool, bHeadshot, bool, bArmorHit, bool, bArmorBroken, float, DirectionAngle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHCombatantDeathDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FAHWeaponShotNativeDelegate, const FHitResult&, bool);

UCLASS(Abstract)
class ASHESOFHEAVEN_API AAHCombatantCharacter : public AAshesOfHeavenCharacter
{
	GENERATED_BODY()

public:
	AAHCombatantCharacter();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHArmorComponent> ArmorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHInventoryComponent> InventoryComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	EAHFaction Faction = EAHFaction::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin=1.0))
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	bool bDestroyOnDeath = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> HurtSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> ArmorDamageSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FAHCombatDamageFeedbackDelegate OnDamageFeedback;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FAHCombatantDeathDelegate OnCombatantDeath;

	FAHWeaponShotNativeDelegate OnWeaponShot;

	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetFaction(EAHFaction NewFaction) { Faction = NewFaction; }

	UFUNCTION(BlueprintPure, Category="Combat")
	EAHFaction GetFaction() const { return Faction; }

	FName GetFactionName() const;
	bool IsHostileTo(const AAHCombatantCharacter* Other) const;

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsCombatantDead() const;

	FVector GetWeaponTraceOrigin() const;
	virtual FVector GetWeaponTargetLocation() const;
	void SetCombatTarget(AActor* NewTarget);
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

	bool IsAimingDownSights() const { return bAimingDownSights; }
	void SetAimingDownSights(bool bNewAiming);
	void ApplyCameraRecoil(float Vertical, float Horizontal);
	void NotifyWeaponShot(const FHitResult& Hit, bool bHit);

	virtual void OnDeathStarted();

	UAHHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UAHArmorComponent* GetArmorComponent() const { return ArmorComponent; }
	UAHCombatComponent* GetCombatComponent() const { return CombatComponent; }
	UAHInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	UAHInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

protected:
	void EnsureGreyboxBody();


	UFUNCTION()
	void HandleHealthDeath();

	UFUNCTION()
	void HandleArmorBroken();

	TWeakObjectPtr<AActor> CombatTarget;
	bool bAimingDownSights = false;
};
