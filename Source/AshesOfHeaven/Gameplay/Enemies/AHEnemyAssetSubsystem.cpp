#include "Gameplay/Enemies/AHEnemyAssetSubsystem.h"

#include "AshesOfHeaven.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gameplay/Enemies/AHEncounterDefinition.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "HAL/IConsoleManager.h"
#include "Platform/AHPlatformManagerSubsystem.h"

void UAHEnemyAssetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Collection.InitializeDependency<UAHPlatformManagerSubsystem>();
	Super::Initialize(Collection);
	RecordLifecycle(TEXT("subsystem initialized"));
}

void UAHEnemyAssetSubsystem::Deinitialize()
{
	TArray<FGuid> RequestIds;
	Requests.GetKeys(RequestIds);
	for (const FGuid& RequestId : RequestIds)
	{
		ReleaseRequestInternal(RequestId, false);
	}
	for (const TPair<FPrimaryAssetId, FResidentEnemyState>& Pair : ResidentEnemies)
	{
		UAssetManager::Get().UnloadPrimaryAsset(Pair.Key);
	}
	ResidentEnemies.Reset();
	for (const TPair<FPrimaryAssetId, int32>& Pair : EncounterReferenceCounts)
	{
		UAssetManager::Get().UnloadPrimaryAsset(Pair.Key);
	}
	EncounterReferenceCounts.Reset();
	RecordLifecycle(TEXT("subsystem deinitialized"));
	Super::Deinitialize();
}

FGuid UAHEnemyAssetSubsystem::PreloadEnemyAssets(
	const TArray<FPrimaryAssetId>& EnemyIds,
	const TArray<FName>& Bundles,
	FName LifecycleOwner,
	FAHEnemyAssetsReady Completion)
{
	FEnemyAssetRequestState Request;
	Request.RequestId = FGuid::NewGuid();
	Request.LifecycleOwner = LifecycleOwner;
	Request.Bundles = Bundles;
	Request.Bundles.AddUnique(AHEnemyAssets::CoreBundle);
	Request.Bundles.Sort(FNameLexicalLess());
	for (const FPrimaryAssetId& EnemyId : EnemyIds)
	{
		Request.EnemyIds.AddUnique(EnemyId);
	}
	Request.Completion = MoveTemp(Completion);
	const FGuid RequestId = Request.RequestId;
	Requests.Add(RequestId, MoveTemp(Request));

	FString Error;
	if (!AddEnemyReferencesAndBeginLoads(RequestId, Error))
	{
		FailRequest(RequestId, Error);
		return RequestId;
	}
	RecordLifecycle(FString::Printf(TEXT("request %s owner=%s enemies=%d bundles=%s"),
		*RequestId.ToString(EGuidFormats::DigitsWithHyphensLower), *LifecycleOwner.ToString(), EnemyIds.Num(),
		*FString::JoinBy(Bundles, TEXT(","), [](FName Name) { return Name.ToString(); })));
	EvaluateRequest(RequestId);
	return RequestId;
}

FGuid UAHEnemyAssetSubsystem::PreloadEncounterAssets(
	const FPrimaryAssetId& EncounterDefinitionId,
	FName LifecycleOwner,
	FAHEnemyAssetsReady Completion)
{
	FEnemyAssetRequestState Request;
	Request.RequestId = FGuid::NewGuid();
	Request.LifecycleOwner = LifecycleOwner;
	Request.EncounterDefinitionId = EncounterDefinitionId;
	Request.Completion = MoveTemp(Completion);
	const FGuid RequestId = Request.RequestId;
	Requests.Add(RequestId, MoveTemp(Request));

	if (!EncounterDefinitionId.IsValid() || EncounterDefinitionId.PrimaryAssetType != AHEnemyAssets::EncounterType
		|| UAssetManager::Get().GetPrimaryAssetPath(EncounterDefinitionId).IsNull())
	{
		FailRequest(RequestId, FString::Printf(TEXT("Encounter Primary Asset ID does not resolve: %s"), *EncounterDefinitionId.ToString()));
		return RequestId;
	}

	EncounterReferenceCounts.FindOrAdd(EncounterDefinitionId)++;
	BeginEncounterDefinitionLoad(RequestId);
	return RequestId;
}

