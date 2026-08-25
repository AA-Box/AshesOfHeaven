#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "AHChapterSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHChapterStageChangedDelegate, EAHChapterStage, Stage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAHChapterCountdownChangedDelegate, float, SecondsRemaining, bool, bActive);

UCLASS()
class ASHESOFHEAVEN_API UAHChapterSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category="Chapter")
	FAHChapterStageChangedDelegate OnStageChanged;

	UPROPERTY(BlueprintAssignable, Category="Chapter")
	FAHChapterCountdownChangedDelegate OnCountdownChanged;

	UFUNCTION(BlueprintPure, Category="Chapter")
	const FAHChapterState& GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category="Chapter")
	EAHChapterStage GetStage() const { return State.Stage; }

	UFUNCTION(BlueprintPure, Category="Chapter")
	float GetCountdownSeconds() const { return State.CountdownSeconds; }

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool IsCountdownActive() const { return State.bCountdownActive; }

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool IsChapterComplete() const { return State.Stage == EAHChapterStage::ChapterComplete; }

	/** Returns the only canonical stage for a persisted Chapter One objective index. */
	UFUNCTION(BlueprintPure, Category="Chapter")
	static EAHChapterStage StageForObjectiveIndex(int32 ObjectiveIndex);

	/** Returns the objective represented by a stage, or INDEX_NONE for non-objective stages. */
	UFUNCTION(BlueprintPure, Category="Chapter")
	static int32 ObjectiveIndexForStage(EAHChapterStage Stage);

	/** Repairs old or contradictory saves before any gameplay or presentation is restored. */
	UFUNCTION(BlueprintPure, Category="Chapter")
	static FAHChapterState NormalizeState(const FAHChapterState& Candidate);

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool HasCompletedNarrativeEvent(FName EventId) const;

	UFUNCTION(BlueprintCallable, Category="Chapter")
	bool SetStage(EAHChapterStage NewStage);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void MarkNarrativeEvent(FName EventId);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void MarkSectionComplete(FName SectionId);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void MarkEncounterComplete(FName EncounterId);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void SetCheckpoint(FName CheckpointId);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void SetObjectiveIndex(int32 NewIndex);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void StartCountdown(float DurationSeconds);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void StopCountdown();

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void TickCountdown(float DeltaSeconds);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void SetFailsafeConfirmed(bool bConfirmed);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void SetVehicleState(const FAHVehicleState& VehicleState);

	UFUNCTION(BlueprintCallable, Category="Chapter")
	void RestoreState(const FAHChapterState& RestoredState);

	void CaptureState(FAHChapterState& OutState) const { OutState = State; }

private:
	FAHChapterState State;
	int32 LastCountdownMilestone = INDEX_NONE;
};
