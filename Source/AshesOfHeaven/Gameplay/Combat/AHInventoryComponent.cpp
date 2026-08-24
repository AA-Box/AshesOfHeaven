#include "Gameplay/Combat/AHInventoryComponent.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAHInventoryComponent::UAHInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAHInventoryComponent::BeginPlay()
{
	Super::BeginPlay();
	Grenades = FMath::Clamp(StartingGrenades, 0, MaximumGrenades);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Inventory] begin grenades=%d"), Grenades);
	#endif
}

void UAHInventoryComponent::AddWeapon(AAHWeaponBase* Weapon)
{
	if (!IsValid(Weapon))
	{
		return;
	}

	Weapon->SetOwner(GetOwner());
	Weapons.AddUnique(Weapon);
	if (CurrentWeaponIndex == INDEX_NONE)
	{
		CurrentWeaponIndex = 0;
	}

	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		Weapons[Index]->SetActorHiddenInGame(Index != CurrentWeaponIndex);
	}
	OnInventoryChanged.Broadcast();
}

void UAHInventoryComponent::DiscardWeapon(AAHWeaponBase* Weapon)
{
	const int32 Index = Weapons.IndexOfByKey(Weapon);
	if (Index == INDEX_NONE)
	{
		return;
	}

	Weapons.RemoveAt(Index);
	if (Weapon)
	{
		Weapon->SetWeaponActive(false);
	}
	// Keep the equipped index pointing at the same weapon it did before the removal shifted the array.
	if (CurrentWeaponIndex == Index)
	{
		CurrentWeaponIndex = INDEX_NONE;
		if (Weapons.Num() > 0)
		{
			EquipWeapon(0);
		}
	}
	else if (CurrentWeaponIndex > Index)
	{
		--CurrentWeaponIndex;
	}
	OnInventoryChanged.Broadcast();
}

void UAHInventoryComponent::DestroyWeaponsForCorpseCleanup()
{
	for (AAHWeaponBase* Weapon : Weapons)
	{
		if (!IsValid(Weapon))
		{
			continue;
		}
		Weapon->StopFire();
		if (UWorld* World = Weapon->GetWorld())
		{
			World->GetTimerManager().ClearAllTimersForObject(Weapon);
		}
		Weapon->OnAmmoChanged.Clear();
		Weapon->OnShot.Clear();
		Weapon->OnReloaded.Clear();
		Weapon->Destroy();
	}
	Weapons.Reset();
	CurrentWeaponIndex = INDEX_NONE;
	OnInventoryChanged.Clear();
}

AAHWeaponBase* UAHInventoryComponent::AddWeaponClass(TSubclassOf<AAHWeaponBase> WeaponClass, bool bEquipImmediately)
{
	if (!GetWorld() || !WeaponClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = Cast<APawn>(GetOwner());
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAHWeaponBase* Weapon = GetWorld()->SpawnActor<AAHWeaponBase>(WeaponClass, GetOwner()->GetActorTransform(), SpawnParams);
	if (Weapon)
	{
		const int32 NewIndex = Weapons.Num();
		AddWeapon(Weapon);
		if (bEquipImmediately)
		{
			EquipWeapon(NewIndex);
		}
	}
	return Weapon;
}

void UAHInventoryComponent::EquipWeapon(int32 Index)
{
	if (!Weapons.IsValidIndex(Index))
	{
		return;
	}

	CurrentWeaponIndex = Index;
	for (int32 WeaponIndex = 0; WeaponIndex < Weapons.Num(); ++WeaponIndex)
	{
		if (IsValid(Weapons[WeaponIndex]))
		{
			Weapons[WeaponIndex]->SetActorHiddenInGame(WeaponIndex != CurrentWeaponIndex);
			Weapons[WeaponIndex]->SetWeaponActive(WeaponIndex == CurrentWeaponIndex);
		}
	}
	OnInventoryChanged.Broadcast();
}

void UAHInventoryComponent::CycleWeapon(int32 Direction)
{
	if (Weapons.Num() < 2)
	{
		return;
	}

	const int32 NewIndex = (CurrentWeaponIndex + Direction + Weapons.Num()) % Weapons.Num();
	EquipWeapon(NewIndex);
}

AAHWeaponBase* UAHInventoryComponent::GetCurrentWeapon() const
{
	return Weapons.IsValidIndex(CurrentWeaponIndex) ? Weapons[CurrentWeaponIndex] : nullptr;
}

bool UAHInventoryComponent::ConsumeGrenade()
{
	if (Grenades <= 0)
	{
		return false;
	}

	--Grenades;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Inventory] grenade_consume remaining=%d"), Grenades);
	#endif
	OnInventoryChanged.Broadcast();
	return true;
}

void UAHInventoryComponent::AddGrenades(int32 Amount)
{
	Grenades = FMath::Clamp(Grenades + Amount, 0, MaximumGrenades);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Inventory] grenade_delta=%d total=%d"), Amount, Grenades);
	#endif
	OnInventoryChanged.Broadcast();
}

void UAHInventoryComponent::SetSavedAmmo(const FAHAmmoState& Ammo)
{
	if (AAHWeaponBase* Weapon = GetCurrentWeapon())
	{
		Weapon->SetAmmoState(Ammo);
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Inventory] ammo_restore magazine=%d reserve=%d"), Ammo.Magazine, Ammo.Reserve);
		#endif
	}
}

FAHAmmoState UAHInventoryComponent::GetSavedAmmo() const
{
	return GetCurrentWeapon() ? GetCurrentWeapon()->GetAmmoState() : FAHAmmoState();
}