FGuid UAHEnemyAssetSubsystem::PreloadEncounterAssets(
	const UAHEncounterDefinition* EncounterDefinition,
	FName LifecycleOwner,
	FAHEnemyAssetsReady Completion)
{
	if (!EncounterDefinition)
	{
		return PreloadEnemyAssets({}, { AHEnemyAssets::CoreBundle }, LifecycleOwner, MoveTemp(Completion));
	}
	TArray<FPrimaryAssetId> EnemyIds;
	EncounterDefinition->GetPredictedEnemySet(EnemyIds);
	return PreloadEnemyAssets(
		EnemyIds,
		BuildBundlesForCurrentPlatform(EncounterDefinition->bPreloadVisuals, EncounterDefinition->bPreloadAudio),
		LifecycleOwner,
		MoveTemp(Completion));
}

FGuid UAHEnemyAssetSubsystem::PreloadChapterAssets(
	FName ChapterId,
	const TArray<FPrimaryAssetId>& EnemyIds,
	bool bLoadVisuals,
	bool bLoadAudio,
	FAHEnemyAssetsReady Completion)
{
	return PreloadEnemyAssets(
		EnemyIds,
		BuildBundlesForCurrentPlatform(bLoadVisuals, bLoadAudio),
		FName(*FString::Printf(TEXT("Chapter.%s"), *ChapterId.ToString())),
		MoveTemp(Completion));
}

void UAHEnemyAssetSubsystem::BeginEncounterDefinitionLoad(const FGuid& RequestId)
{
	FEnemyAssetRequestState* Request = Requests.Find(RequestId);
	if (!Request)
	{
		return;
	}
	FAssetManagerLoadParams Params;
	Params.OnComplete = FStreamableDelegateWithHandle::CreateUObject(this, &ThisClass::HandleEncounterDefinitionLoaded, RequestId);
	Params.OnCancel = FStreamableDelegateWithHandle::CreateUObject(this, &ThisClass::HandleEncounterDefinitionCanceled, RequestId);
	Request->EncounterLoadHandle = UAssetManager::Get().LoadPrimaryAsset(Request->EncounterDefinitionId, {}, MoveTemp(Params));
	RecordLifecycle(FString::Printf(TEXT("encounter load started %s"), *Request->EncounterDefinitionId.ToString()));
}

void UAHEnemyAssetSubsystem::HandleEncounterDefinitionLoaded(TSharedPtr<FStreamableHandle> Handle, FGuid RequestId)
{
	FEnemyAssetRequestState* Request = Requests.Find(RequestId);
	if (!Request || Request->Status != EAHEnemyAssetRequestStatus::Pending)
	{
		return;
	}
	Request->EncounterLoadHandle.Reset();
	const UAHEncounterDefinition* Definition = Cast<UAHEncounterDefinition>(
		UAssetManager::Get().GetPrimaryAssetObject(Request->EncounterDefinitionId));
	if (!Definition)
	{
		FailRequest(RequestId, FString::Printf(TEXT("Loaded encounter has the wrong class: %s"), *Request->EncounterDefinitionId.ToString()));
		return;
	}
	Definition->GetPredictedEnemySet(Request->EnemyIds);
	Request->Bundles = BuildBundlesForCurrentPlatform(Definition->bPreloadVisuals, Definition->bPreloadAudio);

	FString Error;
	if (!AddEnemyReferencesAndBeginLoads(RequestId, Error))
	{
		FailRequest(RequestId, Error);
		return;
	}
	EvaluateRequest(RequestId);
}

void UAHEnemyAssetSubsystem::HandleEncounterDefinitionCanceled(TSharedPtr<FStreamableHandle> Handle, FGuid RequestId)
{
	if (Requests.Contains(RequestId))
	{
		FailRequest(RequestId, TEXT("Encounter definition load was canceled."));
	}
}

