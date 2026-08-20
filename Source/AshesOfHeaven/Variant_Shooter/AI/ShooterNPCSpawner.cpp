// Copyright Epic Games, Inc. All Rights Reserved.


#include "Variant_Shooter/AI/ShooterNPCSpawner.h"
#include "Engine/World.h"
#include "Components/SceneComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/ArrowComponent.h"
#include "TimerManager.h"
#include "ShooterNPC.h"
#include "Kismet/GameplayStatics.h"
#include "ShooterGameMode.h"
#include "Platform/AHPlatformManagerSubsystem.h"

// Sets default values
AShooterNPCSpawner::AShooterNPCSpawner()
{
	PrimaryActorTick.bCanEverTick = false;

	// create the root
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// create the reference spawn capsule
	SpawnCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Spawn Capsule"));
	SpawnCapsule->SetupAttachment(RootComponent);

	SpawnCapsule->SetRelativeLocation(FVector(0.0f, 0.0f, 90.0f));
	SpawnCapsule->SetCapsuleSize(35.0f, 90.0f);
	SpawnCapsule->SetCollisionProfileName(FName("NoCollision"));

	SpawnDirection = CreateDefaultSubobject<UArrowComponent>(TEXT("Spawn Direction"));
	SpawnDirection->SetupAttachment(RootComponent);
}

void AShooterNPCSpawner::BeginPlay()
{
	Super::BeginPlay();
	
	// ignore if enemies are disabled at the GameMode level
	if (AShooterGameMode* GM = Cast<AShooterGameMode>(UGameplayStatics::GetGameMode(GetWorld())))
	{
		if (!GM->ShouldSpawnEnemyNPCs())
		{
			return;
		}
	}

	// ensure we don't spawn NPCs if our initial spawn count is zero
	if (SpawnCount > 0)
	{
		// schedule the first NPC spawn
		GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, InitialSpawnDelay);
	}
}

void AShooterNPCSpawner::EndPlay(EEndPlayReason::Type EndPlayReason)
{
	// clear the spawn timer
	GetWorld()->GetTimerManager().ClearTimer(SpawnTimer);

	if (bRegisteredActiveCombatant)
	{
		if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
		{
			PlatformManager->UnregisterActiveCombatant();
		}
		bRegisteredActiveCombatant = false;
	}

	Super::EndPlay(EndPlayReason);
}

void AShooterNPCSpawner::SpawnNPC()
{
	if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
	{
		if (!PlatformManager->TryRegisterActiveCombatant())
		{
			GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, 0.5f);
			return;
		}
		bRegisteredActiveCombatant = true;
	}

	// ensure the NPC class is valid
	if (IsValid(NPCClass))
	{
		// spawn the NPC at the reference capsule's transform
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AShooterNPC* SpawnedNPC = GetWorld()->SpawnActor<AShooterNPC>(NPCClass, SpawnCapsule->GetComponentTransform(), SpawnParams);

		// was the NPC successfully created?
		if (SpawnedNPC)
		{
			ActiveNPC = SpawnedNPC;
			// subscribe to the death delegate
			SpawnedNPC->OnPawnDeath.AddDynamic(this, &AShooterNPCSpawner::OnNPCDied);
			return;
		}
	}

	if (bRegisteredActiveCombatant)
	{
		if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
		{
			PlatformManager->UnregisterActiveCombatant();
		}
		bRegisteredActiveCombatant = false;
	}
}

void AShooterNPCSpawner::OnNPCDied()
{
	ActiveNPC = nullptr;
	if (bRegisteredActiveCombatant)
	{
		if (UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this))
		{
			PlatformManager->UnregisterActiveCombatant();
		}
		bRegisteredActiveCombatant = false;
	}

	// decrease the spawn counter
	--SpawnCount;

	// is this the last NPC we should spawn?
	if (SpawnCount <= 0)
	{
		return;
	}

	// schedule the next NPC spawn
	GetWorld()->GetTimerManager().SetTimer(SpawnTimer, this, &AShooterNPCSpawner::SpawnNPC, RespawnDelay);
}
