#include "Gameplay/Weapons/AHWeaponBase.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "EngineUtils.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
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
	CapacitorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MagneticCapacitor"));
	CapacitorMesh->SetupAttachment(WeaponMesh);
	CapacitorMesh->SetRelativeLocation(FVector(0.0f, -28.0f, -8.0f));
	CapacitorMesh->SetRelativeScale3D(FVector(0.18f, 0.62f, 0.10f));
	CapacitorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CapacitorMesh->SetOnlyOwnerSee(true);
	CapacitorMesh->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
}

namespace
{
	/** Every hostile AI inside Radius of a shot goes to look. Mirrors the grenade notify: a
	 *  direct call rather than a hearing sense, because nothing here listens for stimuli. */
	void NotifyNearbyAIOfShot(AAHCombatantCharacter* Shooter, float Radius)
	{
		if (!Shooter || !Shooter->GetWorld())
		{
			return;
		}
		const FVector ShotLocation = Shooter->GetActorLocation();
		const float RadiusSquared = FMath::Square(Radius);
		for (TActorIterator<AAHCombatantCharacter> It(Shooter->GetWorld()); It; ++It)
		{
			AAHCombatantCharacter* Listener = *It;
			if (!Listener || Listener == Shooter || Listener->IsCombatantDead())
			{
				continue;
			}
			if (FVector::DistSquared(Listener->GetActorLocation(), ShotLocation) > RadiusSquared)
			{
				continue;
			}
			if (AAHCombatAIController* AI = Cast<AAHCombatAIController>(Listener->GetController()))
			{
				AI->ReactToGunshot(ShotLocation, Shooter);
			}
		}
	}
}

void AAHWeaponBase::BeginPlay()
{
	Super::BeginPlay();
	if (!bHasStreamedLoadout)
	{
		RequestLegacyPresentationAsync();
	}
	Ammo.MagazineCapacity = MagazineCapacity;
	Ammo.ReserveCapacity = ReserveCapacity;
	Ammo.Magazine = MagazineCapacity;
	Ammo.Reserve = ReserveCapacity;
	OnAmmoChanged.Broadcast(Ammo);
}

void AAHWeaponBase::ApplyStreamedLoadout(const FAHEnemyLoadout& Loadout)
{
	bHasStreamedLoadout = true;
	if (LegacyPresentationHandle.IsValid() && !LegacyPresentationHandle->HasLoadCompleted())
	{
		LegacyPresentationHandle->CancelHandle();
	}
	LegacyPresentationHandle.Reset();
	if (WeaponMesh)
	{
		if (USkeletalMesh* Mesh = Loadout.WeaponMesh.Get()) WeaponMesh->SetSkeletalMesh(Mesh);
		if (UMaterialInterface* Material = Loadout.WeaponMaterial.Get())
		{
			for (int32 Index = 0; Index < WeaponMesh->GetNumMaterials(); ++Index) WeaponMesh->SetMaterial(Index, Material);
		}
	}
	if (CapacitorMesh)
	{
		if (UStaticMesh* Mesh = Loadout.CapacitorMesh.Get()) CapacitorMesh->SetStaticMesh(Mesh);
		if (UMaterialInterface* Material = Loadout.CapacitorMaterial.Get()) CapacitorMesh->SetMaterial(0, Material);
	}
	ShotSound = Loadout.ShotSound.Get();
	ReloadSound = Loadout.ReloadSound.Get();
	EmptySound = Loadout.EmptySound.Get();
	ImpactSound = Loadout.ImpactSound.Get();
	MuzzleFlashEffect = Loadout.MuzzleFlashEffect.Get();
	ImpactEffect = Loadout.ImpactEffect.Get();
}

void AAHWeaponBase::RequestLegacyPresentationAsync()
{
	const TArray<FSoftObjectPath> Paths {
		FSoftObjectPath(TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")),
		FSoftObjectPath(TEXT("/Game/Ashes/Materials/Instances/MI_HumanMetal_Dark.MI_HumanMetal_Dark")),
		FSoftObjectPath(TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube")),
		FSoftObjectPath(TEXT("/Game/Weapons/Rifle/Materials/M_Rifle.M_Rifle")),
		FSoftObjectPath(TEXT("/Game/Ashes/Audio/Weapons/M91/SC_M91_Fire.SC_M91_Fire"))
	};
	LegacyPresentationHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleLegacyPresentationLoaded));
}

