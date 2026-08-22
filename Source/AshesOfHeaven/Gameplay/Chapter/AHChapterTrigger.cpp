#include "Gameplay/Chapter/AHChapterTrigger.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
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
		UAHChapterSubsystem* Chapter = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>()
			: nullptr;
		if (!Chapter)
		{
			return;
		}
		const EAHChapterStage CurrentStage = Chapter->GetStage();
		const FAHStageSpatialDefinition& CurrentDefinition = AHChapterSpatial::GetStageDefinition(CurrentStage);
		const int32 CurrentObjectiveIndex = UAHChapterSubsystem::ObjectiveIndexForStage(CurrentStage);
		const int32 TriggerObjectiveIndex = UAHChapterSubsystem::ObjectiveIndexForStage(Stage);
		const bool bStageCompatible = Stage == CurrentStage
			|| (CurrentObjectiveIndex != INDEX_NONE
				&& CurrentObjectiveIndex == TriggerObjectiveIndex
				&& ZoneId != NAME_None
				&& ZoneId == CurrentDefinition.ZoneId);
		if (!bStageCompatible)
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Spatial][Trigger] ignored trigger=%s stage=%s currentStage=%s zone=%s currentZone=%s"), *TriggerId.ToString(), *UEnum::GetValueAsString(Stage), *UEnum::GetValueAsString(CurrentStage), *ZoneId.ToString(), *CurrentDefinition.ZoneId.ToString());
			return;
		}
		bTriggered = true;
		OnTriggered.Broadcast(TriggerId);
	}
}
