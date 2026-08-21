#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "AHChapterOneDirector.generated.h"

class AAHChapterTerminal;
class AAHChapterTrigger;
class AAHCombatEncounter;
class AAHManticoreVehicle;
class AAHCombatantCharacter;
class UAHDialogueSubsystem;
class UMaterialInterface;
class UStaticMesh;

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHMissionStage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mission")
	EAHChapterStage Stage = EAHChapterStage::OpeningBlack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mission")
	FName ObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mission")
	FText StageLabel;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Mission")
	bool bCheckpointSafe = true;
};

UCLASS()
class ASHESOFHEAVEN_API AAHChapterOneDirector : public AActor
{
	GENERATED_BODY()

public:
	AAHChapterOneDirector();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintPure, Category="Chapter")
	EAHChapterStage GetCurrentStage() const;

	UFUNCTION(BlueprintPure, Category="Chapter")
	float GetDestructionFadeAlpha() const { return DestructionFadeAlpha; }

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool IsOpeningBlack() const;

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool IsTitleReveal() const;

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugSkipToStage(EAHChapterStage Stage);

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugLoadCheckpoint(FName CheckpointId);

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugResetChapter();

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugCompleteCurrentEncounter();

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugSetCountdown(float Seconds);

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugSpawnManticore();

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugTeleportToCathedral();

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugTeleportToPresentDay();

protected:
	void BuildGreybox();
	void SpawnGreyboxLighting();
	void BuildMissionGraph();
	void BuildMissionActors();
	void ConfigureObjectives();
	void StartStage(EAHChapterStage Stage);
	void StartDialogueSequence(FName SequenceId, const TArray<FAHDialogueLine>& Lines);
	void CompleteCurrentObjective();
	void SpawnBattlefieldSimulation();
	void SpawnOpeningBattle();
	void SpawnOpenBattlefieldEncounter();
	void SpawnEscapeEncounter();
	void SpawnPresentDayScene();
	void SpawnCathedralTerminal();
	void SpawnManticore();
	void SpawnBlock(const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator, UMaterialInterface* MaterialOverride = nullptr);
	void SpawnCheckpoint(const FVector& Location, FName Id);
	AAHChapterTrigger* SpawnTrigger(const FVector& Location, const FVector& Extent, FName Id);
	AAHCombatEncounter* SpawnEncounter(FName Id, const FVector& Location, int32 Count, FName ObjectiveOnComplete, const TArray<FVector>& Spawns, bool bAutoActivate = false);
	void SpawnFriendly(const FVector& Location, FName DisplayId = NAME_None);
	void SpawnLabel(const FVector& Location, const FString& Text, const FColor& Color = FColor::White);
	void TeleportPlayer(const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);
	UFUNCTION()
	void HandleTrigger(FName TriggerId);
	UFUNCTION()
	void HandleObjectiveCompleted(FName ObjectiveId);
	UFUNCTION()
	void HandleDialogueComplete(FName SequenceId);
	UFUNCTION()
	void HandleTerminalConfirmed();
	UFUNCTION()
	void HandleVehicleEntered(APawn* Driver);
	UFUNCTION()
	void HandleVehicleExited(APawn* Driver);
	UFUNCTION()
	void HandleVehicleDestroyed();
	UFUNCTION()
	void HandleMissionComplete();

	UFUNCTION()
	void FinishDestructionSequence();

	UPROPERTY(Transient)
	TObjectPtr<class UAHChapterSubsystem> Chapter;

	UPROPERTY(Transient)
	TObjectPtr<class UAHDialogueSubsystem> Dialogue;

	UPROPERTY(Transient)
	TObjectPtr<class UAHObjectiveSubsystem> Objectives;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BlockMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BlockMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CathedralMaterial;

	UPROPERTY(Transient)
	TObjectPtr<AAHCombatEncounter> OpeningEncounter;

	UPROPERTY(Transient)
	TObjectPtr<AAHCombatEncounter> BattlefieldEncounter;

	UPROPERTY(Transient)
	TObjectPtr<AAHCombatEncounter> EscapeEncounter;

	UPROPERTY(Transient)
	TObjectPtr<AAHManticoreVehicle> Manticore;

	UPROPERTY(Transient)
	TObjectPtr<AAHChapterTerminal> FailsafeTerminal;

	UPROPERTY()
	TArray<FAHMissionStage> MissionStages;

	UPROPERTY()
	TArray<TObjectPtr<AAHChapterTrigger>> Triggers;

	FTimerHandle StageTimer;
	float StageElapsed = 0.0f;
	float DestructionFadeAlpha = 0.0f;
	bool bMissionActorsBuilt = false;
	bool bOpeningSequenceStarted = false;
	bool bOrderSequenceStarted = false;
	bool bSaelSequenceStarted = false;
	bool bMayaSceneStarted = false;
	bool bNysaSequenceStarted = false;
	bool bOtherLucianSequenceStarted = false;
	bool bOtherLucianShown = false;
};