bool UAHEnemyAssetSubsystem::AddEnemyReferencesAndBeginLoads(const FGuid& RequestId, FString& OutError)
{
	FEnemyAssetRequestState* Request = Requests.Find(RequestId);
	if (!Request || Request->EnemyIds.IsEmpty())
	{
		OutError = TEXT("Enemy preload request contains no enemy Primary Asset IDs.");
		return false;
	}

	for (const FPrimaryAssetId& EnemyId : Request->EnemyIds)
	{
		if (!EnemyId.IsValid() || EnemyId.PrimaryAssetType != AHEnemyAssets::EnemyType
			|| UAssetManager::Get().GetPrimaryAssetPath(EnemyId).IsNull())
		{
			OutError = FString::Printf(TEXT("Enemy Primary Asset ID does not resolve: %s"), *EnemyId.ToString());
			return false;
		}
	}

	for (const FPrimaryAssetId& EnemyId : Request->EnemyIds)
	{
		FResidentEnemyState& Resident = ResidentEnemies.FindOrAdd(EnemyId);
		const bool bFirstReference = Resident.ReferenceCount++ == 0;
		TArray<FName> AddedBundles;
		for (const FName& Bundle : Request->Bundles)
		{
			int32& Count = Resident.BundleReferenceCounts.FindOrAdd(Bundle);
			if (Count++ == 0)
			{
				AddedBundles.Add(Bundle);
			}
		}
		if (bFirstReference || !AddedBundles.IsEmpty())
		{
			BeginOrRefreshEnemyLoad(EnemyId, AddedBundles, {});
		}
	}
	return true;
}

void UAHEnemyAssetSubsystem::BeginOrRefreshEnemyLoad(
	const FPrimaryAssetId& EnemyId,
	const TArray<FName>& AddBundles,
	const TArray<FName>& RemoveBundles)
{
	FResidentEnemyState* Resident = ResidentEnemies.Find(EnemyId);
	if (!Resident)
	{
		return;
	}
	Resident->bLoadComplete = false;
	const uint64 Generation = ++Resident->LoadGeneration;
	FAssetManagerLoadParams Params;
	Params.OnComplete = FStreamableDelegateWithHandle::CreateUObject(this, &ThisClass::HandleEnemyLoadCompleted, EnemyId, Generation);
	Params.OnCancel = FStreamableDelegateWithHandle::CreateUObject(this, &ThisClass::HandleEnemyLoadCanceled, EnemyId, Generation);
	Resident->LoadHandle = UAssetManager::Get().ChangeBundleStateForPrimaryAssets(
		{ EnemyId }, AddBundles, RemoveBundles, false, MoveTemp(Params));
	Resident->LastLifecycleEvent = FString::Printf(TEXT("generation=%llu add=%s remove=%s"), Generation,
		*FString::JoinBy(AddBundles, TEXT(","), [](FName Name) { return Name.ToString(); }),
		*FString::JoinBy(RemoveBundles, TEXT(","), [](FName Name) { return Name.ToString(); }));
}

void UAHEnemyAssetSubsystem::HandleEnemyLoadCompleted(
	TSharedPtr<FStreamableHandle> Handle,
	FPrimaryAssetId EnemyId,
	uint64 Generation)
{
	FResidentEnemyState* Resident = ResidentEnemies.Find(EnemyId);
	if (!Resident || Resident->LoadGeneration != Generation)
	{
		return;
	}
	Resident->LoadHandle.Reset();
	if (!Cast<UAHEnemyDefinition>(UAssetManager::Get().GetPrimaryAssetObject(EnemyId)))
	{
		TArray<FGuid> FailedRequests;
		for (const TPair<FGuid, FEnemyAssetRequestState>& Pair : Requests)
		{
			if (Pair.Value.EnemyIds.Contains(EnemyId)) FailedRequests.Add(Pair.Key);
		}
		for (const FGuid& RequestId : FailedRequests)
		{
			FailRequest(RequestId, FString::Printf(TEXT("Enemy load completed without a UAHEnemyDefinition: %s"), *EnemyId.ToString()));
		}
		return;
	}
	Resident->bLoadComplete = true;
	Resident->LastLifecycleEvent = FString::Printf(TEXT("ready generation=%llu"), Generation);
	RecordLifecycle(FString::Printf(TEXT("enemy ready %s"), *EnemyId.ToString()));
	EvaluateRequestsUsing(EnemyId);
}

