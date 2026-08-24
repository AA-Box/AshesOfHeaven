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
class UAHEncounterDirectorSubsystem;
class UMaterialInterface;
class UStaticMesh;
class AStaticMeshActor;
class ASkeletalMeshActor;
class UNiagaraSystem;

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

	/** Development review toggle; collision and navigation remain unchanged. */
	void SetGreyboxVisualVisibility(bool bVisible);
	/** Development review toggle for the authored presentation layer; gameplay collision remains unchanged. */
	void SetPresentationVisualVisibility(bool bVisible);

	UFUNCTION(BlueprintPure, Category="Chapter")
	EAHChapterStage GetCurrentStage() const;

	UFUNCTION(BlueprintPure, Category="Chapter")
	float GetDestructionFadeAlpha() const { return DestructionFadeAlpha; }

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool IsOpeningBlack() const;

	UFUNCTION(BlueprintPure, Category="Chapter")
	bool IsOpeningPresentationActive() const;

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

	UFUNCTION(BlueprintCallable, Category="Chapter|Debug")
	void DebugSpatialAudit();

protected:
	void BuildGreybox();
	void BuildCathedralSpatialRoute();
	void BuildStageAnchors();
	void SpawnGreyboxLighting();
	void BuildMissionGraph();
	void BuildMissionActors();
	void BuildVisualArtTargets();
	void BuildErebusArtTarget();
	void BuildTransitStationArtTarget();
	void BuildCathedralArtTarget();
	void BuildPresentDayArtTarget();
	void ConfigureObjectives();
	void StartStage(EAHChapterStage Stage);
	void StartDialogueSequence(FName SequenceId, const TArray<FAHDialogueLine>& Lines);
	void CompleteCurrentObjective();
	void SpawnBattlefieldSimulation();
	void SpawnOpeningBattle();
	void SpawnOpenBattlefieldEncounter();
	void SpawnEscapeEncounter();
	void SpawnPresentDayScene();
	void ActivateArtTargetView(FString TargetName);
	void SpawnCathedralTerminal();
	void SpawnManticore();
	void SpawnBlock(const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator, UMaterialInterface* MaterialOverride = nullptr);
	AStaticMeshActor* SpawnVisualShape(const TCHAR* MeshPath, const FVector& Location, const FVector& Scale, const FRotator& Rotation = FRotator::ZeroRotator, UMaterialInterface* MaterialOverride = nullptr);
	/** Runtime material variant of an authored master/instance; parameter names follow the Phase 4.2 masters (BaseTint/Roughness/Wetness/Grime). Negative scalars leave the parent value. */
	UMaterialInterface* MakeTintedMaterial(UMaterialInterface* Parent, const FLinearColor& BaseTint, float Roughness = -1.0f, float Wetness = -1.0f, float Grime = -1.0f, float Metallic = -1.0f);
	void BuildErebusSkyline();
	/** Streams the authored Erebus opening presentation level at the canonical stage anchor.
	 * Returns true when the level loaded with content; the legacy primitive build is the fallback. */
	bool TryLoadAuthoredErebusZone();
	/** Fires/smoke/ash for the authored zone via the proven runtime Niagara spawn path. */
	void BuildErebusZoneEffects();
	void SpawnBannerMonolith(const FVector& BaseCenter, const FVector& Scale, float YawDegrees);
	void SpawnRubblePatch(const FVector& Center, float Radius, int32 Count, uint32 Seed);
	void SpawnPuddle(const FVector& Center, const FVector2D& Extent, float YawDegrees);
	void SpawnHangingChain(const FVector& Top, const FVector& Bottom);
	void SpawnVisualLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius);
	/** Local sky-bounce fill: no shadows, no GI, no volumetric contribution, so it lifts one
	 *  occluded pocket without touching the global exposure or the fog aperture. */
	void SpawnBounceFill(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius);
	void SpawnVisualDust(const FVector& Location, float Scale = 1.0f);
	AActor* SpawnPresentationProp(const TCHAR* BlueprintPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale);
	FName ResolvePresentationZone(const FVector& Location) const;
	void SpawnVisualEffect(const TCHAR* SystemPath, const FVector& Location, const FVector& Scale = FVector::OneVector);
	void SpawnCathedralGlyph(const FVector& Location, float Radius, float Scale = 1.0f);
	ASkeletalMeshActor* SpawnVisualCharacter(const TCHAR* MeshPath, const TCHAR* MaterialPath, const FVector& Location, const FRotator& Rotation, float Scale, FName DisplayId);
	void SpawnCheckpoint(const FVector& Location, FName Id);
	AAHChapterTrigger* SpawnTrigger(const FVector& Location, const FVector& Extent, FName Id, EAHChapterStage Stage = EAHChapterStage::OpeningBlack);
	AAHCombatEncounter* SpawnEncounter(FName Id, const FVector& Location, int32 Count, FName ObjectiveOnComplete, const TArray<FVector>& Spawns, bool bAutoActivate = false);
	void SpawnFriendly(const FVector& Location, FName DisplayId = NAME_None);
	void SpawnLabel(const FVector& Location, const FString& Text, const FColor& Color = FColor::White, float WorldSize = 90.0f, const FRotator& Rotation = FRotator(0.0f, 90.0f, 0.0f));
	void TeleportPlayer(const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator);
	// Projects the named review regions into normalised viewport space and logs them, so the
	// acceptance harness can score "is the road readable" instead of scoring the whole frame and
	// counting a deliberately black building silhouette as a failure.
	void LogArtRoiProjections() const;
	// Places a review camera and keeps it there. The spatial-recovery pass re-teleports the player
	// to the stage spawn a fraction of a second after a plain TeleportPlayer, which silently undid
	// every review pose that did not re-apply past it.
	void HoldReviewPose(const FVector& Location, const FRotator& Rotation);
	/** The -ArtTarget=Combatant test subject, tracked so a capture can say "it was destroyed"
	 *  instead of silently reporting the character as unmeasured. */
	TWeakObjectPtr<ACharacter> ArtBenchSubject;
	bool ValidateStageSpatialDefinition(const FAHStageSpatialDefinition& Definition, bool bLogDetails) const;
	bool ValidateStageSpatialState(EAHChapterStage Stage, bool bLogDetails) const;
	void EnsureStageSpatialValidity(EAHChapterStage Stage, const TCHAR* Reason);
	void RunDelayedStageSpatialValidation(EAHChapterStage Stage);
	void ValidateStageObjectiveConsistency(EAHChapterStage Stage) const;
	void LogPresentationState(EAHChapterStage Stage);
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
	TObjectPtr<UAHEncounterDirectorSubsystem> EncounterDirector;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> BlockMesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BlockMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> CathedralMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> HumanMetalMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> ConcreteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> VeilObsidianMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> EmissiveTechnologyMaterial;

	// Runtime-tinted variants created once in BuildVisualArtTargets (MIDs need a live world).
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> MudMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WallConcreteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> PuddleMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> RuinSilhouetteMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BannerClothMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> BannerEmblemMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> DarkStructureMaterial;

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

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> StageAnchors;

	FTimerHandle StageTimer;
	FTimerHandle StageSpatialValidationTimer;
	float StageElapsed = 0.0f;
	float LastSpatialRecoveryTime = -BIG_NUMBER;
	float DestructionFadeAlpha = 0.0f;
	bool bMissionActorsBuilt = false;
	bool bOpeningSequenceStarted = false;
	bool bOrderSequenceStarted = false;
	bool bSaelSequenceStarted = false;
	bool bMayaSceneStarted = false;
	bool bNysaSequenceStarted = false;
	bool bOtherLucianSequenceStarted = false;
	bool bOtherLucianShown = false;
	bool bVisualArtTargetsBuilt = false;
	bool bErebusAuthoredZoneActive = false;
	int32 PresentationActorCount = 0;
	int32 PresentationVFXCount = 0;
	int32 MissingPresentationAssets = 0;
	int32 FailedBlueprintSpawns = 0;
	int32 FailedMeshLoads = 0;
	int32 FailedVFXLoads = 0;
	EAHChapterStage LastLoggedPresentationStage = EAHChapterStage::OpeningBlack;
	bool bHasLoggedPresentationStage = false;
};
