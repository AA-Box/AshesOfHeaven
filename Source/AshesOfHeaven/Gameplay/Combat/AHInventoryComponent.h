#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHGameplayTypes.h"
#include "AHInventoryComponent.generated.h"

class AAHWeaponBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHInventoryChangedDelegate);

UCLASS(ClassGroup=(AshesOfHeaven), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHInventoryComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin=0, ClampMax=8))
	int32 StartingGrenades = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Inventory", meta=(ClampMin=0, ClampMax=8))
	int32 MaximumGrenades = 4;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FAHInventoryChangedDelegate OnInventoryChanged;

	virtual void BeginPlay() override;

	void AddWeapon(AAHWeaponBase* Weapon);
	AAHWeaponBase* AddWeaponClass(TSubclassOf<AAHWeaponBase> WeaponClass, bool bEquipImmediately = true);
	void EquipWeapon(int32 Index);
	void CycleWeapon(int32 Direction);

	UFUNCTION(BlueprintPure, Category="Inventory")
	AAHWeaponBase* GetCurrentWeapon() const;

	const TArray<TObjectPtr<AAHWeaponBase>>& GetWeapons() const { return Weapons; }

	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetGrenades() const { return Grenades; }

	bool ConsumeGrenade();
	void AddGrenades(int32 Amount);

	void SetSavedAmmo(const FAHAmmoState& Ammo);
	FAHAmmoState GetSavedAmmo() const;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<AAHWeaponBase>> Weapons;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	int32 CurrentWeaponIndex = INDEX_NONE;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Inventory", meta=(AllowPrivateAccess="true"))
	int32 Grenades = 0;
};
