#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHCombatComponent.generated.h"

class AAHGrenadeBase;
class AAHCombatantCharacter;
class UAHInventoryComponent;
class USoundBase;

UCLASS(ClassGroup=(AshesOfHeaven), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHCombatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade")
	TSubclassOf<AAHGrenadeBase> GrenadeClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta=(ClampMin=0.1))
	float MeleeCooldown = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta=(ClampMin=0.0))
	float MeleeDamage = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta=(ClampMin=0.0))
	float MeleeRange = 165.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Melee", meta=(ClampMin=0.0))
	float MeleeRadius = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> MeleeSound;

	virtual void BeginPlay() override;

	void StartFire();
	void StopFire();
	void StartADS();
	void StopADS();
	void Reload();
	void Melee();
	void ThrowGrenade();
	void Interact();
	void CycleWeapon(int32 Direction);
	void DisableCombat();

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsCombatDisabled() const { return bCombatDisabled; }

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsAimingDownSights() const { return bADS; }

private:
	TWeakObjectPtr<AAHCombatantCharacter> CombatantOwner;
	FTimerHandle MeleeTimer;
	bool bADS = false;
	bool bCombatDisabled = false;
};
