#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Engine/World.h"

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

	GetWorld()->GetTimerManager().ClearTimer(LineTimer);
	QueuedLines = Lines;
	CurrentSequenceId = SequenceId;
	CurrentLineIndex = INDEX_NONE;
	bActive = true;
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Dialogue] begin sequence=%s lines=%d"), *SequenceId.ToString(), Lines.Num());
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
