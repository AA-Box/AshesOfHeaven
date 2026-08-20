#include "Gameplay/Objectives/AHObjectiveZone.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Components/BoxComponent.h"

AAHObjectiveZone::AAHObjectiveZone()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AAHObjectiveZone::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AAHObjectiveZone::OnTriggerBeginOverlap);
}

void AAHObjectiveZone::OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (Cast<AAHCombatPlayerCharacter>(OtherActor))
	{
		if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
		{
			if (Objectives->CompleteObjective(ObjectiveId) && bDestroyAfterCompletion)
			{
				Destroy();
			}
		}
	}
}
