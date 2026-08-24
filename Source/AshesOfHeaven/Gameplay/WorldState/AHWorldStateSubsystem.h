#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gameplay/WorldState/AHWorldStateTypes.h"
#include "UObject/ObjectKey.h"
#include "AHWorldStateSubsystem.generated.h"

/** Runtime authority for savable actors, streamed-level restoration, and dirty state. */
UCLASS()
class ASHESOFHEAVEN_API UAHWorldStateSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|World State")
	void RegisterSavableActor(AActor* Actor);

	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|World State")
	void MarkActorDirty(AActor* Actor);

	/** Records a tombstone before an actor is destroyed or a pickup is consumed. */
	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|World State")
	void MarkActorDestroyed(AActor* Actor);

	/** Captures dirty actors in memory; no disk write occurs here. */
	void CaptureDirtyActors();

	/** Produces the deterministic snapshot written at an authorized save boundary. */
	FAHWorldStateSaveData BuildSaveData();

	/** Replaces authoritative state with loaded data; invalid actor records are skipped. */
	void ImportSaveData(const FAHWorldStateSaveData& SaveData);

	/** Clears loaded records, dirty state, and applied-instance bookkeeping for a new expedition. */
	void ResetWorldState();

	const FAHWorldActorState* FindActorState(const FGuid& PersistentId) const;
	int32 GetDirtyActorCount() const { return DirtyActorIds.Num(); }

	static uint32 ComputePayloadCrc(const TArray<uint8>& Payload);
	static bool IsRecordPayloadValid(const FAHWorldActorState& Record);

private:
	void EnsurePlatformStateLoaded();
	void HandleLevelAdded(ULevel* Level, UWorld* World);
	void HandleLevelRemoved(ULevel* Level, UWorld* World);
	void DiscoverLevel(ULevel* Level);
	void CaptureLevel(ULevel* Level);
	bool CaptureActor(AActor* Actor, bool bDestroyedOrConsumed);
	bool ApplyStateToActor(AActor* Actor, const FAHWorldActorState& Record);
	static FName GetActorLevelPackageName(const AActor* Actor);

	TMap<FGuid, FAHWorldActorState> ActorStates;
	TMap<FGuid, TWeakObjectPtr<AActor>> RegisteredActors;
	TSet<FGuid> DirtyActorIds;
	TSet<FObjectKey> AppliedActorInstances;

	FDelegateHandle LevelAddedHandle;
	FDelegateHandle LevelRemovedHandle;
	bool bPlatformStateLoaded = false;
};
