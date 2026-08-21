#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UAHObjectiveSubsystem::UAHObjectiveSubsystem()
{
	BuildDefaultObjectives();
}

void UAHObjectiveSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BuildDefaultObjectives();
	BroadcastCurrentObjective();
}

void UAHObjectiveSubsystem::BuildDefaultObjectives()
{
	Objectives = {
		{FName(TEXT("ReachDefensivePosition")), FText::FromString(TEXT("REACH THE DEFENSIVE POSITION")), FText::FromString(TEXT("Reach the human defensive line."))},
		{FName(TEXT("EliminateVeilAssault")), FText::FromString(TEXT("ELIMINATE THE VEIL ASSAULT")), FText::FromString(TEXT("Break the first Veil push."))},
		{FName(TEXT("AdvanceThroughBreach")), FText::FromString(TEXT("ADVANCE THROUGH THE BREACH")), FText::FromString(TEXT("Push through the ruined passage."))},
		{FName(TEXT("DefendEvacuationGate")), FText::FromString(TEXT("DEFEND THE EVACUATION GATE")), FText::FromString(TEXT("Hold the gate while the line withdraws."))},
		{FName(TEXT("ReachExtraction")), FText::FromString(TEXT("REACH EXTRACTION")), FText::FromString(TEXT("Reach the extraction marker."))}
	};
}

const FAHObjectiveDefinition& UAHObjectiveSubsystem::GetCurrentObjective() const
{
	static const FAHObjectiveDefinition EmptyObjective;
	return Objectives.IsValidIndex(CurrentObjectiveIndex) ? Objectives[CurrentObjectiveIndex] : EmptyObjective;
}

bool UAHObjectiveSubsystem::IsCurrentObjective(FName ObjectiveId) const
{
	return !bMissionComplete && GetCurrentObjective().Id == ObjectiveId;
}

bool UAHObjectiveSubsystem::CompleteObjective(FName ObjectiveId)
{
	if (bMissionComplete || !IsCurrentObjective(ObjectiveId))
	{
		return false;
	}

	CompletedObjectives.Add(ObjectiveId);
	OnObjectiveCompleted.Broadcast(ObjectiveId);
	++CurrentObjectiveIndex;
	if (CurrentObjectiveIndex >= Objectives.Num())
	{
		bMissionComplete = true;
		OnMissionComplete.Broadcast();
		return true;
	}

	BroadcastCurrentObjective();
	return true;
}

void UAHObjectiveSubsystem::ConfigureObjectives(const TArray<FAHObjectiveDefinition>& NewObjectives, int32 RestoreIndex)
{
	Objectives = NewObjectives;
	RestoreState(RestoreIndex);
}

void UAHObjectiveSubsystem::RestoreState(int32 ObjectiveIndex)
{
	CurrentObjectiveIndex = FMath::Clamp(ObjectiveIndex, 0, Objectives.Num());
	CompletedObjectives.Reset();
	for (int32 Index = 0; Index < CurrentObjectiveIndex && Objectives.IsValidIndex(Index); ++Index)
	{
		CompletedObjectives.Add(Objectives[Index].Id);
	}
	bMissionComplete = CurrentObjectiveIndex >= Objectives.Num();
	if (bMissionComplete)
	{
		OnMissionComplete.Broadcast();
	}
	else
	{
		BroadcastCurrentObjective();
	}
}

void UAHObjectiveSubsystem::DebugAdvanceObjective()
{
	if (!bMissionComplete)
	{
		CompleteObjective(GetCurrentObjective().Id);
	}
}

TArray<FName> UAHObjectiveSubsystem::GetCompletedObjectiveIds() const
{
	TArray<FName> Result;
	for (const FName& ObjectiveId : CompletedObjectives)
	{
		Result.Add(ObjectiveId);
	}
	return Result;
}

void UAHObjectiveSubsystem::BroadcastCurrentObjective()
{
	if (!bMissionComplete)
	{
		OnObjectiveChanged.Broadcast(GetCurrentObjective().DisplayText, CurrentObjectiveIndex, Objectives.Num());
		if (ObjectiveChangeSound && GetWorld())
		{
			UGameplayStatics::PlaySound2D(GetWorld(), ObjectiveChangeSound);
		}
	}
}