void AAHWeaponBase::HandleLegacyPresentationLoaded()
{
	if (bHasStreamedLoadout)
	{
		return;
	}
	if (WeaponMesh)
	{
		WeaponMesh->SetSkeletalMesh(Cast<USkeletalMesh>(FSoftObjectPath(
			TEXT("/Game/Weapons/Rifle/Meshes/SKM_Rifle.SKM_Rifle")).ResolveObject()));
		if (UMaterialInterface* Material = Cast<UMaterialInterface>(FSoftObjectPath(
			TEXT("/Game/Ashes/Materials/Instances/MI_HumanMetal_Dark.MI_HumanMetal_Dark")).ResolveObject()))
		{
			for (int32 Index = 0; Index < WeaponMesh->GetNumMaterials(); ++Index) WeaponMesh->SetMaterial(Index, Material);
		}
	}
	if (CapacitorMesh)
	{
		CapacitorMesh->SetStaticMesh(Cast<UStaticMesh>(FSoftObjectPath(
			TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube")).ResolveObject()));
		CapacitorMesh->SetMaterial(0, Cast<UMaterialInterface>(FSoftObjectPath(
			TEXT("/Game/Weapons/Rifle/Materials/M_Rifle.M_Rifle")).ResolveObject()));
	}
	ShotSound = Cast<USoundBase>(FSoftObjectPath(
		TEXT("/Game/Ashes/Audio/Weapons/M91/SC_M91_Fire.SC_M91_Fire")).ResolveObject());
	LegacyPresentationHandle.Reset();
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
		if (bIsPlayer)
		{
			WeaponMesh->SetVisibility(bActive && bLocalPresentationVisible);
			if (CapacitorMesh)
			{
				CapacitorMesh->SetVisibility(bActive && bLocalPresentationVisible);
			}
		}
		else
		{
			WeaponMesh->SetVisibility(bActive);
			if (CapacitorMesh)
			{
				CapacitorMesh->SetVisibility(bActive);
			}
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

void AAHWeaponBase::SetLocalPresentationVisible(bool bVisible)
{
	bLocalPresentationVisible = bVisible;
	AAHCombatantCharacter* Combatant = GetCombatantOwner();
	if (!Combatant || !Combatant->IsPlayerControlled())
	{
		return;
	}
	const bool bVisibleNow = bWeaponActive && bVisible;
	if (WeaponMesh)
	{
		WeaponMesh->SetVisibility(bVisibleNow);
	}
	if (CapacitorMesh)
	{
		CapacitorMesh->SetVisibility(bVisibleNow);
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
	else if (GetWorld())
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayWorldCue(EAHAudioCue::Reload, GetActorLocation(), 0.75f);
		}
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
		else if (GetWorld())
		{
			if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
			{
				Audio->PlayWorldCue(EAHAudioCue::Empty, GetActorLocation(), 0.8f);
			}
		}
		return;
	}

	const FVector TraceStart = Combatant->GetWeaponTraceOrigin();
	FRotator AimRotation = (Combatant->GetWeaponTargetLocation() - TraceStart).Rotation();
	// The shooter's own aim error rides on top of the weapon's mechanical spread.
	const float Spread = (Combatant->IsAimingDownSights() ? ADSSpreadDegrees : HipSpreadDegrees)
		+ Combatant->GetAimSpreadPenaltyDegrees();
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
		else if (GetWorld())
		{
			if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
			{
				Audio->PlayWorldCue(EAHAudioCue::Impact, Hit.ImpactPoint, 0.65f);
			}
		}
		if (ImpactEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, ImpactEffect, Hit.ImpactPoint, Hit.ImpactNormal.Rotation(), FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
		}
	}
	if (TracerEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this, TracerEffect, TraceStart, AimRotation, FVector::OneVector, true, true, ENCPoolMethod::AutoRelease);
	}
	if (MuzzleFlashEffect && WeaponMesh)
	{
		UNiagaraFunctionLibrary::SpawnSystemAttached(
			MuzzleFlashEffect, WeaponMesh, NAME_None, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true, true, ENCPoolMethod::AutoRelease);
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
	else if (GetWorld())
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayWorldCue(EAHAudioCue::Shot, GetActorLocation(), 0.9f);
		}
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
	// MakeNoise has no listener in this project - no AI configures a hearing sense - so the
	// stimulus was emitted and dropped on every shot. Notify directly, the same way
	// AAHGrenadeBase notifies ReactToGrenade, so firing from concealment actually costs the
	// player something.
	NotifyNearbyAIOfShot(Combatant, 3000.0f);
}
