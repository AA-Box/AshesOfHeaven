#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Enemies/AHEncounterDefinition.h"
#include "Gameplay/Enemies/AHEnemyAssetSubsystem.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"

AAHCombatEncounter::AAHCombatEncounter()
{
	PrimaryActorTick.bCanEverTick = false;
	ActivationVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationVolume"));
	RootComponent = ActivationVolume;
	ActivationVolume->SetBoxExtent(FVector(500.0f, 800.0f, 200.0f));
	ActivationVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivationVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivationVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EncounterDefinitionId = AHEnemyAssets::EncounterId(TEXT("PilgrimPatrol"));
}

void AAHCombatEncounter::BeginPlay()
{
	Super::BeginPlay();
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (Checkpoints->IsEncounterCompleted(EncounterId))
		{
			bComplete = true;
			#if !UE_BUILD_SHIPPING
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Encounter] restore_complete id=%s"), *EncounterId.ToString());
			#endif
			return;
		}
	}
	if (bActivateOnPlayerOverlap)
	{
		ActivationVolume->OnComponentBeginOverlap.AddDynamic(this, &AAHCombatEncounter::OnActivationOverlap);
	}
	if (bPreloadOnBeginPlay)
	{
		PreloadEncounterAssets();
	}
}

void AAHCombatEncounter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseAssetLease();
	Super::EndPlay(EndPlayReason);
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
	if (bActive || bComplete || !GetWorld())
	{
		return;
	}
	bActivationRequested = true;
	if (!bPreloadStarted)
	{
		PreloadEncounterAssets();
	}
	if (!bAssetsReady)
	{
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets][Encounter] activation_waiting id=%s definition=%s"),
			*EncounterId.ToString(), *EncounterDefinitionId.ToString());
		#endif
		return;
	}
	SpawnLoadedEnemies();
}

void AAHCombatEncounter::PreloadEncounterAssets()
{
	if (bComplete || bPreloadStarted || !GetGameInstance())
	{
		return;
	}
	UAHEnemyAssetSubsystem* Assets = GetGameInstance()->GetSubsystem<UAHEnemyAssetSubsystem>();
	if (!Assets)
	{
		return;
	}
	bPreloadStarted = true;
	AssetLease = Assets->PreloadEncounterAssets(
		EncounterDefinitionId,
		EncounterId.IsNone() ? GetFName() : EncounterId,
		FAHEnemyAssetsReady::CreateUObject(this, &ThisClass::HandleAssetsReady));
}

void AAHCombatEncounter::HandleAssetsReady(
	FGuid RequestId,
	bool bSuccess,
	const TArray<UAHEnemyDefinition*>& Definitions,
	const FString& Error)
{
	if (RequestId != AssetLease)
	{
		return;
	}
	if (!bSuccess)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Assets][Encounter] preload_failed id=%s error=%s"), *EncounterId.ToString(), *Error);
		AssetLease.Invalidate();
		bPreloadStarted = false;
		bAssetsReady = false;
		return;
	}
	LoadedEnemyDefinitions.Reset();
	for (UAHEnemyDefinition* Definition : Definitions)
	{
		LoadedEnemyDefinitions.Add(Definition);
	}
	bAssetsReady = !LoadedEnemyDefinitions.IsEmpty();
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets][Encounter] preload_ready id=%s enemies=%d"),
		*EncounterId.ToString(), LoadedEnemyDefinitions.Num());
	#endif
	if (bActivationRequested && bAssetsReady)
	{
		SpawnLoadedEnemies();
	}
}

void AAHCombatEncounter::SpawnLoadedEnemies()
{
	if (bActive || bComplete || !bAssetsReady || !GetWorld())
	{
		return;
	}
	const UAHEncounterDefinition* EncounterDefinition = Cast<UAHEncounterDefinition>(
		UAssetManager::Get().GetPrimaryAssetObject(EncounterDefinitionId));
	if (!EncounterDefinition)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Assets][Encounter] resident definition missing id=%s"), *EncounterDefinitionId.ToString());
		return;
	}
	TArray<FPrimaryAssetId> SpawnSequence;
	EncounterDefinition->BuildSpawnSequence(EnemyCount, SpawnSequence);
	TMap<FPrimaryAssetId, UAHEnemyDefinition*> DefinitionsById;
	for (UAHEnemyDefinition* Definition : LoadedEnemyDefinitions)
	{
		if (Definition) DefinitionsById.Add(Definition->GetPrimaryAssetId(), Definition);
	}

	bActive = true;
	bActivationRequested = false;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Encounter] start id=%s requested_enemies=%d"), *EncounterId.ToString(), EnemyCount);
	#endif
	for (int32 Index = 0; Index < SpawnSequence.Num(); ++Index)
	{
		if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
		{
			if (!Platform->TryRegisterActiveCombatant())
			{
				break;
			}
		}

		UAHEnemyDefinition* EnemyDefinition = DefinitionsById.FindRef(SpawnSequence[Index]);
		UClass* SpawnClass = EnemyDefinition ? EnemyDefinition->CombatClass.Get() : nullptr;
		if (!SpawnClass)
		{
			if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
			{
				Platform->UnregisterActiveCombatant();
			}
			continue;
		}

		const FVector SpawnLocation = SpawnLocations.IsValidIndex(Index) ? SpawnLocations[Index] : GetActorLocation() + FVector(Index * 160.0f, (Index % 2 == 0 ? 1.0f : -1.0f) * 450.0f, 100.0f);
		const FTransform SpawnTransform(GetActorRotation(), SpawnLocation);
		AAHCombatantCharacter* Enemy = GetWorld()->SpawnActorDeferred<AAHCombatantCharacter>(
			SpawnClass, SpawnTransform, this, nullptr,
			ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
		if (Enemy)
		{
			Enemy->ApplyEnemyDefinition(EnemyDefinition);
			UGameplayStatics::FinishSpawningActor(Enemy, SpawnTransform);
			ActiveEnemies.Add(Enemy);
			Enemy->OnCombatantDeath.AddDynamic(this, &AAHCombatEncounter::OnEnemyDied);
			Enemy->OnDestroyed.AddDynamic(this, &AAHCombatEncounter::OnEnemyDestroyed);
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

void AAHCombatEncounter::ReleaseAssetLease()
{
	if (AssetLease.IsValid() && GetGameInstance())
	{
		if (UAHEnemyAssetSubsystem* Assets = GetGameInstance()->GetSubsystem<UAHEnemyAssetSubsystem>())
		{
			Assets->ReleaseEncounterAssets(AssetLease);
		}
	}
	AssetLease.Invalidate();
	LoadedEnemyDefinitions.Reset();
	bAssetsReady = false;
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

void AAHCombatEncounter::OnEnemyDestroyed(AActor* DestroyedActor)
{
	for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(ActiveEnemies[Index]) || ActiveEnemies[Index].Get() == DestroyedActor)
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
	ReleaseAssetLease();
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Encounter] complete id=%s"), *EncounterId.ToString());
	#endif
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
