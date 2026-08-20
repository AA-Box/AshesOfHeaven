#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Weapons/AHGrenadeBase.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/DamageEvents.h"
#include "Sound/SoundBase.h"

UAHCombatComponent::UAHCombatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAHCombatComponent::BeginPlay()
{
	Super::BeginPlay();
	CombatantOwner = Cast<AAHCombatantCharacter>(GetOwner());
}

void UAHCombatComponent::StartFire()
{
	if (!bCombatDisabled && CombatantOwner.IsValid() && CombatantOwner->GetInventoryComponent())
	{
		if (AAHWeaponBase* Weapon = CombatantOwner->GetInventoryComponent()->GetCurrentWeapon())
		{
			Weapon->StartFire();
		}
	}
}

void UAHCombatComponent::StopFire()
{
	if (CombatantOwner.IsValid() && CombatantOwner->GetInventoryComponent())
	{
		if (AAHWeaponBase* Weapon = CombatantOwner->GetInventoryComponent()->GetCurrentWeapon())
		{
			Weapon->StopFire();
		}
	}
}

void UAHCombatComponent::StartADS()
{
	if (!bCombatDisabled)
	{
		bADS = true;
		if (CombatantOwner.IsValid())
		{
			CombatantOwner->SetAimingDownSights(true);
		}
	}
}

void UAHCombatComponent::StopADS()
{
	bADS = false;
	if (CombatantOwner.IsValid())
	{
		CombatantOwner->SetAimingDownSights(false);
	}
}

void UAHCombatComponent::Reload()
{
	if (!bCombatDisabled && CombatantOwner.IsValid() && CombatantOwner->GetInventoryComponent())
	{
		if (AAHWeaponBase* Weapon = CombatantOwner->GetInventoryComponent()->GetCurrentWeapon())
		{
			Weapon->Reload();
		}
	}
}

void UAHCombatComponent::Melee()
{
	if (bCombatDisabled || !CombatantOwner.IsValid() || GetWorld()->GetTimerManager().IsTimerActive(MeleeTimer))
	{
		return;
	}

	GetWorld()->GetTimerManager().SetTimer(MeleeTimer, MeleeCooldown, false);
	if (MeleeSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, MeleeSound, CombatantOwner->GetActorLocation());
	}
	const FVector Start = CombatantOwner->GetWeaponTraceOrigin();
	const FVector End = Start + CombatantOwner->GetControlRotation().Vector() * MeleeRange;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHMelee), true, CombatantOwner.Get());
	FHitResult Hit;
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(MeleeRadius), Params) && Hit.GetActor())
	{
		FPointDamageEvent Event;
		Event.Damage = MeleeDamage;
		Event.HitInfo = Hit;
		Event.ShotDirection = (End - Start).GetSafeNormal();
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), MeleeDamage, Event.ShotDirection, Hit, CombatantOwner->GetController(), CombatantOwner.Get(), nullptr);
		if (ACharacter* HitCharacter = Cast<ACharacter>(Hit.GetActor()))
		{
			HitCharacter->LaunchCharacter(Event.ShotDirection * 180.0f + FVector(0.0f, 0.0f, 80.0f), true, true);
		}
	}
}

void UAHCombatComponent::ThrowGrenade()
{
	if (bCombatDisabled || !CombatantOwner.IsValid() || !CombatantOwner->GetInventoryComponent() || !CombatantOwner->GetInventoryComponent()->ConsumeGrenade())
	{
		return;
	}

	TSubclassOf<AAHGrenadeBase> ClassToSpawn = GrenadeClass;
	if (!ClassToSpawn)
	{
		ClassToSpawn = AAHGrenadeBase::StaticClass();
	}
	const FVector Origin = CombatantOwner->GetWeaponTraceOrigin() + CombatantOwner->GetControlRotation().Vector() * 55.0f;
	FActorSpawnParameters Params;
	Params.Owner = CombatantOwner.Get();
	Params.Instigator = CombatantOwner.Get();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	if (AAHGrenadeBase* Grenade = GetWorld()->SpawnActor<AAHGrenadeBase>(ClassToSpawn, Origin, CombatantOwner->GetControlRotation(), Params))
	{
		Grenade->PrimeAndThrow(CombatantOwner->GetControlRotation().Vector());
	}
}

void UAHCombatComponent::Interact()
{
	if (!bCombatDisabled && CombatantOwner.IsValid() && CombatantOwner->GetInteractionComponent())
	{
		CombatantOwner->GetInteractionComponent()->Interact();
	}
}

void UAHCombatComponent::CycleWeapon(int32 Direction)
{
	if (!bCombatDisabled && CombatantOwner.IsValid() && CombatantOwner->GetInventoryComponent())
	{
		CombatantOwner->GetInventoryComponent()->CycleWeapon(Direction);
	}
}

void UAHCombatComponent::DisableCombat()
{
	bCombatDisabled = true;
	StopFire();
	StopADS();
}
