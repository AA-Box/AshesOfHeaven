#include "Gameplay/Weapons/AHWeaponBase.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AAHWeaponBase::AAHWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
	WeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(RootComponent);
	WeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMesh->SetOnlyOwnerSee(true);
	// Without this the rifle draws at world scale and fills the view. First person primitives
	// use the camera's FirstPersonFieldOfView/FirstPersonScale, which is what makes a held
	// weapon read at the right size.
	WeaponMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	if (USkeletalMesh* RifleMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")))
	{
		WeaponMesh->SetSkeletalMesh(RifleMesh);
	}
	CapacitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MagneticCapacitor"));
	CapacitorMesh->SetupAttachment(WeaponMesh);
	CapacitorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	CapacitorMesh->SetRelativeLocation(FVector(0.0f, -28.0f, -8.0f));
	CapacitorMesh->SetRelativeScale3D(FVector(0.18f, 0.62f, 0.10f));
	CapacitorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapacitorMesh->SetOnlyOwnerSee(true);
	CapacitorMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	if (UMaterialInterface* RifleMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Weapons/Rifle/Materials/M_Rifle.M_Rifle")))
	{
		CapacitorMesh->SetMaterial(0, RifleMaterial);
	}
}

void AAHWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	Ammo.MagazineCapacity = MagazineCapacity;
	Ammo.ReserveCapacity = ReserveCapacity;
	Ammo.Magazine = MagazineCapacity;
	Ammo.Reserve = ReserveCapacity;
	OnAmmoChanged.Broadcast(Ammo);
}

FRotator AAHWeaponBase::GetRestRotation() const
{
	if (bUsingFirstPersonHold)
	{
		return FirstPersonHoldRotation;
	}
	return bUsingThirdPersonHold ? ThirdPersonHoldRotation : FRotator::ZeroRotator;
}

void AAHWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (WeaponMesh)
	{
		// Recoil settles back to the rest pose. With a hand socket that rest pose is zero,
		// because the socket already orients the weapon; without one it is the hold rotation,
		// otherwise this would drag the weapon back to the mesh's own axes every frame.
		const FRotator RestRotation = GetRestRotation();
		const FRotator Current = WeaponMesh->GetRelativeRotation();
		WeaponMesh->SetRelativeRotation(FMath::RInterpTo(Current, RestRotation, DeltaSeconds, 14.0f));
	}
}

USceneComponent* AAHWeaponBase::GetFirstPersonHoldParent(AAHCombatantCharacter* Combatant, USkeletalMeshComponent* AttachTarget) const
{
	// Only the local view needs the offset hold pose; NPCs keep their own mesh as the parent.
	if (Combatant && Combatant->IsPlayerControlled())
	{
		if (USceneComponent* Camera = Combatant->GetFirstPersonCameraComponent())
		{
			return Camera;
		}
	}
	return AttachTarget;
}

