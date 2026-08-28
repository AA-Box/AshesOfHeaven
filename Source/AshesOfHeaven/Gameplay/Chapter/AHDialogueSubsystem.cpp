#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHLevelOneNarrative.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UAHDialogueSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	// The chapter subsystem lives on the game instance, which is not guaranteed to be
	// attached to the world yet when world subsystems initialize. Binding at Initialize()
	// silently dropped every stage-entry beat whenever the game instance arrived later.
	if (UAHChapterSubsystem* Chapter = GetChapterSubsystem())
	{
		Chapter->OnStageChanged.AddDynamic(this, &UAHDialogueSubsystem::HandleChapterStageChanged);
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] bound_stage_delegate world=%s"), *InWorld.GetName());
		#endif
	}
	else
	{
		// Warning, not error: bare automation worlds legitimately have no game instance.
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase3.2][Dialogue] no chapter subsystem at world begin play; stage-entry dialogue is disabled"));
	}
}

void UAHDialogueSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LineTimer);
		GetWorld()->GetTimerManager().ClearTimer(StageEntryTimer);
	}
	PendingStageEntries.Reset();
	if (UAHChapterSubsystem* Chapter = GetChapterSubsystem())
	{
		Chapter->OnStageChanged.RemoveDynamic(this, &UAHDialogueSubsystem::HandleChapterStageChanged);
	}
	Super::Deinitialize();
}

UAHChapterSubsystem* UAHDialogueSubsystem::GetChapterSubsystem() const
{
	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UAHChapterSubsystem>() : nullptr;
}

void UAHDialogueSubsystem::HandleChapterStageChanged(EAHChapterStage Stage)
{
	if (!GetWorld())
	{
		return;
	}

	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] stage_changed=%s"), *UEnum::GetValueAsString(Stage));
	#endif
	// Deferred by one short beat so the director's own dialogue for this stage (started
	// immediately after SetStage) claims the channel first; the stage-entry beat then
	// queues behind it instead of racing it.
	GetWorld()->GetTimerManager().SetTimer(
		StageEntryTimer,
		FTimerDelegate::CreateWeakLambda(this, [this, Stage]()
		{
			StartStageEntrySequence(Stage);
		}),
		0.05f,
		false);
}

void UAHDialogueSubsystem::StartStageEntrySequence(EAHChapterStage Stage, int32 ResumeLineIndex)
{
	if (!GetWorld())
	{
		return;
	}

	FName SequenceId = NAME_None;
	TArray<FAHDialogueLine> Lines;
	if (!AHLevelOneNarrative::BuildStageEntrySequence(Stage, SequenceId, Lines))
	{
		return;
	}

	const int32 ResumeFrom = FMath::Clamp(ResumeLineIndex, 0, Lines.Num());
	if (ResumeFrom >= Lines.Num())
	{
		// Nothing left to say; treat the beat as delivered.
		return;
	}
	if (ResumeFrom > 0)
	{
		Lines.RemoveAt(0, ResumeFrom);
	}

	if (HasActiveDialogue())
	{
		// Never drop a stage beat because another sequence is talking: queue it and play it
		// when the channel frees up. The one-shot guard in StartSequence still prevents
		// replaying a beat that already ran.
		if (!PendingStageEntries.ContainsByPredicate([Stage](const FAHPendingStageEntry& Entry) { return Entry.Stage == Stage; }))
		{
			PendingStageEntries.Add({Stage, ResumeFrom});
		}
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] queue_stage_entry sequence=%s resume=%d behind=%s pending=%d"), *SequenceId.ToString(), ResumeFrom, *CurrentSequenceId.ToString(), PendingStageEntries.Num());
		#endif
		return;
	}

	StartSequence(SequenceId, Lines, true);
	if (bActive && CurrentSequenceId == SequenceId)
	{
		// Remember which beat owns the channel so a preempting director sequence can put it
		// back rather than truncating it.
		bActiveSequenceIsStageEntry = true;
		ActiveStageEntryStage = Stage;
		ActiveStageEntryResumeBase = ResumeFrom;
	}
}

void UAHDialogueSubsystem::RequeueActiveStageEntry()
{
	if (!bActive || !bActiveSequenceIsStageEntry)
	{
		return;
	}
	// Resume on the line that was cut off: the player only heard part of it. The beat has not
	// been marked complete, so the one-shot guard still lets it play.
	const int32 ResumeLineIndex = ActiveStageEntryResumeBase + FMath::Max(0, CurrentLineIndex);
	PendingStageEntries.RemoveAll([this](const FAHPendingStageEntry& Entry) { return Entry.Stage == ActiveStageEntryStage; });
	PendingStageEntries.Insert({ActiveStageEntryStage, ResumeLineIndex}, 0);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] requeue_preempted sequence=%s resume=%d"), *CurrentSequenceId.ToString(), ResumeLineIndex);
	#endif
}

