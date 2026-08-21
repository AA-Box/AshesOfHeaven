#include "Gameplay/Weapons/AHWeaponBase.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
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
	if (USkeletalMesh* RifleMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")))
	{
		WeaponMesh->SetSkeletalMesh(RifleMesh);
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

void AAHWeaponBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (WeaponMesh)
	{
		const FRotator Current = WeaponMesh->GetRelativeRotation();
		WeaponMesh->SetRelativeRotation(FMath::RInterpTo(Current, FRotator::ZeroRotator, DeltaSeconds, 14.0f));
	}
}

void AAHWeaponBase::SetWeaponActive(bool bActive)
{
	bWeaponActive = bActive;
	SetActorHiddenInGame(!bActive);

	if (AAHCombatantCharacter* Combatant = GetCombatantOwner())
	{
		USkeletalMeshComponent* AttachTarget = Combatant->GetFirstPersonMesh();
		if (!Combatant->IsPlayerControlled())
		{
			AttachTarget = Combatant->GetMesh();
			WeaponMesh->SetOnlyOwnerSee(false);
		}
		if (AttachTarget && WeaponMesh->GetAttachParent() != AttachTarget)
		{
			// Greybox characters have no skeletal mesh, so HandGrip_R cannot resolve and the
			// attachment logs a warning on every transform update. Attach socketless until a
			// real mesh provides the socket.
			const FName GripSocket(TEXT("HandGrip_R"));
			const FName AttachSocket = AttachTarget->DoesSocketExist(GripSocket) ? GripSocket : NAME_None;
			WeaponMesh->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale, AttachSocket);
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
		WeaponMesh->SetRelativeRotation(FRotator(-VerticalRecoil * 2.0f, FMath::FRandRange(-HorizontalRecoil, HorizontalRecoil), 0.0f));
	}

	MakeNoise(1.0f, Combatant, Combatant->GetActorLocation(), 3000.0f, Combatant->GetFactionName());
}
