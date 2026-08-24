#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "AHDialogueSubsystem.generated.h"

class UAHChapterSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAHDialogueLineChangedDelegate, FName, Speaker, FText, Subtitle, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHDialogueSequenceCompleteDelegate, FName, SequenceId);

UCLASS()
class ASHESOFHEAVEN_API UAHDialogueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FAHDialogueLineChangedDelegate OnLineChanged;

	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FAHDialogueSequenceCompleteDelegate OnSequenceComplete;

	/**
	 * Plays a dialogue sequence. If SequenceId names a canonical Level One sequence, the
	 * canonical lines from AHLevelOneNarrative replace Lines (logged as canonical_level1=true)
	 * so the legacy director's inline copies cannot drift from the authored story data.
	 */
	UFUNCTION(BlueprintCallable, Category="Dialogue")
	void StartSequence(FName SequenceId, const TArray<FAHDialogueLine>& Lines, bool bOneShot = true);

	UFUNCTION(BlueprintCallable, Category="Dialogue")
	void Advance();

	UFUNCTION(BlueprintCallable, Category="Dialogue")
	void SkipCurrentSequence();

	UFUNCTION(BlueprintPure, Category="Dialogue")
	bool HasActiveDialogue() const { return bActive; }

	UFUNCTION(BlueprintPure, Category="Dialogue")
	FName GetCurrentSpeaker() const { return CurrentLine.Speaker; }

	UFUNCTION(BlueprintPure, Category="Dialogue")
	FText GetCurrentSubtitle() const { return CurrentLine.Subtitle; }

	UFUNCTION(BlueprintPure, Category="Dialogue")
	FName GetCurrentSequence() const { return CurrentSequenceId; }

	/** Stage-entry beats waiting for the dialogue channel. Test/diagnostic accessor. */
	UFUNCTION(BlueprintPure, Category="Dialogue")
	int32 GetPendingStageEntryCount() const { return PendingStageEntries.Num(); }

private:
	UFUNCTION()
	void HandleChapterStageChanged(EAHChapterStage Stage);

	UAHChapterSubsystem* GetChapterSubsystem() const;
	void StartStageEntrySequence(EAHChapterStage Stage);
	void DrainPendingStageEntries();
	void ShowNextLine();
	void FinishSequence();

	TArray<FAHDialogueLine> QueuedLines;
	TArray<EAHChapterStage> PendingStageEntries;
	FAHDialogueLine CurrentLine;
	FName CurrentSequenceId = NAME_None;
	int32 CurrentLineIndex = INDEX_NONE;
	bool bActive = false;
	FTimerHandle LineTimer;
	FTimerHandle StageEntryTimer;
};
