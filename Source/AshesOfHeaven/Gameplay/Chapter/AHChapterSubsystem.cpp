#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "AshesOfHeaven.h"

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
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Chapter] stage=%s"), *UEnum::GetValueAsString(NewStage));
	#endif
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
	OnCountdownChanged.Broadcast(State.CountdownSeconds, State.bCountdownActive);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Countdown] begin seconds=%0.1f"), State.CountdownSeconds);
	#endif
}

void UAHChapterSubsystem::StopCountdown()
{
	State.bCountdownActive = false;
	OnCountdownChanged.Broadcast(State.CountdownSeconds, false);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Countdown] stop remaining=%0.1f"), State.CountdownSeconds);
	#endif
}

void UAHChapterSubsystem::TickCountdown(float DeltaSeconds)
{
	if (!State.bCountdownActive || State.bChapterComplete)
	{
		return;
	}

	State.CountdownSeconds = FMath::Max(0.0f, State.CountdownSeconds - FMath::Max(0.0f, DeltaSeconds));
	const bool bWasActive = State.bCountdownActive;
	const int32 CurrentMilestone = FMath::FloorToInt(State.CountdownSeconds);
	if (CurrentMilestone != LastCountdownMilestone)
	{
		LastCountdownMilestone = CurrentMilestone;
		if (CurrentMilestone % 60 == 0 || CurrentMilestone <= 10)
		{
			#if !UE_BUILD_SHIPPING
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Countdown] milestone seconds=%d"), CurrentMilestone);
			#endif
			OnCountdownMilestone.Broadcast(CurrentMilestone);
		}
	}
	if (State.CountdownSeconds <= 0.0f)
	{
		State.bCountdownActive = false;
	}
	if (bWasActive && (FMath::FloorToInt(State.CountdownSeconds) != FMath::FloorToInt(State.CountdownSeconds + FMath::Max(0.0f, DeltaSeconds)) || !State.bCountdownActive))
	{
		OnCountdownChanged.Broadcast(State.CountdownSeconds, State.bCountdownActive);
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
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Chapter] restore stage=%s objective=%d checkpoint=%s countdown=%0.1f vehicle_spawned=%s vehicle_destroyed=%s"), *UEnum::GetValueAsString(State.Stage), State.ObjectiveIndex, *State.CheckpointId.ToString(), State.CountdownSeconds, State.Vehicle.bSpawned ? TEXT("true") : TEXT("false"), State.Vehicle.bDestroyed ? TEXT("true") : TEXT("false"));
	#endif
	OnStageChanged.Broadcast(State.Stage);
	OnCountdownChanged.Broadcast(State.CountdownSeconds, State.bCountdownActive);
}
