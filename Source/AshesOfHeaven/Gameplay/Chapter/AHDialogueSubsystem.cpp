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

void UAHDialogueSubsystem::StartStageEntrySequence(EAHChapterStage Stage)
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

	if (HasActiveDialogue())
	{
		// Never drop a stage beat because another sequence is talking: queue it and play it
		// when the channel frees up. The one-shot guard in StartSequence still prevents
		// replaying a beat that already ran.
		PendingStageEntries.AddUnique(Stage);
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] queue_stage_entry sequence=%s behind=%s pending=%d"), *SequenceId.ToString(), *CurrentSequenceId.ToString(), PendingStageEntries.Num());
		#endif
		return;
	}

	StartSequence(SequenceId, Lines, true);
}

void UAHDialogueSubsystem::DrainPendingStageEntries()
{
	// Each iteration removes one entry, so this terminates even when a queued beat is
	// already completed (StartSequence returns without taking the channel).
	while (!bActive && PendingStageEntries.Num() > 0)
	{
		const EAHChapterStage Stage = PendingStageEntries[0];
		PendingStageEntries.RemoveAt(0);
		StartStageEntrySequence(Stage);
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
