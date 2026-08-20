#include "Gameplay/Level/AHErebusCombatSliceDirector.h"
#include "Gameplay/Characters/AHHumanSoldierCharacter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointActor.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Objectives/AHObjectiveZone.h"
#include "Gameplay/Weapons/AHWeaponPickup.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AAHErebusCombatSliceDirector::AAHErebusCombatSliceDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	BlockMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/M_PrototypeGrid.M_PrototypeGrid"));
}

void AAHErebusCombatSliceDirector::BeginPlay()
{
	Super::BeginPlay();
	BuildBlockout();
	BuildMissionActors();
	if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
	{
		Objectives->OnObjectiveChanged.AddDynamic(this, &AAHErebusCombatSliceDirector::HandleObjectiveChanged);
	}
}

void AAHErebusCombatSliceDirector::BuildBlockout()
{
	// The route is deliberately readable: trench → ruined passage → courtyard → interior → gate.
	SpawnBlock(FVector(6000.0f, 0.0f, -100.0f), FVector(150.0f, 24.0f, 1.0f));
	SpawnBlock(FVector(800.0f, -850.0f, 140.0f), FVector(5.0f, 0.55f, 1.4f));
	SpawnBlock(FVector(800.0f, 850.0f, 140.0f), FVector(5.0f, 0.55f, 1.4f));
	SpawnBlock(FVector(2500.0f, -650.0f, 180.0f), FVector(5.0f, 0.35f, 1.8f));
	SpawnBlock(FVector(2500.0f, 650.0f, 180.0f), FVector(5.0f, 0.35f, 1.8f));
	SpawnBlock(FVector(3500.0f, 0.0f, 250.0f), FVector(1.0f, 3.0f, 2.5f));
	SpawnBlock(FVector(5900.0f, -950.0f, 270.0f), FVector(1.6f, 1.2f, 2.7f));
	SpawnBlock(FVector(6650.0f, 950.0f, 270.0f), FVector(1.6f, 1.2f, 2.7f));
	SpawnBlock(FVector(7350.0f, -450.0f, 120.0f), FVector(1.0f, 2.1f, 1.2f));
	SpawnBlock(FVector(7350.0f, 450.0f, 120.0f), FVector(1.0f, 2.1f, 1.2f));
	SpawnBlock(FVector(8450.0f, -480.0f, 170.0f), FVector(3.0f, 0.3f, 1.7f));
	SpawnBlock(FVector(8450.0f, 480.0f, 170.0f), FVector(3.0f, 0.3f, 1.7f));
	SpawnBlock(FVector(9150.0f, 0.0f, 360.0f), FVector(0.5f, 5.0f, 3.6f));
	SpawnBlock(FVector(10300.0f, -800.0f, 150.0f), FVector(2.0f, 0.35f, 1.5f));
	SpawnBlock(FVector(10300.0f, 800.0f, 150.0f), FVector(2.0f, 0.35f, 1.5f));
	SpawnBlock(FVector(11300.0f, 0.0f, 350.0f), FVector(0.45f, 6.0f, 3.5f));
	SpawnBlock(FVector(12700.0f, -650.0f, 110.0f), FVector(1.0f, 1.5f, 1.1f));
	SpawnBlock(FVector(12700.0f, 650.0f, 110.0f), FVector(1.0f, 1.5f, 1.1f));
}

void AAHErebusCombatSliceDirector::SpawnBlock(const FVector& Location, const FVector& Scale, const FRotator& Rotation)
{
	if (!GetWorld() || !BlockMesh)
	{
		return;
	}
	AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Location, Scale));
	if (Block && Block->GetStaticMeshComponent())
	{
		Block->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		if (BlockMaterial)
		{
			Block->GetStaticMeshComponent()->SetMaterial(0, BlockMaterial);
		}
	}
}

