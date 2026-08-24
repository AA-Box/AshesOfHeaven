#pragma once

#include "CoreMinimal.h"
#include "AshesOfHeavenCharacter.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "AHCombatantCharacter.generated.h"

class UAHHealthComponent;
class UAHArmorComponent;
class UAHCombatComponent;
class UAHInteractionComponent;
class UAHInventoryComponent;
class AAHWeaponBase;
class USoundBase;
class UMaterialInterface;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FAHCombatDamageFeedbackDelegate, float, Damage, bool, bHeadshot, bool, bArmorHit, bool, bArmorBroken, float, DirectionAngle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHCombatantDeathDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FAHWeaponShotNativeDelegate, const FHitResult&, bool);

UCLASS(Abstract)
class ASHESOFHEAVEN_API AAHCombatantCharacter : public AAshesOfHeavenCharacter, public IAHInteractable
{
	GENERATED_BODY()

public:
	AAHCombatantCharacter();

	/** A body can be stripped for the weapon it was still holding. Live combatants offer nothing. */
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual float GetInteractionPriority_Implementation() const override;

	UFUNCTION(BlueprintPure, Category="Combat")
	AAHWeaponBase* GetLootableWeapon() const;

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

	/** Reads the combatant out of the dark without lighting the street.
	 * Erebus has no ground-level fill and auto-exposure keys off the bright sky, so a soldier
	 * standing in it is a flat black cut-out against the fog - no face, no armour, no read on
	 * which way they are facing. This light is restricted to lighting channel 1, which only
	 * character bodies are on, so it shapes them without touching the environment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPointLightComponent> BodyFillLight;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(ClampMin=0.0))
	float BodyFillIntensity = 260.0f;

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

	/** Extra cone this combatant's shots scatter into, on top of the weapon's own spread.
	 * Zero for the player; AI controllers set it from their Accuracy so a squad is dangerous
	 * without being a wall of instant hitscan. */
	float GetAimSpreadPenaltyDegrees() const { return AimSpreadPenaltyDegrees; }
	void SetAimSpreadPenaltyDegrees(float Degrees) { AimSpreadPenaltyDegrees = FMath::Max(0.0f, Degrees); }
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
	/** Per-slot faction skin, applied in BeginPlay once the subclass has set Faction.
	 * These are lit character materials on purpose: the environment materials that were here
	 * before have a near-black base colour, and against Erebus's bright fog a combatant wearing
	 * one is a flat silhouette with no face, no armour and no readable pose. */
	UPROPERTY(EditDefaultsOnly, Category="Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> HumanBodyMaterials {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_01_New.MI_Manny_01_New"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.MI_Manny_02_New")))
	};

	UPROPERTY(EditDefaultsOnly, Category="Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> VeilBodyMaterials {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_01.MI_Quinn_01"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_02.MI_Quinn_02")))
	};

	/** Seconds a ragdolled corpse stays in the world before cleanup. Long enough to read as a kill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin=0.0))
	float CorpseLifeSpan = 30.0f;

	void ApplyFactionAppearance();
	void StartRagdoll();

	UFUNCTION()
	void HandleHealthDeath();

	UFUNCTION()
	void HandleArmorBroken();

	TWeakObjectPtr<AActor> CombatTarget;
	bool bAimingDownSights = false;
	float AimSpreadPenaltyDegrees = 0.0f;
};
