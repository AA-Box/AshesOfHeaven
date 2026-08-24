#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AHEnemyAssetSubsystem.generated.h"

struct FStreamableHandle;
class UAHEncounterDefinition;
class UAHEnemyDefinition;

DECLARE_DELEGATE_FourParams(FAHEnemyAssetsReady, FGuid, bool, const TArray<UAHEnemyDefinition*>&, const FString&);

UENUM()
enum class EAHEnemyAssetRequestStatus : uint8
{
	Pending,
	Ready
};

/**
 * Owns asynchronous Primary Asset leases for enemy and encounter definitions. A request keeps
 * the union of its bundles resident until explicitly released; definitions contain no lease state.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHEnemyAssetSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Preloads a known enemy set and returns the lease used to release it. */
	FGuid PreloadEnemyAssets(
		const TArray<FPrimaryAssetId>& EnemyIds,
		const TArray<FName>& Bundles,
		FName LifecycleOwner,
		FAHEnemyAssetsReady Completion = FAHEnemyAssetsReady());

	/** Loads the encounter definition, predicts its enemy set, then preloads the selected bundles. */
	FGuid PreloadEncounterAssets(
		const FPrimaryAssetId& EncounterDefinitionId,
		FName LifecycleOwner,
		FAHEnemyAssetsReady Completion = FAHEnemyAssetsReady());

	/** Preloads an encounter definition that is already resident. */
	FGuid PreloadEncounterAssets(
		const UAHEncounterDefinition* EncounterDefinition,
		FName LifecycleOwner,
		FAHEnemyAssetsReady Completion = FAHEnemyAssetsReady());

	/** Chapter-level lease for a predicted set shared by several encounters. */
	FGuid PreloadChapterAssets(
		FName ChapterId,
		const TArray<FPrimaryAssetId>& EnemyIds,
		bool bLoadVisuals = true,
		bool bLoadAudio = true,
		FAHEnemyAssetsReady Completion = FAHEnemyAssetsReady());

	/** Cancels a pending request. Ready requests should be released instead. */
	void CancelAssetRequest(const FGuid& RequestId);

	/** Releases one encounter/chapter lease and any bundles no longer referenced by another lease. */
	void ReleaseEncounterAssets(const FGuid& RequestId);

	EAHEnemyAssetRequestStatus GetRequestStatus(const FGuid& RequestId) const;
	bool HasRequest(const FGuid& RequestId) const;
	int32 GetActiveRequestCount() const { return Requests.Num(); }
	int32 GetResidentEnemyCount() const { return ResidentEnemies.Num(); }
	int32 GetOutstandingLoadCount() const;

	TArray<FName> BuildBundlesForCurrentPlatform(bool bLoadVisuals, bool bLoadAudio) const;
	void DumpEnemyDefinitions() const;
	void DumpLoadedAssets() const;

private:
	struct FResidentEnemyState
	{
		int32 ReferenceCount = 0;
		TMap<FName, int32> BundleReferenceCounts;
		TSharedPtr<FStreamableHandle> LoadHandle;
		uint64 LoadGeneration = 0;
		bool bLoadComplete = false;
		FString LastLifecycleEvent;
	};

	struct FEnemyAssetRequestState
	{
		FGuid RequestId;
		FName LifecycleOwner = NAME_None;
		FPrimaryAssetId EncounterDefinitionId;
		TArray<FPrimaryAssetId> EnemyIds;
		TArray<FName> Bundles;
		TSharedPtr<FStreamableHandle> EncounterLoadHandle;
		FAHEnemyAssetsReady Completion;
		EAHEnemyAssetRequestStatus Status = EAHEnemyAssetRequestStatus::Pending;
	};

	void BeginEncounterDefinitionLoad(const FGuid& RequestId);
	void HandleEncounterDefinitionLoaded(TSharedPtr<FStreamableHandle> Handle, FGuid RequestId);
	void HandleEncounterDefinitionCanceled(TSharedPtr<FStreamableHandle> Handle, FGuid RequestId);
	bool AddEnemyReferencesAndBeginLoads(const FGuid& RequestId, FString& OutError);
	void BeginOrRefreshEnemyLoad(const FPrimaryAssetId& EnemyId, const TArray<FName>& AddBundles, const TArray<FName>& RemoveBundles);
	void HandleEnemyLoadCompleted(TSharedPtr<FStreamableHandle> Handle, FPrimaryAssetId EnemyId, uint64 Generation);
	void HandleEnemyLoadCanceled(TSharedPtr<FStreamableHandle> Handle, FPrimaryAssetId EnemyId, uint64 Generation);
	void EvaluateRequest(const FGuid& RequestId);
	void EvaluateRequestsUsing(const FPrimaryAssetId& EnemyId);
	void FailRequest(const FGuid& RequestId, const FString& Error);
	void ReleaseRequestInternal(const FGuid& RequestId, bool bNotifyCancellation);
	void ReleaseEncounterDefinitionReference(const FPrimaryAssetId& EncounterDefinitionId);
	void RecordLifecycle(const FString& Event);

	TMap<FPrimaryAssetId, FResidentEnemyState> ResidentEnemies;
	TMap<FGuid, FEnemyAssetRequestState> Requests;
	TMap<FPrimaryAssetId, int32> EncounterReferenceCounts;
	TArray<FString> LifecycleHistory;
};
