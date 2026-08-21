#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AHObjectiveSubsystem.generated.h"

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	FName Id = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	FText DisplayText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	FText Description;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAHObjectiveChangedDelegate, FText, Objective, int32, ObjectiveIndex, int32, ObjectiveCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHObjectiveMissionCompleteDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHObjectiveCompletedDelegate, FName, ObjectiveId);

UCLASS()
class ASHESOFHEAVEN_API UAHObjectiveSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UAHObjectiveSubsystem();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	UPROPERTY(BlueprintAssignable, Category="Objective")
	FAHObjectiveChangedDelegate OnObjectiveChanged;

	UPROPERTY(BlueprintAssignable, Category="Objective")
	FAHObjectiveMissionCompleteDelegate OnMissionComplete;

	UPROPERTY(BlueprintAssignable, Category="Objective")
	FAHObjectiveCompletedDelegate OnObjectiveCompleted;

	UFUNCTION(BlueprintPure, Category="Objective")
	const FAHObjectiveDefinition& GetCurrentObjective() const;

	UFUNCTION(BlueprintPure, Category="Objective")
	int32 GetCurrentObjectiveIndex() const { return CurrentObjectiveIndex; }

	UFUNCTION(BlueprintPure, Category="Objective")
	int32 GetObjectiveCount() const { return Objectives.Num(); }

	UFUNCTION(BlueprintPure, Category="Objective")
	bool IsMissionComplete() const { return bMissionComplete; }

	UFUNCTION(BlueprintPure, Category="Objective")
	bool IsCurrentObjective(FName ObjectiveId) const;

	UFUNCTION(BlueprintCallable, Category="Objective")
	bool CompleteObjective(FName ObjectiveId);

	UFUNCTION(BlueprintCallable, Category="Objective")
	void RestoreState(int32 ObjectiveIndex);

	UFUNCTION(BlueprintCallable, Category="Objective")
	void ConfigureObjectives(const TArray<FAHObjectiveDefinition>& NewObjectives, int32 RestoreIndex = 0);

	UFUNCTION(BlueprintCallable, Category="Objective")
	void DebugAdvanceObjective();

	TArray<FName> GetCompletedObjectiveIds() const;

private:
	void BuildDefaultObjectives();
	void BroadcastCurrentObjective();

	UPROPERTY(EditAnywhere, Category="Objective")
	TArray<FAHObjectiveDefinition> Objectives;

	UPROPERTY(VisibleInstanceOnly, Category="Objective")
	int32 CurrentObjectiveIndex = 0;

	UPROPERTY(VisibleInstanceOnly, Category="Objective")
	bool bMissionComplete = false;

	TSet<FName> CompletedObjectives;

	UPROPERTY(EditAnywhere, Category="Audio")
	TObjectPtr<class USoundBase> ObjectiveChangeSound;
};