void AAHErebusCombatSliceDirector::BuildMissionActors()
{
	SpawnPickup(FVector(160.0f, 0.0f, 110.0f), EAHResourcePickupType::Weapon);
	SpawnPickup(FVector(1300.0f, -300.0f, 90.0f), EAHResourcePickupType::Ammo, 72);
	SpawnPickup(FVector(4300.0f, 300.0f, 90.0f), EAHResourcePickupType::Grenades, 2);
	SpawnPickup(FVector(7600.0f, 240.0f, 90.0f), EAHResourcePickupType::Ammo, 108);

	SpawnObjectiveZone(FVector(1050.0f, 0.0f, 110.0f), FVector(260.0f, 900.0f, 160.0f), FName(TEXT("ReachDefensivePosition")));
	SpawnObjectiveZone(FVector(4250.0f, 0.0f, 110.0f), FVector(220.0f, 900.0f, 160.0f), FName(TEXT("AdvanceThroughBreach")));
	SpawnObjectiveZone(FVector(13100.0f, 0.0f, 110.0f), FVector(320.0f, 600.0f, 160.0f), FName(TEXT("ReachExtraction")));

	FirstEncounter = SpawnEncounter(FName(TEXT("Encounter_One")), FVector(1900.0f, 0.0f, 100.0f), 4, FName(TEXT("EliminateVeilAssault")),
		{FVector(2300.0f, -480.0f, 100.0f), FVector(2600.0f, 480.0f, 100.0f), FVector(2950.0f, -520.0f, 100.0f), FVector(3150.0f, 500.0f, 100.0f)});
	SecondEncounter = SpawnEncounter(FName(TEXT("Encounter_Two")), FVector(5200.0f, 0.0f, 100.0f), 6, NAME_None,
		{FVector(5700.0f, -850.0f, 100.0f), FVector(6200.0f, 850.0f, 100.0f), FVector(6750.0f, -650.0f, 100.0f), FVector(7100.0f, 650.0f, 100.0f), FVector(7500.0f, -800.0f, 100.0f), FVector(7900.0f, 800.0f, 100.0f)});
	FinalEncounter = SpawnEncounter(FName(TEXT("Encounter_FinalDefense")), FVector(10400.0f, 0.0f, 100.0f), 5, FName(TEXT("DefendEvacuationGate")),
		{FVector(9700.0f, -750.0f, 100.0f), FVector(9800.0f, 750.0f, 100.0f), FVector(10400.0f, -600.0f, 100.0f), FVector(10600.0f, 600.0f, 100.0f), FVector(11000.0f, 0.0f, 100.0f)});

	if (FirstEncounter) FirstEncounter->OnEncounterComplete.AddDynamic(this, &AAHErebusCombatSliceDirector::HandleEncounterComplete);
	if (SecondEncounter) SecondEncounter->OnEncounterComplete.AddDynamic(this, &AAHErebusCombatSliceDirector::HandleEncounterComplete);
	if (FinalEncounter) FinalEncounter->OnEncounterComplete.AddDynamic(this, &AAHErebusCombatSliceDirector::HandleEncounterComplete);

	SpawnCheckpoint(FVector(-500.0f, 0.0f, 100.0f), FName(TEXT("Checkpoint_1")));
	SpawnCheckpoint(FVector(3650.0f, 0.0f, 100.0f), FName(TEXT("Checkpoint_2")));
	SpawnCheckpoint(FVector(9000.0f, 0.0f, 100.0f), FName(TEXT("Checkpoint_3")));
	SpawnFriendly(FVector(9300.0f, -300.0f, 100.0f));
	SpawnFriendly(FVector(9300.0f, 300.0f, 100.0f));
}

AAHCombatEncounter* AAHErebusCombatSliceDirector::SpawnEncounter(FName Id, const FVector& Location, int32 Count, FName ObjectiveOnComplete, const TArray<FVector>& Spawns)
{
	AAHCombatEncounter* Encounter = GetWorld()->SpawnActor<AAHCombatEncounter>(AAHCombatEncounter::StaticClass(), Location, FRotator::ZeroRotator);
	if (Encounter)
	{
		Encounter->EncounterId = Id;
		Encounter->EnemyCount = Count;
		Encounter->ObjectiveOnComplete = ObjectiveOnComplete;
		Encounter->SpawnLocations = Spawns;
	}
	return Encounter;
}

void AAHErebusCombatSliceDirector::SpawnPickup(const FVector& Location, EAHResourcePickupType Type, int32 Amount)
{
	AAHWeaponPickup* Pickup = GetWorld()->SpawnActor<AAHWeaponPickup>(AAHWeaponPickup::StaticClass(), Location, FRotator::ZeroRotator);
	if (Pickup)
	{
		Pickup->PickupType = Type;
		Pickup->Amount = Amount;
		Pickup->PickupMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Weapons/Rifle/Meshes/SM_Rifle.SM_Rifle")));
	}
}

void AAHErebusCombatSliceDirector::SpawnCheckpoint(const FVector& Location, FName Id)
{
	AAHCheckpointActor* Checkpoint = GetWorld()->SpawnActor<AAHCheckpointActor>(AAHCheckpointActor::StaticClass(), Location, FRotator::ZeroRotator);
	if (Checkpoint)
	{
		Checkpoint->CheckpointId = Id;
	}
}

void AAHErebusCombatSliceDirector::SpawnObjectiveZone(const FVector& Location, const FVector& Extent, FName Id)
{
	AAHObjectiveZone* Zone = GetWorld()->SpawnActor<AAHObjectiveZone>(AAHObjectiveZone::StaticClass(), Location, FRotator::ZeroRotator);
	if (Zone)
	{
		Zone->ObjectiveId = Id;
		Zone->Trigger->SetBoxExtent(Extent);
	}
}

void AAHErebusCombatSliceDirector::SpawnFriendly(const FVector& Location)
{
	GetWorld()->SpawnActor<AAHHumanSoldierCharacter>(AAHHumanSoldierCharacter::StaticClass(), Location, FRotator::ZeroRotator);
}

void AAHErebusCombatSliceDirector::HandleObjectiveChanged(FText Objective, int32 Index, int32 Count)
{
}

void AAHErebusCombatSliceDirector::HandleEncounterComplete(FName EncounterId)
{
}
