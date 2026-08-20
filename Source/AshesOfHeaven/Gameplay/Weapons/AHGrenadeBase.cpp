#include "Gameplay/Weapons/AHGrenadeBase.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Components/SphereComponent.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

AAHGrenadeBase::AAHGrenadeBase()
{
	CollisionComponent->SetSphereRadius(12.0f);
	ProjectileMovement->InitialSpeed = 1050.0f;
	ProjectileMovement->MaxSpeed = 1050.0f;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->Bounciness = 0.38f;
	ProjectileMovement->Friction = 0.25f;
	ProjectileMovement->ProjectileGravityScale = 1.15f;
	LifeSeconds = 12.0f;
}

void AAHGrenadeBase::BeginPlay()
{
	Super::BeginPlay();
	ProjectileMovement->OnProjectileStop.AddDynamic(this, &AAHGrenadeBase::OnProjectileStopped);
}

void AAHGrenadeBase::PrimeAndThrow(const FVector& Direction)
{
	bPrimed = true;
	ProjectileMovement->Velocity = Direction.GetSafeNormal() * ProjectileMovement->InitialSpeed;
	GetWorld()->GetTimerManager().SetTimer(FuseTimer, this, &AAHGrenadeBase::Explode, FuseSeconds, false);
}

void AAHGrenadeBase::OnProjectileStopped(const FHitResult& ImpactResult)
{
	if (bPrimed && !GetWorld()->GetTimerManager().IsTimerActive(FuseTimer))
	{
		GetWorld()->GetTimerManager().SetTimer(FuseTimer, this, &AAHGrenadeBase::Explode, 0.1f, false);
	}
}

void AAHGrenadeBase::Explode()
{
	if (!IsValid(this))
	{
		return;
	}
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExplosionSound, GetActorLocation());
	}
	if (ExplosionEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ExplosionEffect, GetActorLocation());
	}

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		MaximumExplosionDamage,
		MinimumExplosionDamage,
		GetActorLocation(),
		ExplosionRadius * 0.18f,
		ExplosionRadius,
		1.25f,
		nullptr,
		TArray<AActor*>(),
		this,
		GetInstigatorController(),
		ECC_Visibility);
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		this,
		MaximumExplosionDamage,
		MinimumExplosionDamage,
		GetActorLocation(),
		ExplosionRadius * 0.18f,
		ExplosionRadius,
		1.25f,
		nullptr,
		TArray<AActor*>(),
		this,
		GetInstigatorController(),
		ECC_Pawn);

	TArray<AActor*> NearbyActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AAHCombatantCharacter::StaticClass(), NearbyActors);
	for (AActor* Actor : NearbyActors)
	{
		if (AAHCombatantCharacter* Character = Cast<AAHCombatantCharacter>(Actor))
		{
			if (AAHCombatAIController* AI = Cast<AAHCombatAIController>(Character->GetController()))
			{
				AI->ReactToGrenade(GetActorLocation(), ExplosionRadius);
			}
			if (FVector::DistSquared(Character->GetActorLocation(), GetActorLocation()) <= FMath::Square(ExplosionRadius))
			{
				Character->LaunchCharacter((Character->GetActorLocation() - GetActorLocation()).GetSafeNormal() * ExplosionImpulse + FVector(0.0f, 0.0f, 100.0f), true, true);
			}
		}
	}

	Destroy();
}