void AAHWeaponBase::SetWeaponActive(bool bActive)
{
	bWeaponActive = bActive;
	SetActorHiddenInGame(!bActive);

	if (AAHCombatantCharacter* Combatant = GetCombatantOwner())
	{
		const bool bIsPlayer = Combatant->IsPlayerControlled();
		USkeletalMeshComponent* AttachTarget = bIsPlayer ? Combatant->GetFirstPersonMesh() : Combatant->GetMesh();
		WeaponMesh->SetOnlyOwnerSee(bIsPlayer);
		WeaponMesh->FirstPersonPrimitiveType = bIsPlayer ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::WorldSpaceRepresentation;
		if (CapacitorMesh)
		{
			CapacitorMesh->SetOnlyOwnerSee(bIsPlayer);
			CapacitorMesh->FirstPersonPrimitiveType = bIsPlayer ? EFirstPersonPrimitiveType::FirstPerson : EFirstPersonPrimitiveType::WorldSpaceRepresentation;
		}
		if (!bIsPlayer)
		{
			bUsingFirstPersonHold = false;
		}
		const FName GripSocket(TEXT("HandGrip_R"));
		if (AttachTarget && AttachTarget->DoesSocketExist(GripSocket))
		{
			bUsingFirstPersonHold = false;
			bUsingThirdPersonHold = false;
			if (WeaponMesh->GetAttachParent() != AttachTarget)
			{
				WeaponMesh->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale, GripSocket);
			}
			WeaponMesh->SetRelativeLocation(FVector::ZeroVector);
			WeaponMesh->SetRelativeRotation(FRotator::ZeroRotator);
			WeaponMesh->SetRelativeScale3D(FVector::OneVector);
		}
		else if (AttachTarget)
		{
			// Greybox characters have no skeletal mesh, so HandGrip_R does not exist. Snapping
			// to the meshless component drops the weapon on the character's origin, where it
			// fills the view, and re-resolving the missing socket logs a warning every frame.
			// Hang it off the camera at a hold offset instead so the weapon reads as carried.
			// Once a real mesh supplies the socket this branch stops being taken.
			USceneComponent* FallbackParent = GetFirstPersonHoldParent(Combatant, AttachTarget);
			if (WeaponMesh->GetAttachParent() != FallbackParent)
			{
				WeaponMesh->AttachToComponent(FallbackParent, FAttachmentTransformRules::SnapToTargetNotIncludingScale, NAME_None);
			}
			bUsingFirstPersonHold = FallbackParent != AttachTarget;
			bUsingThirdPersonHold = !bUsingFirstPersonHold;
			WeaponMesh->SetRelativeLocation(bUsingFirstPersonHold ? FirstPersonHoldOffset : ThirdPersonHoldOffset);
			WeaponMesh->SetRelativeRotation(bUsingFirstPersonHold ? FirstPersonHoldRotation : ThirdPersonHoldRotation);
			WeaponMesh->SetRelativeScale3D(bUsingFirstPersonHold ? FirstPersonHoldScale : FVector::OneVector);
		}
	}
}

void AAHWeaponBase::StartFire()
{
	if (!bWeaponActive || bIsReloading || bWantsToFire)
	{
		return;
	}

	bWantsToFire = true;
	FireShot();
	if (RoundsPerMinute > 0.0f)
	{
		GetWorld()->GetTimerManager().SetTimer(FireTimer, this, &AAHWeaponBase::FireShot, 60.0f / RoundsPerMinute, true);
	}
}

void AAHWeaponBase::StopFire()
{
	bWantsToFire = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(FireTimer);
	}
}

void AAHWeaponBase::Reload()
{
	if (!bWeaponActive || bIsReloading || Ammo.Magazine >= Ammo.MagazineCapacity || Ammo.Reserve <= 0)
	{
		return;
	}

	StopFire();
	bIsReloading = true;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Weapon] reload_start weapon=%s ammo=%d/%d"), *GetName(), Ammo.Magazine, Ammo.Reserve);
	#endif
	if (ReloadSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSound, GetActorLocation());
	}
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &AAHWeaponBase::FinishReload, ReloadDuration, false);
}

void AAHWeaponBase::FinishReload()
{
	const int32 Transfer = UAHCombatRulesLibrary::CalculateReloadTransfer(Ammo);
	Ammo.Magazine += Transfer;
	Ammo.Reserve -= Transfer;
	bIsReloading = false;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Weapon] reload_complete weapon=%s ammo=%d/%d"), *GetName(), Ammo.Magazine, Ammo.Reserve);
	#endif
	OnAmmoChanged.Broadcast(Ammo);
	OnReloaded.Broadcast();
}

void AAHWeaponBase::SetAmmoState(const FAHAmmoState& SavedAmmo)
{
	Ammo = SavedAmmo;
	Ammo.MagazineCapacity = MagazineCapacity;
	Ammo.ReserveCapacity = ReserveCapacity;
	Ammo.Magazine = FMath::Clamp(Ammo.Magazine, 0, MagazineCapacity);
	Ammo.Reserve = FMath::Clamp(Ammo.Reserve, 0, ReserveCapacity);
	bIsReloading = false;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Weapon] ammo_restore weapon=%s ammo=%d/%d"), *GetName(), Ammo.Magazine, Ammo.Reserve);
	#endif
	OnAmmoChanged.Broadcast(Ammo);
}

