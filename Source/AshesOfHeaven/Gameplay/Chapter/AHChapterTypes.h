#pragma once

#include "CoreMinimal.h"
#include "AHChapterTypes.generated.h"

class USoundBase;

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
	TenYearsLater,
	MayaScene,
	NysaTransmission,
	FleetDeparture,
	StarsDisappearing,
	ChapterComplete
};

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
	int32 SaveVersion = 1;

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