void UAHEnemyAssetSubsystem::HandleEnemyLoadCanceled(
	TSharedPtr<FStreamableHandle> Handle,
	FPrimaryAssetId EnemyId,
	uint64 Generation)
{
	FResidentEnemyState* Resident = ResidentEnemies.Find(EnemyId);
	if (!Resident || Resident->LoadGeneration != Generation)
	{
		return;
	}
	TArray<FGuid> FailedRequests;
	for (const TPair<FGuid, FEnemyAssetRequestState>& Pair : Requests)
	{
		if (Pair.Value.EnemyIds.Contains(EnemyId)) FailedRequests.Add(Pair.Key);
	}
	for (const FGuid& RequestId : FailedRequests)
	{
		FailRequest(RequestId, FString::Printf(TEXT("Enemy asset load was canceled: %s"), *EnemyId.ToString()));
	}
}

void UAHEnemyAssetSubsystem::EvaluateRequest(const FGuid& RequestId)
{
	FEnemyAssetRequestState* Request = Requests.Find(RequestId);
	if (!Request || Request->Status != EAHEnemyAssetRequestStatus::Pending || Request->EnemyIds.IsEmpty())
	{
		return;
	}
	TArray<UAHEnemyDefinition*> Definitions;
	for (const FPrimaryAssetId& EnemyId : Request->EnemyIds)
	{
		const FResidentEnemyState* Resident = ResidentEnemies.Find(EnemyId);
		UAHEnemyDefinition* Definition = Cast<UAHEnemyDefinition>(UAssetManager::Get().GetPrimaryAssetObject(EnemyId));
		if (!Resident || !Resident->bLoadComplete || !Definition)
		{
			return;
		}
		Definitions.Add(Definition);
	}

	Request->Status = EAHEnemyAssetRequestStatus::Ready;
	FAHEnemyAssetsReady Completion = MoveTemp(Request->Completion);
	Request->Completion.Unbind();
	RecordLifecycle(FString::Printf(TEXT("request ready %s owner=%s"),
		*RequestId.ToString(EGuidFormats::DigitsWithHyphensLower), *Request->LifecycleOwner.ToString()));
	if (Completion.IsBound())
	{
		Completion.Execute(RequestId, true, Definitions, FString());
	}
}

void UAHEnemyAssetSubsystem::EvaluateRequestsUsing(const FPrimaryAssetId& EnemyId)
{
	TArray<FGuid> CandidateRequests;
	for (const TPair<FGuid, FEnemyAssetRequestState>& Pair : Requests)
	{
		if (Pair.Value.Status == EAHEnemyAssetRequestStatus::Pending && Pair.Value.EnemyIds.Contains(EnemyId))
		{
			CandidateRequests.Add(Pair.Key);
		}
	}
	for (const FGuid& RequestId : CandidateRequests)
	{
		EvaluateRequest(RequestId);
	}
}

void UAHEnemyAssetSubsystem::FailRequest(const FGuid& RequestId, const FString& Error)
{
	FEnemyAssetRequestState* Request = Requests.Find(RequestId);
	if (!Request)
	{
		return;
	}
	FAHEnemyAssetsReady Completion = MoveTemp(Request->Completion);
	RecordLifecycle(FString::Printf(TEXT("request failed %s error=%s"),
		*RequestId.ToString(EGuidFormats::DigitsWithHyphensLower), *Error));
	ReleaseRequestInternal(RequestId, false);
	if (Completion.IsBound())
	{
		Completion.Execute(RequestId, false, {}, Error);
	}
}

void UAHEnemyAssetSubsystem::CancelAssetRequest(const FGuid& RequestId)
{
	ReleaseRequestInternal(RequestId, true);
}

void UAHEnemyAssetSubsystem::ReleaseEncounterAssets(const FGuid& RequestId)
{
	ReleaseRequestInternal(RequestId, true);
}

