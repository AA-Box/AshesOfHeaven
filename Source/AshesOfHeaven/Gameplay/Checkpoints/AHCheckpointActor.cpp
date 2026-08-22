#include "Gameplay/Checkpoints/AHCheckpointActor.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
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
		UAHChapterSubsystem* Chapter = GetWorld() && GetWorld()->GetGameInstance()
			? GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>()
			: nullptr;
		const FAHCheckpointSpatialDefinition* Definition = AHChapterSpatial::FindCheckpointDefinition(CheckpointId);
		if (!Chapter || !Definition)
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Spatial][Checkpoint] ignored unknown checkpoint id=%s"), *CheckpointId.ToString());
			return;
		}
		const EAHChapterStage CurrentStage = Chapter->GetStage();
		const FAHStageSpatialDefinition& CurrentDefinition = AHChapterSpatial::GetStageDefinition(CurrentStage);
		const bool bStageCompatible = Definition->Stage == CurrentStage
			|| (UAHChapterSubsystem::ObjectiveIndexForStage(Definition->Stage) == UAHChapterSubsystem::ObjectiveIndexForStage(CurrentStage)
				&& Definition->ZoneId == CurrentDefinition.ZoneId);
		if (!bStageCompatible)
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Spatial][Checkpoint] ignored checkpoint id=%s stage=%s currentStage=%s zone=%s currentZone=%s"), *CheckpointId.ToString(), *UEnum::GetValueAsString(Definition->Stage), *UEnum::GetValueAsString(CurrentStage), *Definition->ZoneId.ToString(), *CurrentDefinition.ZoneId.ToString());
			return;
		}
		if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
		{
			Checkpoints->CaptureCheckpoint(CheckpointId);
		}
	}
}
