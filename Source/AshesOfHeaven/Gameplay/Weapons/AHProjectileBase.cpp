#include "Gameplay/Weapons/AHProjectileBase.h"
#include "Gameplay/Pooling/AHObjectPoolSubsystem.h"
#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

AAHProjectileBase::AAHProjectileBase()
{
	PrimaryActorTick.bCanEverTick = false;
	CollisionComponent = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	CollisionComponent->InitSphereRadius(6.0f);
	CollisionComponent->SetCollisionProfileName(TEXT("Projectile"));
	RootComponent = CollisionComponent;
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComponent;
	ProjectileMovement->InitialSpeed = 3000.0f;
	ProjectileMovement->MaxSpeed = 3000.0f;
	TrailComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Trail"));
	TrailComponent->SetupAttachment(CollisionComponent);
	TrailComponent->SetAutoActivate(false);
	TrailComponent->SetAutoDestroy(false);
	bReplicates = true;
}

void AAHProjectileBase::BeginPlay()
{
	Super::BeginPlay();
	RestoreLifecycleDefaults();
	InitializeProjectile(GetActorForwardVector(), Damage);
}

void AAHProjectileBase::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	}
	CollisionComponent->OnComponentHit.RemoveDynamic(this, &AAHProjectileBase::OnProjectileHit);
	if (TrailComponent)
	{
		TrailComponent->DeactivateImmediate();
	}
	Super::EndPlay(EndPlayReason);
}

AAHProjectileBase* AAHProjectileBase::SpawnProjectile(
	const UObject* WorldContextObject,
	TSubclassOf<AAHProjectileBase> ProjectileClass,
	const FTransform& SpawnTransform,
	AActor* ProjectileOwner,
	APawn* ProjectileInstigator,
	const FVector& Direction,
	float ShotDamage)
{
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	if (!ProjectileClass)
	{
		ProjectileClass = StaticClass();
	}

	FAHObjectPoolAcquireContext Context;
	Context.Transform = SpawnTransform;
	Context.Owner = ProjectileOwner;
	Context.Instigator = ProjectileInstigator;
	AAHProjectileBase* Projectile = nullptr;
	if (UAHObjectPoolSubsystem* Pool = World->GetSubsystem<UAHObjectPoolSubsystem>())
	{
		Projectile = Pool->AcquireActor<AAHProjectileBase>(ProjectileClass, Context);
	}
	else
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = ProjectileOwner;
		SpawnParameters.Instigator = ProjectileInstigator;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Projectile = World->SpawnActor<AAHProjectileBase>(ProjectileClass, SpawnTransform, SpawnParameters);
	}
	if (Projectile)
	{
		Projectile->InitializeProjectile(Direction, ShotDamage);
	}
	return Projectile;
}

bool AAHProjectileBase::CanBePooled_Implementation() const
{
	// Any derived native or Blueprint class may add timers, delegates, or VFX state. It must
	// explicitly override this after implementing its own complete reset contract.
	return GetClass() == StaticClass();
}

void AAHProjectileBase::OnAcquireFromPool_Implementation(const FAHObjectPoolAcquireContext& Context)
{
	RestoreLifecycleDefaults();
	InitializeProjectile(Context.Transform.GetRotation().GetForwardVector(), Damage);
}

