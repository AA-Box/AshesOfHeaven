#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHLevelOneNarrative.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UAHDialogueSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
		{
			Chapter->OnStageChanged.AddDynamic(this, &UAHDialogueSubsystem::HandleChapterStageChanged);
		}
	}
}

void UAHDialogueSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(LineTimer);
		GetWorld()->GetTimerManager().ClearTimer(StageEntryTimer);
		if (GetWorld()->GetGameInstance())
		{
			if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
			{
				Chapter->OnStageChanged.RemoveDynamic(this, &UAHDialogueSubsystem::HandleChapterStageChanged);
			}
		}
	}
	Super::Deinitialize();
}

void UAHDialogueSubsystem::HandleChapterStageChanged(EAHChapterStage Stage)
{
	if (!GetWorld())
	{
		return;
	}

	GetWorld()->GetTimerManager().ClearTimer(StageEntryTimer);
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
	if (!GetWorld() || HasActiveDialogue())
	{
		return;
	}

	FName SequenceId = NAME_None;
	TArray<FAHDialogueLine> Lines;
	if (AHLevelOneNarrative::BuildStageEntrySequence(Stage, SequenceId, Lines))
	{
		StartSequence(SequenceId, Lines, true);
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
		if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
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
		if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
		{
			Chapter->MarkNarrativeEvent(CurrentSequenceId);
		}
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
}
