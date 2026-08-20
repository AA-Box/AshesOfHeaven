#include "Gameplay/Checkpoints/AHCheckpointActor.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Components/BoxComponent.h"

AAHCheckpointActor::AAHCheckpointActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetBoxExtent(FVector(240.0f, 240.0f, 120.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AAHCheckpointActor::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AAHCheckpointActor::OnTriggerBeginOverlap);
}

void AAHCheckpointActor::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<AAHCombatPlayerCharacter>(OtherActor))
	{
		if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
		{
			Checkpoints->CaptureCheckpoint(CheckpointId);
		}
	}
}
