#include "Gameplay/Characters/AHVeilWardenCharacter.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"

AAHVeilWardenCharacter::AAHVeilWardenCharacter()
{
	Faction = EAHFaction::Veil;
	HealthComponent->MaxHealth = 220.0f;
	ArmorComponent->MaxArmor = 110.0f;
	bDestroyOnDeath = true;
	AIControllerClass = AAHCombatAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->MaxWalkSpeed = 220.0f;
	WeaponClass = AAHWeaponBase::StaticClass();
	HeadshotMultiplier = 1.5f;
}

void AAHVeilWardenCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (WeaponClass && InventoryComponent && InventoryComponent->GetWeapons().IsEmpty())
	{
		InventoryComponent->AddWeaponClass(WeaponClass);
	}
}

void AAHVeilWardenCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	ShieldTimer += DeltaSeconds;
	TeleportTimer += DeltaSeconds;
	if (ShieldTimer >= ShieldCycleSeconds)
	{
		ShieldTimer = 0.0f;
		bShieldActive = !bShieldActive;
	}
	if (TeleportTimer >= TeleportCycleSeconds && GetWorld())
	{
		TeleportTimer = 0.0f;
		if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
		{
			const FVector ToPlayer = PlayerPawn->GetActorLocation() - GetActorLocation();
			if (ToPlayer.SizeSquared() > FMath::Square(900.0f))
			{
				const FVector DesiredLocation = PlayerPawn->GetActorLocation() - ToPlayer.GetSafeNormal() * TeleportDistance;
				FNavLocation ProjectedLocation;
				UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
				if (Navigation && Navigation->ProjectPointToNavigation(DesiredLocation, ProjectedLocation, FVector(500.0f, 500.0f, 300.0f)))
				{
					SetActorLocation(ProjectedLocation.Location, false, nullptr, ETeleportType::TeleportPhysics);
				}
				else
				{
					SetActorLocation(DesiredLocation, false, nullptr, ETeleportType::TeleportPhysics);
				}
			}
		}
	}
}

float AAHVeilWardenCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return Super::TakeDamage(bShieldActive ? DamageAmount * ShieldDamageMultiplier : DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}
