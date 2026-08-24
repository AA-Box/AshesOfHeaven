#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Gameplay/WorldState/AHWorldStateTypes.h"
#include "AHSavableActor.generated.h"

UINTERFACE(BlueprintType)
class ASHESOFHEAVEN_API UAHSavableActor : public UInterface
{
	GENERATED_BODY()
};

/** Contract for actors whose meaningful campaign state survives level and process reloads. */
class ASHESOFHEAVEN_API IAHSavableActor
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ashes of Heaven|World State")
	FGuid GetPersistentId() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ashes of Heaven|World State")
	int32 GetWorldStateVersion() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ashes of Heaven|World State")
	EAHWorldStateSerializationMode GetWorldStateSerializationMode() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Ashes of Heaven|World State")
	bool ShouldSaveWorldTransform() const;

	/** Writes the actor's compact state. Used only in ExplicitPayload mode. */
	UFUNCTION(BlueprintNativeEvent, Category="Ashes of Heaven|World State")
	bool CaptureWorldState(TArray<uint8>& OutPayload) const;

	/** Applies explicit state; SavedStateVersion allows actor-local migration. */
	UFUNCTION(BlueprintNativeEvent, Category="Ashes of Heaven|World State")
	bool RestoreWorldState(const TArray<uint8>& Payload, int32 SavedStateVersion);

	/** Called after transform and payload restoration have both succeeded. */
	UFUNCTION(BlueprintNativeEvent, Category="Ashes of Heaven|World State")
	void OnWorldStateRestored();
};