void UAHDialogueSubsystem::DrainPendingStageEntries()
{
	// A queued beat is only allowed to play inside its own objective window. Without this the
	// queue is unbounded in time: an Escape beat stuck behind the long terminal sequence
	// arrived a stage late, over dialogue for a scene the player had already left.
	// Objective index, not the raw stage, is the window - stages that share an index
	// (CathedralInterior/SaelTransmission, Escape/OtherLucian) are the same beat's window.
	const UAHChapterSubsystem* Chapter = GetChapterSubsystem();
	const int32 CurrentObjective = Chapter ? UAHChapterSubsystem::ObjectiveIndexForStage(Chapter->GetStage()) : INDEX_NONE;

	// Each iteration removes one entry, so this terminates even when a queued beat is
	// already completed (StartSequence returns without taking the channel).
	while (!bActive && PendingStageEntries.Num() > 0)
	{
		const FAHPendingStageEntry Entry = PendingStageEntries[0];
		PendingStageEntries.RemoveAt(0);
		const int32 EntryObjective = UAHChapterSubsystem::ObjectiveIndexForStage(Entry.Stage);
		if (CurrentObjective != INDEX_NONE && EntryObjective != INDEX_NONE && EntryObjective < CurrentObjective)
		{
			#if !UE_BUILD_SHIPPING
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] drop_stale_stage_entry stage=%s entryObjective=%d currentObjective=%d"), *UEnum::GetValueAsString(Entry.Stage), EntryObjective, CurrentObjective);
			#endif
			continue;
		}
		StartStageEntrySequence(Entry.Stage, Entry.ResumeLineIndex);
	}
}

void UAHDialogueSubsystem::StartSequence(FName SequenceId, const TArray<FAHDialogueLine>& Lines, bool bOneShot)
{
	if (SequenceId == NAME_None || Lines.IsEmpty() || !GetWorld())
	{
		return;
	}

	if (bOneShot)
	{
		if (UAHChapterSubsystem* Chapter = GetChapterSubsystem())
		{
			if (Chapter->HasCompletedNarrativeEvent(SequenceId))
			{
				#if !UE_BUILD_SHIPPING
				UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] skip_completed sequence=%s"), *SequenceId.ToString());
				#endif
				OnSequenceComplete.Broadcast(SequenceId);
				return;
			}
		}
	}

	TArray<FAHDialogueLine> CanonicalLines;
	const bool bCanonicalLevelOneSequence = AHLevelOneNarrative::ResolveDirectorSequence(SequenceId, CanonicalLines);

	// A director sequence takes the channel immediately - progression depends on it. If it is
	// cutting off a stage beat, put that beat back in the queue instead of discarding it.
	RequeueActiveStageEntry();
	bActiveSequenceIsStageEntry = false;

	GetWorld()->GetTimerManager().ClearTimer(LineTimer);
	QueuedLines = bCanonicalLevelOneSequence ? MoveTemp(CanonicalLines) : Lines;
	CurrentSequenceId = SequenceId;
	CurrentLineIndex = INDEX_NONE;
	bActive = true;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] begin sequence=%s lines=%d canonical_level1=%s"), *SequenceId.ToString(), QueuedLines.Num(), bCanonicalLevelOneSequence ? TEXT("true") : TEXT("false"));
	#endif
	ShowNextLine();
}

void UAHDialogueSubsystem::Advance()
{
	if (bActive)
	{
		ShowNextLine();
	}
}

void UAHDialogueSubsystem::SkipCurrentSequence()
{
	if (bActive)
	{
		FinishSequence();
	}
}

void UAHDialogueSubsystem::ShowNextLine()
{
	if (!bActive)
	{
		return;
	}

	++CurrentLineIndex;
	if (!QueuedLines.IsValidIndex(CurrentLineIndex))
	{
		FinishSequence();
		return;
	}

	CurrentLine = QueuedLines[CurrentLineIndex];
	if (!CurrentLine.Voice && GetWorld())
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayUICue(EAHAudioCue::Dialogue, 0.35f);
		}
	}
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] line sequence=%s index=%d speaker=%s duration=%0.1f"), *CurrentSequenceId.ToString(), CurrentLineIndex, *CurrentLine.Speaker.ToString(), CurrentLine.Duration);
	#endif
	OnLineChanged.Broadcast(CurrentLine.Speaker, CurrentLine.Subtitle, CurrentLine.Duration);
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(LineTimer, this, &UAHDialogueSubsystem::ShowNextLine, FMath::Max(0.1f, CurrentLine.Duration), false);
	}
}

void UAHDialogueSubsystem::FinishSequence()
{
	if (!bActive)
	{
		return;
	}

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LineTimer);
	}
	if (UAHChapterSubsystem* Chapter = GetChapterSubsystem())
	{
		Chapter->MarkNarrativeEvent(CurrentSequenceId);
	}

	const FName CompletedSequence = CurrentSequenceId;
	bActive = false;
	bActiveSequenceIsStageEntry = false;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] complete sequence=%s"), *CompletedSequence.ToString());
	#endif
	CurrentSequenceId = NAME_None;
	CurrentLineIndex = INDEX_NONE;
	QueuedLines.Reset();
	CurrentLine = FAHDialogueLine();
	OnSequenceComplete.Broadcast(CompletedSequence);
	DrainPendingStageEntries();
}