void AAHProjectileBase::OnReleaseToPool_Implementation()
{
	bLifecycleActive = false;
	bHasImpacted = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
	}
	ExpiryTimer.Invalidate();

	OnDestroyed.Clear();
	OnEndPlay.Clear();
	OnActorBeginOverlap.Clear();
	OnActorEndOverlap.Clear();
	OnActorHit.Clear();
	OnBeginCursorOver.Clear();
	OnEndCursorOver.Clear();
	OnClicked.Clear();
	OnReleased.Clear();
	OnInputTouchBegin.Clear();
	OnInputTouchEnd.Clear();
	OnInputTouchEnter.Clear();
	OnInputTouchLeave.Clear();
	OnTakeAnyDamage.Clear();
	OnTakePointDamage.Clear();
	OnTakeRadialDamage.Clear();
	CollisionComponent->OnComponentHit.Clear();
	CollisionComponent->OnComponentBeginOverlap.Clear();
	CollisionComponent->OnComponentEndOverlap.Clear();
	CollisionComponent->OnComponentWake.Clear();
	CollisionComponent->OnComponentSleep.Clear();
	CollisionComponent->OnBeginCursorOver.Clear();
	CollisionComponent->OnEndCursorOver.Clear();
	CollisionComponent->OnClicked.Clear();
	CollisionComponent->OnReleased.Clear();
	CollisionComponent->OnInputTouchBegin.Clear();
	CollisionComponent->OnInputTouchEnd.Clear();
	CollisionComponent->OnInputTouchEnter.Clear();
	CollisionComponent->OnInputTouchLeave.Clear();
	CollisionComponent->OnTouchBegan.Clear();
	CollisionComponent->OnTouchEnded.Clear();
	CollisionComponent->OnTouchEntered.Clear();
	CollisionComponent->OnTouchExited.Clear();
	CollisionComponent->ClearMoveIgnoreActors();
	CollisionComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->OnProjectileBounce.Clear();
	ProjectileMovement->OnProjectileStop.Clear();
	ProjectileMovement->HomingTargetComponent = nullptr;
	ProjectileMovement->SetInterpolatedComponent(nullptr);
	ProjectileMovement->ResetInterpolation();
	ProjectileMovement->Deactivate();
	ProjectileMovement->SetComponentTickEnabled(false);
	ProjectileMovement->SetUpdatedComponent(nullptr);

	if (TrailComponent)
	{
		TrailComponent->OnSystemFinished.Clear();
		TrailComponent->DeactivateImmediate();
		TrailComponent->ResetSystem();
		TrailComponent->GetOverrideParameters().Empty();
		TrailComponent->SetAsset(nullptr);
	}
	Damage = 0.0f;
	LifeSeconds = 0.0f;
	TrailEffect = nullptr;
	SetLifeSpan(0.0f);
}

void AAHProjectileBase::RestoreLifecycleDefaults()
{
	const AAHProjectileBase* Defaults = GetClass()->GetDefaultObject<AAHProjectileBase>();
	if (!Defaults)
	{
		return;
	}
	Damage = Defaults->Damage;
	LifeSeconds = Defaults->LifeSeconds;
	TrailEffect = Defaults->TrailEffect;
	bHasImpacted = false;

	CollisionComponent->SetSphereRadius(Defaults->CollisionComponent->GetUnscaledSphereRadius());
	CollisionComponent->SetCollisionProfileName(Defaults->CollisionComponent->GetCollisionProfileName());
	CollisionComponent->SetGenerateOverlapEvents(Defaults->CollisionComponent->GetGenerateOverlapEvents());
	CollisionComponent->SetCollisionEnabled(Defaults->CollisionComponent->GetCollisionEnabled());
	CollisionComponent->ClearMoveIgnoreActors();
	if (GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(GetOwner(), true);
	}
	if (GetInstigator() && GetInstigator() != GetOwner())
	{
		CollisionComponent->IgnoreActorWhenMoving(GetInstigator(), true);
	}
	BindHitDelegate();

	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->InitialSpeed = Defaults->ProjectileMovement->InitialSpeed;
	ProjectileMovement->MaxSpeed = Defaults->ProjectileMovement->MaxSpeed;
	ProjectileMovement->bRotationFollowsVelocity = Defaults->ProjectileMovement->bRotationFollowsVelocity;
	ProjectileMovement->bRotationRemainsVertical = Defaults->ProjectileMovement->bRotationRemainsVertical;
	ProjectileMovement->bShouldBounce = Defaults->ProjectileMovement->bShouldBounce;
	ProjectileMovement->bInitialVelocityInLocalSpace = Defaults->ProjectileMovement->bInitialVelocityInLocalSpace;
	ProjectileMovement->bForceSubStepping = Defaults->ProjectileMovement->bForceSubStepping;
	ProjectileMovement->bSimulationEnabled = Defaults->ProjectileMovement->bSimulationEnabled;
	ProjectileMovement->bSweepCollision = Defaults->ProjectileMovement->bSweepCollision;
	ProjectileMovement->bIsHomingProjectile = Defaults->ProjectileMovement->bIsHomingProjectile;
	ProjectileMovement->bBounceAngleAffectsFriction = Defaults->ProjectileMovement->bBounceAngleAffectsFriction;
	ProjectileMovement->bSimulationUseScopedMovement = Defaults->ProjectileMovement->bSimulationUseScopedMovement;
	ProjectileMovement->bInterpolationUseScopedMovement = Defaults->ProjectileMovement->bInterpolationUseScopedMovement;
	ProjectileMovement->ProjectileGravityScale = Defaults->ProjectileMovement->ProjectileGravityScale;
	ProjectileMovement->Buoyancy = Defaults->ProjectileMovement->Buoyancy;
	ProjectileMovement->Bounciness = Defaults->ProjectileMovement->Bounciness;
	ProjectileMovement->Friction = Defaults->ProjectileMovement->Friction;
	ProjectileMovement->BounceVelocityStopSimulatingThreshold = Defaults->ProjectileMovement->BounceVelocityStopSimulatingThreshold;
	ProjectileMovement->MinFrictionFraction = Defaults->ProjectileMovement->MinFrictionFraction;
	ProjectileMovement->HomingAccelerationMagnitude = Defaults->ProjectileMovement->HomingAccelerationMagnitude;
	ProjectileMovement->MaxSimulationTimeStep = Defaults->ProjectileMovement->MaxSimulationTimeStep;
	ProjectileMovement->MaxSimulationIterations = Defaults->ProjectileMovement->MaxSimulationIterations;
	ProjectileMovement->BounceAdditionalIterations = Defaults->ProjectileMovement->BounceAdditionalIterations;
	ProjectileMovement->HomingTargetComponent = nullptr;
	ProjectileMovement->SetInterpolatedComponent(nullptr);
	ProjectileMovement->ResetInterpolation();
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->SetComponentTickEnabled(true);
	ProjectileMovement->Activate(true);
}

