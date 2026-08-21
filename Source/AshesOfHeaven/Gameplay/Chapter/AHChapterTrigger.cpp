#include "Gameplay/Chapter/AHChapterTrigger.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Components/BoxComponent.h"

AAHChapterTrigger::AAHChapterTrigger()
{
	PrimaryActorTick.bCanEverTick = false;
	Trigger = CreateDefaultSubobject<UBoxComponent>(TEXT("Trigger"));
	RootComponent = Trigger;
	Trigger->SetBoxExtent(FVector(300.0f, 900.0f, 180.0f));
	Trigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Trigger->SetCollisionResponseToAllChannels(ECR_Ignore);
	Trigger->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void AAHChapterTrigger::BeginPlay()
{
	Super::BeginPlay();
	Trigger->OnComponentBeginOverlap.AddDynamic(this, &AAHChapterTrigger::HandleOverlap);
}

void AAHChapterTrigger::ResetTrigger()
{
	bTriggered = false;
}

void AAHChapterTrigger::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bOneShot && bTriggered)
	{
		return;
	}
	if (Cast<AAHCombatPlayerCharacter>(OtherActor))
	{
		bTriggered = true;
		OnTriggered.Broadcast(TriggerId);
	}
}
