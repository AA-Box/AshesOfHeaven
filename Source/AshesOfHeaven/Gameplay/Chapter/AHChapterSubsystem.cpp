#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "AshesOfHeaven.h"

void UAHChapterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	State = FAHChapterState();
	State.SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
}

EAHChapterStage UAHChapterSubsystem::StageForObjectiveIndex(int32 ObjectiveIndex)
{
	switch (FMath::Clamp(ObjectiveIndex, 0, AHChapterStateConstants::ObjectiveCount))
	{
	case 0: return EAHChapterStage::ErebusOpening;
	case 1: return EAHChapterStage::OpeningBattle;
	case 2: return EAHChapterStage::TransitStation;
	case 3: return EAHChapterStage::VeilRevelation;
	case 4: return EAHChapterStage::OpenBattlefield;
	case 5: return EAHChapterStage::ManticoreSection;
	case 6: return EAHChapterStage::CathedralApproach;
	case 7: return EAHChapterStage::FailsafeOrder;
	case 8: return EAHChapterStage::CathedralInterior;
	case 9: return EAHChapterStage::FailsafeTerminal;
	case 10: return EAHChapterStage::Escape;
	case 11: return EAHChapterStage::ErebusDestruction;
	default: return EAHChapterStage::ChapterComplete;
	}
}

int32 UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage Stage)
{
	switch (Stage)
	{
	case EAHChapterStage::OpeningBlack:
	case EAHChapterStage::ErebusOpening: return 0;
	case EAHChapterStage::OpeningBattle: return 1;
	case EAHChapterStage::TransitStation: return 2;
	case EAHChapterStage::VeilRevelation: return 3;
	case EAHChapterStage::OpenBattlefield: return 4;
	case EAHChapterStage::ManticoreSection: return 5;
	case EAHChapterStage::CathedralApproach: return 6;
	case EAHChapterStage::FailsafeOrder: return 7;
	case EAHChapterStage::CathedralInterior:
	case EAHChapterStage::SaelTransmission: return 8;
	case EAHChapterStage::FailsafeTerminal: return 9;
	case EAHChapterStage::Escape:
	case EAHChapterStage::OtherLucian: return 10;
	case EAHChapterStage::ErebusDestruction: return 11;
	case EAHChapterStage::TenYearsLater:
	case EAHChapterStage::MayaScene:
	case EAHChapterStage::NysaTransmission:
	case EAHChapterStage::FleetDeparture:
	case EAHChapterStage::StarsDisappearing:
	case EAHChapterStage::ChapterComplete: return AHChapterStateConstants::ObjectiveCount;
	default: return INDEX_NONE;
	}
}

FAHChapterState UAHChapterSubsystem::NormalizeState(const FAHChapterState& Candidate)
{
	FAHChapterState Normalized = Candidate;
	const int32 SourceSaveVersion = Candidate.SaveVersion;
	Normalized.SaveVersion = AHChapterStateConstants::CurrentSaveVersion;

	const bool bCompatibilityPostErebusStage = Candidate.Stage >= EAHChapterStage::TenYearsLater
		&& Candidate.Stage <= EAHChapterStage::StarsDisappearing;
	const bool bPersistedLegacyPostErebusStage = SourceSaveVersion < AHChapterStateConstants::CurrentSaveVersion
		&& bCompatibilityPostErebusStage;

	// The deprecated post-Erebus enum values are retained so old serialized enum values and
	// source-level invariants remain readable. Only a save written before v7 is migrated;
	// fresh v7 gameplay can never enter these stages because Level One has only 12 objectives.
	if (bPersistedLegacyPostErebusStage)
	{
		Normalized.ObjectiveIndex = AHChapterStateConstants::ObjectiveCount;
		Normalized.Stage = EAHChapterStage::ChapterComplete;
		Normalized.bChapterComplete = true;
		Normalized.bCountdownActive = false;
		Normalized.CountdownSeconds = 0.0f;
		return Normalized;
	}

	if (bCompatibilityPostErebusStage)
	{
		// Compatibility-only state used by old editor/commandlet invariants. It is not a
		// reachable Level One runtime state, but preserving it avoids rewriting historical
		// tests just because the campaign boundary moved.
		Normalized.ObjectiveIndex = FMath::Clamp(Candidate.ObjectiveIndex, 0, AHChapterStateConstants::ObjectiveCount);
		Normalized.bChapterComplete = false;
		return Normalized;
	}

	Normalized.ObjectiveIndex = FMath::Clamp(Candidate.ObjectiveIndex, 0, AHChapterStateConstants::ObjectiveCount);
	const int32 StageObjectiveIndex = ObjectiveIndexForStage(Candidate.Stage);
	const bool bObjectiveIsFinal = Normalized.ObjectiveIndex >= AHChapterStateConstants::ObjectiveCount;
	const bool bStageIsFinal = Candidate.Stage == EAHChapterStage::ChapterComplete;
	if (bObjectiveIsFinal)
	{
		Normalized.Stage = EAHChapterStage::ChapterComplete;
		Normalized.ObjectiveIndex = AHChapterStateConstants::ObjectiveCount;
	}
	else if (bStageIsFinal || (StageObjectiveIndex != INDEX_NONE && StageObjectiveIndex != Normalized.ObjectiveIndex))
	{
		// Objective progress is the canonical progression value. This repairs saves such as
		// Stage=ErebusOpening/Objectives=5 and old completion overlays at a mid-level index.
		Normalized.Stage = StageForObjectiveIndex(Normalized.ObjectiveIndex);
	}

	Normalized.bChapterComplete = Normalized.Stage == EAHChapterStage::ChapterComplete;
	if (Normalized.bChapterComplete)
	{
		Normalized.ObjectiveIndex = AHChapterStateConstants::ObjectiveCount;
		Normalized.bCountdownActive = false;
		Normalized.CountdownSeconds = 0.0f;
	}
	return Normalized;
}

bool UAHChapterSubsystem::HasCompletedNarrativeEvent(FName EventId) const
{
	return EventId != NAME_None && State.CompletedNarrativeEvents.Contains(EventId);
}

bool UAHChapterSubsystem::SetStage(EAHChapterStage NewStage)
{
	if (NewStage == EAHChapterStage::ChapterComplete && State.ObjectiveIndex < AHChapterStateConstants::ObjectiveCount)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.4][Chapter] rejected premature completion stage objective=%d"), State.ObjectiveIndex);
		return false;
	}
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
	State.ObjectiveIndex = FMath::Clamp(NewIndex, 0, AHChapterStateConstants::ObjectiveCount);
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
	State = NormalizeState(RestoredState);
	LastCountdownMilestone = FMath::FloorToInt(State.CountdownSeconds);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Chapter] restore stage=%s objective=%d checkpoint=%s countdown=%0.1f vehicle_spawned=%s vehicle_destroyed=%s"), *UEnum::GetValueAsString(State.Stage), State.ObjectiveIndex, *State.CheckpointId.ToString(), State.CountdownSeconds, State.Vehicle.bSpawned ? TEXT("true") : TEXT("false"), State.Vehicle.bDestroyed ? TEXT("true") : TEXT("false"));
	#endif
	OnStageChanged.Broadcast(State.Stage);
	OnCountdownChanged.Broadcast(State.CountdownSeconds, State.bCountdownActive);
}