void AAHProjectileBase::BindHitDelegate()
{
	CollisionComponent->OnComponentHit.RemoveDynamic(this, &AAHProjectileBase::OnProjectileHit);
	CollisionComponent->OnComponentHit.AddDynamic(this, &AAHProjectileBase::OnProjectileHit);
}

void AAHProjectileBase::InitializeProjectile(const FVector& Direction, float ShotDamage)
{
	bLifecycleActive = true;
	bHasImpacted = false;
	Damage = FMath::Max(0.0f, ShotDamage);
	const FVector LaunchDirection = Direction.IsNearlyZero() ? GetActorForwardVector() : Direction.GetSafeNormal();
	ProjectileMovement->SetUpdatedComponent(CollisionComponent);
	ProjectileMovement->Velocity = LaunchDirection * ProjectileMovement->InitialSpeed;
	ProjectileMovement->SetComponentTickEnabled(true);
	ProjectileMovement->Activate(true);
	RestartTrail();
	ScheduleExpiry();
	SetActorEnableCollision(true);
}

void AAHProjectileBase::RestartTrail()
{
	if (!TrailComponent)
	{
		return;
	}
	TrailComponent->DeactivateImmediate();
	TrailComponent->ResetSystem();
	TrailComponent->SetAsset(TrailEffect, true);
	if (TrailEffect)
	{
		TrailComponent->Activate(true);
	}
}

void AAHProjectileBase::ScheduleExpiry()
{
	SetLifeSpan(0.0f);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(ExpiryTimer);
		if (LifeSeconds > 0.0f)
		{
			GetWorld()->GetTimerManager().SetTimer(ExpiryTimer, this, &AAHProjectileBase::ExpireProjectile, LifeSeconds, false);
		}
	}
}

void AAHProjectileBase::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bLifecycleActive || bHasImpacted || OtherActor == GetOwner() || OtherActor == GetInstigator())
	{
		return;
	}
	bHasImpacted = true;
	bLifecycleActive = false;
	if (OtherActor)
	{
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, GetVelocity().GetSafeNormal(), Hit, GetInstigatorController(), this, nullptr);
	}
	ReleaseOrDestroy();
}

void AAHProjectileBase::ExpireProjectile()
{
	if (!bLifecycleActive)
	{
		return;
	}
	bLifecycleActive = false;
	ReleaseOrDestroy();
}

void AAHProjectileBase::ReleaseOrDestroy()
{
	if (UAHObjectPoolSubsystem* Pool = GetWorld() ? GetWorld()->GetSubsystem<UAHObjectPoolSubsystem>() : nullptr)
	{
		if (Pool->ReleaseActor(this))
		{
			return;
		}
	}
	Destroy();
}

#if WITH_DEV_AUTOMATION_TESTS
void AAHProjectileBase::TestSimulateImpact(AActor* OtherActor, const FHitResult& Hit)
{
	OnProjectileHit(CollisionComponent, OtherActor, nullptr, FVector::ZeroVector, Hit);
}

bool AAHProjectileBase::IsHitDelegateBoundForTest() const
{
	return CollisionComponent->OnComponentHit.IsAlreadyBound(this, &AAHProjectileBase::OnProjectileHit);
}

bool AAHProjectileBase::IsExpiryTimerActiveForTest() const
{
	return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(ExpiryTimer);
}
#endif
