#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Chapter/AHChapterTypes.h"

namespace AHLevelOneNarrative
{
	/** Returns canonical Level One dialogue for a sequence owned by the legacy chapter director. */
	ASHESOFHEAVEN_API bool ResolveDirectorSequence(FName SequenceId, TArray<FAHDialogueLine>& OutLines);

	/** Returns a one-shot sequence to fire when entering a stage that has no director-owned dialogue. */
	ASHESOFHEAVEN_API bool BuildStageEntrySequence(EAHChapterStage Stage, FName& OutSequenceId, TArray<FAHDialogueLine>& OutLines);

	/** Total playback seconds of a stage-entry sequence, or 0 when the stage has no beat. */
	ASHESOFHEAVEN_API float GetStageEntrySequenceDuration(EAHChapterStage Stage);

	/**
	 * Seconds the Erebus destruction stage must stay on screen before the final objective
	 * resolves. Derived from the finale sequence so lengthening a line cannot cut the
	 * closing Nysa transmission.
	 */
	ASHESOFHEAVEN_API float GetErebusDestructionHoldSeconds();

	/** True for the final objective of Level One: FOR A WHILE. */
	ASHESOFHEAVEN_API bool IsLevelOneFinalObjective(FName ObjectiveId);
}
