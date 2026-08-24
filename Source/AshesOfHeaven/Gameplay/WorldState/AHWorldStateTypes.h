#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPath.h"
#include "AHWorldStateTypes.generated.h"

namespace AHWorldStateConstants
{
	constexpr int32 CurrentSchemaVersion = 1;
	constexpr int32 FirstSaveVersion = 5;
}

UENUM(BlueprintType)
enum class EAHWorldStateSerializationMode : uint8
{
	SaveGameProperties,
	ExplicitPayload
};

/** Versioned state for one persistent world actor. A destroyed record is a tombstone. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHWorldActorState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	FGuid PersistentId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	FSoftClassPath ActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	FName LevelPackageName = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	int32 StateVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	EAHWorldStateSerializationMode SerializationMode = EAHWorldStateSerializationMode::ExplicitPayload;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	bool bHasTransform = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	bool bDestroyedOrConsumed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	TArray<uint8> Payload;

	/** CRC isolates corrupt optional actor data from the rest of the campaign save. */
	UPROPERTY()
	uint32 PayloadCrc = 0;
};

/** Persistent campaign-world snapshot embedded in UAHSaveGame. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHWorldStateSaveData
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	int32 SchemaVersion = AHWorldStateConstants::CurrentSchemaVersion;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World State")
	TArray<FAHWorldActorState> Actors;
};
