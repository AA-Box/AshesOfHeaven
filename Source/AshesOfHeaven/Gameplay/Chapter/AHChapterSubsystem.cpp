#include "Gameplay/Chapter/AHChapterSubsystem.h"

void UAHChapterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = FAHChapterState();
}

bool UAHChapterSubsystem::HasCompletedNarrativeEvent(FName EventId) const
{
	return EventId != NAME_None && State.CompletedNarrativeEvents.Contains(EventId);
}

bool UAHChapterSubsystem::SetStage(EAHChapterStage NewStage)
{
	if (State.Stage == NewStage)
	{
		return false;
	}

	State.Stage = NewStage;
	State.CompletedSections.AddUnique(FName(*UEnum::GetValueAsString(NewStage)));
	if (NewStage == EAHChapterStage::ChapterComplete)
	{
		State.bChapterComplete = true;
		State.bCountdownActive = false;
	}
	OnStageChanged.Broadcast(NewStage);
	return true;
}

void UAHChapterSubsystem::MarkNarrativeEvent(FName EventId)
{
	if (EventId != NAME_None)
	{
		State.CompletedNarrativeEvents.AddUnique(EventId);
	}
}

void UAHChapterSubsystem::MarkSectionComplete(FName SectionId)
{
	if (SectionId != NAME_None)
	{
		State.CompletedSections.AddUnique(SectionId);
	}
}

void UAHChapterSubsystem::MarkEncounterComplete(FName EncounterId)
{
	if (EncounterId != NAME_None)
	{
		State.CompletedEncounters.AddUnique(EncounterId);
	}
}

void UAHChapterSubsystem::SetCheckpoint(FName CheckpointId)
{
	State.CheckpointId = CheckpointId;
}

void UAHChapterSubsystem::SetObjectiveIndex(int32 NewIndex)
{
	State.ObjectiveIndex = FMath::Max(0, NewIndex);
}

void UAHChapterSubsystem::StartCountdown(float DurationSeconds)
{
	State.CountdownSeconds = FMath::Max(0.0f, DurationSeconds);
	State.bCountdownActive = State.CountdownSeconds > 0.0f;
	LastCountdownMilestone = FMath::CeilToInt(State.CountdownSeconds);
}

void UAHChapterSubsystem::StopCountdown()
{
	State.bCountdownActive = false;
}

void UAHChapterSubsystem::TickCountdown(float DeltaSeconds)
{
	if (!State.bCountdownActive || State.bChapterComplete)
	{
		return;
	}

	State.CountdownSeconds = FMath::Max(0.0f, State.CountdownSeconds - FMath::Max(0.0f, DeltaSeconds));
	const int32 CurrentMilestone = FMath::FloorToInt(State.CountdownSeconds);
	if (CurrentMilestone != LastCountdownMilestone)
	{
		LastCountdownMilestone = CurrentMilestone;
		if (CurrentMilestone % 60 == 0 || CurrentMilestone <= 10)
		{
			OnCountdownMilestone.Broadcast(CurrentMilestone);
		}
	}
	if (State.CountdownSeconds <= 0.0f)
	{
		State.bCountdownActive = false;
	}
}

void UAHChapterSubsystem::SetFailsafeConfirmed(bool bConfirmed)
{
	State.bFailsafeConfirmed = bConfirmed;
}

void UAHChapterSubsystem::SetVehicleState(const FAHVehicleState& VehicleState)
{
	State.Vehicle = VehicleState;
}

void UAHChapterSubsystem::RestoreState(const FAHChapterState& RestoredState)
{
	State = RestoredState;
	LastCountdownMilestone = FMath::FloorToInt(State.CountdownSeconds);
	OnStageChanged.Broadcast(State.Stage);
}