void AAHWeaponBase::AddReserveAmmo(int32 Amount)
{
	Ammo.Reserve = FMath::Clamp(Ammo.Reserve + Amount, 0, Ammo.ReserveCapacity);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Weapon] ammo_pickup weapon=%s delta=%d reserve=%d"), *GetName(), Amount, Ammo.Reserve);
	#endif
	OnAmmoChanged.Broadcast(Ammo);
}

AAHCombatantCharacter* AAHWeaponBase::GetCombatantOwner() const
{
	return Cast<AAHCombatantCharacter>(GetOwner());
}

float AAHWeaponBase::GetDamageAtDistance(float Distance) const
{
	if (FalloffEnd <= FalloffStart || Distance <= FalloffStart)
	{
		return BodyDamage;
	}
	return BodyDamage * FMath::Lerp(1.0f, 0.55f, FMath::Clamp((Distance - FalloffStart) / (FalloffEnd - FalloffStart), 0.0f, 1.0f));
}

void AAHWeaponBase::FireShot()
{
	if (!bWantsToFire || !bWeaponActive || bIsReloading)
	{
		return;
	}

	AAHCombatantCharacter* Combatant = GetCombatantOwner();
	if (!Combatant)
	{
		return;
	}

	if (Ammo.Magazine <= 0)
	{
		StopFire();
		if (EmptySound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, EmptySound, GetActorLocation());
		}
		return;
	}

	const FVector TraceStart = Combatant->GetWeaponTraceOrigin();
	FRotator AimRotation = (Combatant->GetWeaponTargetLocation() - TraceStart).Rotation();
	const float Spread = Combatant->IsAimingDownSights() ? ADSSpreadDegrees : HipSpreadDegrees;
	AimRotation.Yaw += FMath::FRandRange(-Spread, Spread);
	AimRotation.Pitch += FMath::FRandRange(-Spread, Spread);
	const FVector TraceEnd = TraceStart + (AimRotation.Vector() * MaxRange);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHRevenantShot), true, Combatant);
	Params.AddIgnoredActor(this);
	FHitResult Hit;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params);
	if (bHit && Hit.GetActor())
	{
		const float Damage = GetDamageAtDistance(FVector::Distance(TraceStart, Hit.ImpactPoint));
		UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, AimRotation.Vector(), Hit, Combatant->GetController(), this, nullptr);
		if (ImpactSound)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, Hit.ImpactPoint);
		}
		if (ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation());
		}
	}
	if (TracerEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, TracerEffect, TraceStart, AimRotation);
	}
	if (MuzzleFlashEffect && WeaponMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(MuzzleFlashEffect, WeaponMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
	}

	--Ammo.Magazine;
	OnAmmoChanged.Broadcast(Ammo);
	OnShot.Broadcast();
	Combatant->ApplyCameraRecoil(VerticalRecoil, HorizontalRecoil);
	Combatant->NotifyWeaponShot(Hit, bHit);

	if (ShotSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ShotSound, GetActorLocation());
	}
	if (WeaponMesh)
	{
		// Kick on top of the rest pose, in the parent's frame, so it reads as muzzle rise no
		// matter how the mesh's own axes sit. Setting the kick as the relative rotation would
		// throw the hold rotation away and snap the weapon back to the mesh's axes each shot.
		const FRotator Kick(-VerticalRecoil * 2.0f, FMath::FRandRange(-HorizontalRecoil, HorizontalRecoil), 0.0f);
		WeaponMesh->SetRelativeRotation(FQuat(Kick) * FQuat(GetRestRotation()));
	}

	MakeNoise(1.0f, Combatant, Combatant->GetActorLocation(), 3000.0f, Combatant->GetFactionName());
}