void UAHEnemyAssetSubsystem::ReleaseRequestInternal(const FGuid& RequestId, bool bNotifyCancellation)
{
	FEnemyAssetRequestState* Found = Requests.Find(RequestId);
	if (!Found)
	{
		return;
	}
	FEnemyAssetRequestState Request = MoveTemp(*Found);
	Requests.Remove(RequestId);
	if (Request.EncounterLoadHandle.IsValid() && Request.Status == EAHEnemyAssetRequestStatus::Pending)
	{
		Request.EncounterLoadHandle->CancelHandle();
	}

	for (const FPrimaryAssetId& EnemyId : Request.EnemyIds)
	{
		FResidentEnemyState* Resident = ResidentEnemies.Find(EnemyId);
		if (!Resident)
		{
			continue;
		}
		TArray<FName> RemovedBundles;
		for (const FName& Bundle : Request.Bundles)
		{
			if (int32* Count = Resident->BundleReferenceCounts.Find(Bundle))
			{
				if (--(*Count) <= 0)
				{
					Resident->BundleReferenceCounts.Remove(Bundle);
					RemovedBundles.Add(Bundle);
				}
			}
		}
		Resident->ReferenceCount = FMath::Max(0, Resident->ReferenceCount - 1);
		if (Resident->ReferenceCount == 0)
		{
			UAssetManager::Get().UnloadPrimaryAsset(EnemyId);
			ResidentEnemies.Remove(EnemyId);
			RecordLifecycle(FString::Printf(TEXT("enemy released %s"), *EnemyId.ToString()));
		}
		else if (!RemovedBundles.IsEmpty())
		{
			BeginOrRefreshEnemyLoad(EnemyId, {}, RemovedBundles);
		}
	}
	ReleaseEncounterDefinitionReference(Request.EncounterDefinitionId);

	if (bNotifyCancellation && Request.Status == EAHEnemyAssetRequestStatus::Pending && Request.Completion.IsBound())
	{
		Request.Completion.Execute(RequestId, false, {}, TEXT("Enemy asset request was canceled."));
	}
	RecordLifecycle(FString::Printf(TEXT("request released %s owner=%s"),
		*RequestId.ToString(EGuidFormats::DigitsWithHyphensLower), *Request.LifecycleOwner.ToString()));
}

void UAHEnemyAssetSubsystem::ReleaseEncounterDefinitionReference(const FPrimaryAssetId& EncounterDefinitionId)
{
	if (!EncounterDefinitionId.IsValid())
	{
		return;
	}
	int32* Count = EncounterReferenceCounts.Find(EncounterDefinitionId);
	if (Count && --(*Count) <= 0)
	{
		EncounterReferenceCounts.Remove(EncounterDefinitionId);
		UAssetManager::Get().UnloadPrimaryAsset(EncounterDefinitionId);
	}
}

EAHEnemyAssetRequestStatus UAHEnemyAssetSubsystem::GetRequestStatus(const FGuid& RequestId) const
{
	if (const FEnemyAssetRequestState* Request = Requests.Find(RequestId))
	{
		return Request->Status;
	}
	return EAHEnemyAssetRequestStatus::Pending;
}

bool UAHEnemyAssetSubsystem::HasRequest(const FGuid& RequestId) const
{
	return Requests.Contains(RequestId);
}

int32 UAHEnemyAssetSubsystem::GetOutstandingLoadCount() const
{
	int32 Count = 0;
	for (const TPair<FPrimaryAssetId, FResidentEnemyState>& Pair : ResidentEnemies)
	{
		Count += Pair.Value.bLoadComplete ? 0 : 1;
	}
	for (const TPair<FGuid, FEnemyAssetRequestState>& Pair : Requests)
	{
		Count += Pair.Value.EncounterLoadHandle.IsValid() ? 1 : 0;
	}
	return Count;
}

TArray<FName> UAHEnemyAssetSubsystem::BuildBundlesForCurrentPlatform(bool bLoadVisuals, bool bLoadAudio) const
{
	TArray<FName> Bundles { AHEnemyAssets::CoreBundle };
	const UAHPlatformManagerSubsystem* Platform = GetGameInstance()
		? GetGameInstance()->GetSubsystem<UAHPlatformManagerSubsystem>() : nullptr;
	const bool bMobile = Platform && Platform->GetCapabilities().bIsMobile;
	if (bLoadVisuals)
	{
		Bundles.Add(AHEnemyAssets::VisualBundle);
		Bundles.Add(bMobile ? AHEnemyAssets::MobileBundle : AHEnemyAssets::DesktopBundle);
	}
	if (bLoadAudio)
	{
		Bundles.Add(AHEnemyAssets::AudioBundle);
		Bundles.Add(bMobile ? AHEnemyAssets::MobileAudioBundle : AHEnemyAssets::DesktopAudioBundle);
	}
	return Bundles;
}

