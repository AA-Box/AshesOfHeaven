#include "Gameplay/Weapons/AHWeaponPickup.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Components/StaticMeshComponent.h"

AAHWeaponPickup::AAHWeaponPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
	RootComponent = PickupMesh;
	PickupMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PickupMesh->SetCollisionResponseToAllChannels(ECR_Block);
	WeaponClass = AAHWeaponBase::StaticClass();
}

void AAHWeaponPickup::Interact_Implementation(AActor* Interactor)
{
	AAHCombatantCharacter* Character = Cast<AAHCombatantCharacter>(Interactor);
	if (!Character || !Character->GetInventoryComponent())
	{
		return;
	}

	switch (PickupType)
	{
	case EAHResourcePickupType::Weapon:
		if (WeaponClass && Character->GetInventoryComponent()->AddWeaponClass(WeaponClass))
		{
			#if !UE_BUILD_SHIPPING
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Pickup] weapon actor=%s amount=%d"), *GetName(), Amount);
			#endif
			Destroy();
		}
		break;
	case EAHResourcePickupType::Ammo:
		if (AAHWeaponBase* Weapon = Character->GetInventoryComponent()->GetCurrentWeapon())
		{
			Weapon->AddReserveAmmo(Amount);
			#if !UE_BUILD_SHIPPING
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Pickup] ammo actor=%s amount=%d"), *GetName(), Amount);
			#endif
			Destroy();
		}
		break;
	case EAHResourcePickupType::Grenades:
		Character->GetInventoryComponent()->AddGrenades(Amount);
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Pickup] grenades actor=%s amount=%d"), *GetName(), Amount);
		#endif
		Destroy();
		break;
	default:
		break;
	}
}

FText AAHWeaponPickup::GetInteractionPrompt_Implementation() const
{
	switch (PickupType)
	{
	case EAHResourcePickupType::Weapon:
		return FText::FromString(TEXT("INTERACT  M91 REVENANT"));
	case EAHResourcePickupType::Ammo:
		return FText::FromString(TEXT("INTERACT  AMMUNITION"));
	case EAHResourcePickupType::Grenades:
		return FText::FromString(TEXT("INTERACT  FRAG GRENADES"));
	default:
		return FText::FromString(TEXT("INTERACT"));
	}
}
