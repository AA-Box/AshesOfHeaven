#include "Gameplay/Characters/AHVeilWardenCharacter.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "NavigationSystem.h"

AAHVeilWardenCharacter::AAHVeilWardenCharacter()
{
}

void AAHVeilWardenCharacter::BeginPlay()
{
	Super::BeginPlay();
}

void AAHVeilWardenCharacter::ApplyEnemyDefinition(UAHEnemyDefinition* Definition)
{
	Super::ApplyEnemyDefinition(Definition);
	if (!Definition)
	{
		return;
	}
	if (const float* Value = Definition->Difficulty.AbilityScalars.Find(TEXT("ShieldCycleSeconds"))) ShieldCycleSeconds = *Value;
	if (const float* Value = Definition->Difficulty.AbilityScalars.Find(TEXT("ShieldDamageMultiplier"))) ShieldDamageMultiplier = *Value;
	if (const float* Value = Definition->Difficulty.AbilityScalars.Find(TEXT("TeleportCycleSeconds"))) TeleportCycleSeconds = *Value;
	if (const float* Value = Definition->Difficulty.AbilityScalars.Find(TEXT("TeleportDistance"))) TeleportDistance = *Value;
}

FPrimaryAssetId AAHVeilWardenCharacter::GetDefaultEnemyDefinitionId() const
{
	return AHEnemyAssets::EnemyId(TEXT("Warden"));
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