void UAHEnemyAssetSubsystem::RecordLifecycle(const FString& Event)
{
	LifecycleHistory.Add(FString::Printf(TEXT("%.3f %s"), FPlatformTime::Seconds(), *Event));
	if (LifecycleHistory.Num() > 64)
	{
		LifecycleHistory.RemoveAt(0, LifecycleHistory.Num() - 64);
	}
}

void UAHEnemyAssetSubsystem::DumpEnemyDefinitions() const
{
#if !UE_BUILD_SHIPPING
	TArray<FPrimaryAssetId> EnemyIds;
	UAssetManager::Get().GetPrimaryAssetIdList(AHEnemyAssets::EnemyType, EnemyIds);
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets] AHEnemy definitions=%d"), EnemyIds.Num());
	for (const FPrimaryAssetId& EnemyId : EnemyIds)
	{
		const FResidentEnemyState* Resident = ResidentEnemies.Find(EnemyId);
		TArray<FName> Bundles;
		if (Resident) Resident->BundleReferenceCounts.GetKeys(Bundles);
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets] %s registered=%s loaded=%s refs=%d bundles=%s lifecycle=%s"),
			*EnemyId.ToString(), UAssetManager::Get().GetPrimaryAssetPath(EnemyId).IsValid() ? TEXT("yes") : TEXT("no"),
			UAssetManager::Get().GetPrimaryAssetObject(EnemyId) ? TEXT("yes") : TEXT("no"),
			Resident ? Resident->ReferenceCount : 0,
			*FString::JoinBy(Bundles, TEXT(","), [](FName Name) { return Name.ToString(); }),
			Resident ? *Resident->LastLifecycleEvent : TEXT("unrequested"));
	}
#endif
}

void UAHEnemyAssetSubsystem::DumpLoadedAssets() const
{
#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets] requests=%d resident_enemies=%d outstanding_handles=%d"),
		Requests.Num(), ResidentEnemies.Num(), GetOutstandingLoadCount());
	for (const TPair<FGuid, FEnemyAssetRequestState>& Pair : Requests)
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets] request=%s owner=%s status=%s encounter=%s enemies=%d bundles=%s"),
			*Pair.Key.ToString(EGuidFormats::DigitsWithHyphensLower), *Pair.Value.LifecycleOwner.ToString(),
			Pair.Value.Status == EAHEnemyAssetRequestStatus::Ready ? TEXT("ready") : TEXT("pending"),
			*Pair.Value.EncounterDefinitionId.ToString(), Pair.Value.EnemyIds.Num(),
			*FString::JoinBy(Pair.Value.Bundles, TEXT(","), [](FName Name) { return Name.ToString(); }));
	}
	for (const FString& Event : LifecycleHistory)
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Assets] lifecycle %s"), *Event);
	}
#endif
}

#if !UE_BUILD_SHIPPING
namespace
{
	void ForEachEnemyAssetSubsystem(TFunctionRef<void(UAHEnemyAssetSubsystem&)> Callback)
	{
		if (!GEngine) return;
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UGameInstance* GameInstance = Context.OwningGameInstance;
			if (UAHEnemyAssetSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UAHEnemyAssetSubsystem>() : nullptr)
			{
				Callback(*Subsystem);
			}
		}
	}

	FAutoConsoleCommand DumpEnemiesCommand(
		TEXT("ah.Assets.DumpEnemies"),
		TEXT("Lists registered AHEnemy definitions and their bundle/ref-count lifecycle."),
		FConsoleCommandDelegate::CreateStatic([]
		{
			ForEachEnemyAssetSubsystem([](UAHEnemyAssetSubsystem& Subsystem) { Subsystem.DumpEnemyDefinitions(); });
		}));

	FAutoConsoleCommand DumpLoadedCommand(
		TEXT("ah.Assets.DumpLoaded"),
		TEXT("Lists enemy asset requests, outstanding handles, bundles, and recent lifecycle events."),
		FConsoleCommandDelegate::CreateStatic([]
		{
			ForEachEnemyAssetSubsystem([](UAHEnemyAssetSubsystem& Subsystem) { Subsystem.DumpLoadedAssets(); });
		}));
}
#endif
