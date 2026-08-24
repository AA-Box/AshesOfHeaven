#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "AHDialogueSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAHDialogueLineChangedDelegate, FName, Speaker, FText, Subtitle, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHDialogueSequenceCompleteDelegate, FName, SequenceId);

UCLASS()
class ASHESOFHEAVEN_API UAHDialogueSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FAHDialogueLineChangedDelegate OnLineChanged;

	UPROPERTY(BlueprintAssignable, Category="Dialogue")
	FAHDialogueSequenceCompleteDelegate OnSequenceComplete;

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

private:
	UFUNCTION()
	void HandleChapterStageChanged(EAHChapterStage Stage);

	void StartStageEntrySequence(EAHChapterStage Stage);
	void ShowNextLine();
	void FinishSequence();

	TArray<FAHDialogueLine> QueuedLines;
	FAHDialogueLine CurrentLine;
	FName CurrentSequenceId = NAME_None;
	int32 CurrentLineIndex = INDEX_NONE;
	bool bActive = false;
	FTimerHandle LineTimer;
	FTimerHandle StageEntryTimer;
};
