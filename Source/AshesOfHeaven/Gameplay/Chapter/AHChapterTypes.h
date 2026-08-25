#pragma once

#include "CoreMinimal.h"
#include "AHChapterTypes.generated.h"

class USoundBase;

namespace AHChapterStateConstants
{
	// v7 ends Level One at the destruction of Erebus. The old PresentDay stages remain in
	// the enum for save compatibility and future Level Two migration, but are no longer
	// objectives in Level One.
	constexpr int32 CurrentSaveVersion = 7;
	constexpr int32 CurrentSpatialSchemaVersion = 2;
	constexpr int32 ObjectiveCount = 12;
}

UENUM(BlueprintType)
enum class EAHChapterStage : uint8
{
	OpeningBlack,
	ErebusOpening,
	OpeningBattle,
	TransitStation,
	VeilRevelation,
	OpenBattlefield,
	ManticoreSection,
	CathedralApproach,
	FailsafeOrder,
	CathedralInterior,
	SaelTransmission,
	FailsafeTerminal,
	Escape,
	OtherLucian,
	ErebusDestruction,
	// Retained for compatibility. These beats belong to the later campaign now and are
	// not part of Level One: FOR A WHILE.
	TenYearsLater,
	MayaScene,
	NysaTransmission,
	FleetDeparture,
	StarsDisappearing,
	ChapterComplete
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHStageSpatialDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	EAHChapterStage Stage = EAHChapterStage::OpeningBlack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName ZoneId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FVector SafePlayerLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FRotator SafePlayerRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FVector StageAnchor = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FVector ExpectedBoundsMin = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FVector ExpectedBoundsMax = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	float MaxDistanceFromAnchor = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	float GameplayFloorZ = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	bool bRequiresGround = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName EnvironmentProfile = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName ObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName ObjectiveTargetId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FVector ObjectiveTargetLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName CheckpointId = NAME_None;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHCheckpointSpatialDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName CheckpointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	EAHChapterStage Stage = EAHChapterStage::OpeningBlack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FName ZoneId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Spatial")
	FRotator Rotation = FRotator::ZeroRotator;
};

namespace AHChapterSpatial
{
	ASHESOFHEAVEN_API const FAHStageSpatialDefinition& GetStageDefinition(EAHChapterStage Stage);
	ASHESOFHEAVEN_API const TArray<FAHStageSpatialDefinition>& GetStageDefinitions();
	ASHESOFHEAVEN_API const TArray<FAHCheckpointSpatialDefinition>& GetCheckpointDefinitions();
	ASHESOFHEAVEN_API const FAHCheckpointSpatialDefinition* FindCheckpointDefinition(FName CheckpointId);
}

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHDialogueLine
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue")
	FName Speaker = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue")
	FText Subtitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue", meta=(ClampMin=0.1))
	float Duration = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue")
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue")
	bool bCanInterrupt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dialogue")
	TObjectPtr<USoundBase> Voice;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHVehicleState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	bool bSpawned = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	bool bDestroyed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	bool bOccupied = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	float Health = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle")
	int32 DriverSeat = 0;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHChapterState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	int32 SaveVersion = AHChapterStateConstants::CurrentSaveVersion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	EAHChapterStage Stage = EAHChapterStage::OpeningBlack;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	int32 ObjectiveIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	FName CheckpointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	float CountdownSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	bool bCountdownActive = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	bool bFailsafeConfirmed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	bool bChapterComplete = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	TArray<FName> CompletedNarrativeEvents;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	TArray<FName> CompletedSections;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	TArray<FName> CompletedEncounters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	FAHVehicleState Vehicle;
};
