#include "Gameplay/WorldState/AHWorldStateDoor.h"

#include "Gameplay/WorldState/AHPersistentIdComponent.h"
#include "Gameplay/WorldState/AHWorldStateSubsystem.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"

AAHWorldStateDoor::AAHWorldStateDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
	DoorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorMesh"));
	DoorMesh->SetupAttachment(SceneRoot);
	DoorMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DoorMesh->SetCollisionResponseToAllChannels(ECR_Block);
	PersistentIdComponent = CreateDefaultSubobject<UAHPersistentIdComponent>(TEXT("PersistentId"));
}

void AAHWorldStateDoor::BeginPlay()
{
	Super::BeginPlay();
	ClosedMeshTransform = DoorMesh->GetRelativeTransform();
	bOpen = bStartsOpen;
	ApplyDoorPresentation();
	if (UAHWorldStateSubsystem* WorldState = GetWorld()->GetSubsystem<UAHWorldStateSubsystem>())
	{
		WorldState->RegisterSavableActor(this);
	}
}

void AAHWorldStateDoor::SetDoorOpen(bool bNewOpen)
{
	if (bOpen == bNewOpen)
	{
		return;
	}
	bOpen = bNewOpen;
	ApplyDoorPresentation();
	if (UAHWorldStateSubsystem* WorldState = GetWorld()->GetSubsystem<UAHWorldStateSubsystem>())
	{
		WorldState->MarkActorDirty(this);
	}
}

void AAHWorldStateDoor::SetPersistentId(const FGuid& PersistentId)
{
	PersistentIdComponent->SetPersistentId(PersistentId);
}

void AAHWorldStateDoor::Interact_Implementation(AActor* Interactor)
{
	SetDoorOpen(!bOpen);
}

FText AAHWorldStateDoor::GetInteractionPrompt_Implementation() const
{
	return bOpen ? FText::FromString(TEXT("INTERACT  CLOSE DOOR")) : FText::FromString(TEXT("INTERACT  OPEN DOOR"));
}

FGuid AAHWorldStateDoor::GetPersistentId_Implementation() const
{
	return PersistentIdComponent ? PersistentIdComponent->GetPersistentId() : FGuid();
}

bool AAHWorldStateDoor::CaptureWorldState_Implementation(TArray<uint8>& OutPayload) const
{
	OutPayload = { static_cast<uint8>(bOpen ? 1 : 0) };
	return true;
}

bool AAHWorldStateDoor::RestoreWorldState_Implementation(const TArray<uint8>& Payload, int32 SavedStateVersion)
{
	if (SavedStateVersion < 0 || SavedStateVersion > GetWorldStateVersion_Implementation() || Payload.Num() != 1)
	{
		return false;
	}
	bOpen = Payload[0] != 0;
	return true;
}

void AAHWorldStateDoor::OnWorldStateRestored_Implementation()
{
	ApplyDoorPresentation();
}

void AAHWorldStateDoor::ApplyDoorPresentation()
{
	if (!DoorMesh)
	{
		return;
	}
	FTransform NewTransform = ClosedMeshTransform;
	FRotator Rotation = NewTransform.Rotator();
	Rotation.Yaw += bOpen ? OpenAngleDegrees : 0.0f;
	NewTransform.SetRotation(Rotation.Quaternion());
	DoorMesh->SetRelativeTransform(NewTransform);
	DoorMesh->SetCollisionEnabled(bOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryAndPhysics);
}
