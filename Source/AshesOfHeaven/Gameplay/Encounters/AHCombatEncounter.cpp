#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

AAHCombatEncounter::AAHCombatEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	ActivationVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationVolume"));
	RootComponent = ActivationVolume;
	ActivationVolume->SetBoxExtent(FVector(500.0f, 800.0f, 200.0f));
	ActivationVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivationVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivationVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EnemyClass = AAHVeilPilgrimCharacter::StaticClass();
}

void AAHCombatEncounter::BeginPlay()
{
	Super::BeginPlay();
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (Checkpoints->IsEncounterCompleted(EncounterId))
		{
			bComplete = true;
			return;
		}
	}
	if (bActivateOnPlayerOverlap)
	{
		ActivationVolume->OnComponentBeginOverlap.AddDynamic(this, &AAHCombatEncounter::OnActivationOverlap);
	}
}

void AAHCombatEncounter::OnActivationOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<AAHCombatPlayerCharacter>(OtherActor))
	{
		ActivateEncounter();
	}
}

void AAHCombatEncounter::ActivateEncounter()
{
	if (bActive || bComplete || !EnemyClass || !GetWorld())
	{
		return;
	}

	bActive = true;
	for (int32 Index = 0; Index < EnemyCount; ++Index)
	{
		if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
		{
			if (!Platform->TryRegisterActiveCombatant())
			{
				break;
			}
		}

		const FVector SpawnLocation = SpawnLocations.IsValidIndex(Index) ? SpawnLocations[Index] : GetActorLocation() + FVector(Index * 160.0f, (Index % 2 == 0 ? 1.0f : -1.0f) * 450.0f, 100.0f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		AAHCombatantCharacter* Enemy = GetWorld()->SpawnActor<AAHCombatantCharacter>(EnemyClass, SpawnLocation, GetActorRotation(), Params);
		if (Enemy)
		{
			ActiveEnemies.Add(Enemy);
			Enemy->OnCombatantDeath.AddDynamic(this, &AAHCombatEncounter::OnEnemyDied);
		}
		else if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
		{
			Platform->UnregisterActiveCombatant();
		}
	}

	if (ActiveEnemies.IsEmpty())
	{
		CompleteEncounter();
	}
}

void AAHCombatEncounter::OnEnemyDied()
{
	for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(ActiveEnemies[Index]) || ActiveEnemies[Index]->IsCombatantDead())
		{
			ActiveEnemies.RemoveAt(Index);
			if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
			{
				Platform->UnregisterActiveCombatant();
			}
		}
	}

	if (bActive && ActiveEnemies.IsEmpty())
	{
		CompleteEncounter();
	}
}

void AAHCombatEncounter::CompleteEncounter()
{
	if (bComplete)
	{
		return;
	}

	bComplete = true;
	bActive = false;
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		Checkpoints->MarkEncounterCompleted(EncounterId);
	}
	if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
	{
		if (ObjectiveOnComplete != NAME_None)
		{
			Objectives->CompleteObjective(ObjectiveOnComplete);
		}
	}
	OnEncounterComplete.Broadcast(EncounterId);
}
