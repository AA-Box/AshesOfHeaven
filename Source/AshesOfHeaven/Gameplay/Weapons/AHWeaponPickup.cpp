#include "Gameplay/Weapons/AHWeaponPickup.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
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

	bool bCollected = false;
	switch (PickupType)
	{
	case EAHResourcePickupType::Weapon:
		if (WeaponClass && Character->GetInventoryComponent()->AddWeaponClass(WeaponClass))
		{
			#if !UE_BUILD_SHIPPING
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Pickup] weapon actor=%s amount=%d"), *GetName(), Amount);
			#endif
			bCollected = true;
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
			bCollected = true;
			Destroy();
		}
		break;
	case EAHResourcePickupType::Grenades:
		Character->GetInventoryComponent()->AddGrenades(Amount);
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Pickup] grenades actor=%s amount=%d"), *GetName(), Amount);
		#endif
		bCollected = true;
		Destroy();
		break;
	default:
		break;
	}

	if (bCollected)
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayWorldCue(EAHAudioCue::Pickup, GetActorLocation(), 0.7f);
		}
	}
}

FText AAHWeaponPickup::GetInteractionPrompt_Implementation() const
{
	switch (PickupType)
	{
	case EAHResourcePickupType::Weapon:
		if (const AAHWeaponBase* WeaponDefaults = WeaponClass ? WeaponClass->GetDefaultObject<AAHWeaponBase>() : nullptr)
		{
			return FText::Format(NSLOCTEXT("AshesOfHeaven", "PickupWeaponPrompt", "E — PICK UP {0}"), WeaponDefaults->DisplayName);
		}
		return FText::GetEmpty();
	case EAHResourcePickupType::Ammo:
		return Amount > 0 ? NSLOCTEXT("AshesOfHeaven", "TakeAmmoPrompt", "E — TAKE AMMO") : FText::GetEmpty();
	case EAHResourcePickupType::Grenades:
		return Amount > 0 ? NSLOCTEXT("AshesOfHeaven", "TakeGrenadePrompt", "E — TAKE FRAG GRENADES") : FText::GetEmpty();
	default:
		return FText::GetEmpty();
	}
}

float AAHWeaponPickup::GetInteractionPriority_Implementation() const
{
	switch (PickupType)
	{
	case EAHResourcePickupType::Weapon:
		return 0.50f;
	case EAHResourcePickupType::Grenades:
		return 0.20f;
	case EAHResourcePickupType::Ammo:
		return 0.10f;
	default:
		return 0.0f;
	}
}
