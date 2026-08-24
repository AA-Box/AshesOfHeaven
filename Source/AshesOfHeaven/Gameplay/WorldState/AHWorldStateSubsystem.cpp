#include "Gameplay/WorldState/AHWorldStateSubsystem.h"

#include "AshesOfHeaven.h"
#include "Gameplay/WorldState/AHSavableActor.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Misc/Crc.h"
#include "UObject/ObjectKey.h"

void UAHWorldStateSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LevelAddedHandle = FWorldDelegates::LevelAddedToWorld.AddUObject(this, &UAHWorldStateSubsystem::HandleLevelAdded);
	LevelRemovedHandle = FWorldDelegates::LevelRemovedFromWorld.AddUObject(this, &UAHWorldStateSubsystem::HandleLevelRemoved);
	EnsurePlatformStateLoaded();
}

void UAHWorldStateSubsystem::EnsurePlatformStateLoaded()
{
	if (bPlatformStateLoaded)
	{
		return;
	}
	if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (const UAHPlatformSaveSubsystem* Save = GameInstance->GetSubsystem<UAHPlatformSaveSubsystem>())
		{
			FAHWorldStateSaveData LoadedState;
			if (Save->LoadWorldState(LoadedState))
			{
				ImportSaveData(LoadedState);
			}
			bPlatformStateLoaded = true;
		}
	}
}

void UAHWorldStateSubsystem::Deinitialize()
{
	FWorldDelegates::LevelAddedToWorld.Remove(LevelAddedHandle);
	FWorldDelegates::LevelRemovedFromWorld.Remove(LevelRemovedHandle);
	RegisteredActors.Reset();
	AppliedActorInstances.Reset();
	Super::Deinitialize();
}

void UAHWorldStateSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	EnsurePlatformStateLoaded();
	for (ULevel* Level : InWorld.GetLevels())
	{
		DiscoverLevel(Level);
	}
}

void UAHWorldStateSubsystem::HandleLevelAdded(ULevel* Level, UWorld* World)
{
	if (World == GetWorld())
	{
		DiscoverLevel(Level);
	}
}

void UAHWorldStateSubsystem::HandleLevelRemoved(ULevel* Level, UWorld* World)
{
	if (World == GetWorld())
	{
		CaptureLevel(Level);
	}
}

void UAHWorldStateSubsystem::DiscoverLevel(ULevel* Level)
{
	if (!Level)
	{
		return;
	}
	for (AActor* Actor : Level->Actors)
	{
		if (IsValid(Actor) && Actor->Implements<UAHSavableActor>())
		{
			RegisterSavableActor(Actor);
		}
	}
}

void UAHWorldStateSubsystem::CaptureLevel(ULevel* Level)
{
	if (!Level)
	{
		return;
	}
	for (AActor* Actor : Level->Actors)
	{
		if (IsValid(Actor) && Actor->Implements<UAHSavableActor>())
		{
			CaptureActor(Actor, false);
			const FGuid Id = IAHSavableActor::Execute_GetPersistentId(Actor);
			RegisteredActors.Remove(Id);
			DirtyActorIds.Remove(Id);
			AppliedActorInstances.Remove(FObjectKey(Actor));
		}
	}
}

void UAHWorldStateSubsystem::RegisterSavableActor(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->Implements<UAHSavableActor>())
	{
		return;
	}
	const FGuid PersistentId = IAHSavableActor::Execute_GetPersistentId(Actor);
	if (!PersistentId.IsValid())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[WorldState] actor=%s has no valid persistent ID and will not be saved"), *Actor->GetPathName());
		return;
	}
	if (const TWeakObjectPtr<AActor>* Existing = RegisteredActors.Find(PersistentId))
	{
		if (Existing->IsValid() && Existing->Get() != Actor)
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("[WorldState] duplicate persistent ID=%s actors=%s and %s"),
				*PersistentId.ToString(), *Existing->Get()->GetPathName(), *Actor->GetPathName());
			return;
		}
	}
	RegisteredActors.Add(PersistentId, Actor);
	const FObjectKey ActorKey(Actor);
	if (AppliedActorInstances.Contains(ActorKey))
	{
		return;
	}
	AppliedActorInstances.Add(ActorKey);
	if (const FAHWorldActorState* Record = ActorStates.Find(PersistentId))
	{
		ApplyStateToActor(Actor, *Record);
	}
}

void UAHWorldStateSubsystem::MarkActorDirty(AActor* Actor)
{
	if (!IsValid(Actor) || !Actor->Implements<UAHSavableActor>())
	{
		return;
	}
	const FGuid PersistentId = IAHSavableActor::Execute_GetPersistentId(Actor);
	if (PersistentId.IsValid())
	{
		RegisteredActors.Add(PersistentId, Actor);
		DirtyActorIds.Add(PersistentId);
	}
}

void UAHWorldStateSubsystem::MarkActorDestroyed(AActor* Actor)
{
	if (CaptureActor(Actor, true))
	{
		const FGuid PersistentId = IAHSavableActor::Execute_GetPersistentId(Actor);
		DirtyActorIds.Remove(PersistentId);
		RegisteredActors.Remove(PersistentId);
	}
}

void UAHWorldStateSubsystem::CaptureDirtyActors()
{
	const TArray<FGuid> PendingIds = DirtyActorIds.Array();
	for (const FGuid& PersistentId : PendingIds)
	{
		const TWeakObjectPtr<AActor>* Actor = RegisteredActors.Find(PersistentId);
		if (Actor && Actor->IsValid())
		{
			CaptureActor(Actor->Get(), false);
		}
	}
	DirtyActorIds.Reset();
}

bool UAHWorldStateSubsystem::CaptureActor(AActor* Actor, bool bDestroyedOrConsumed)
{
	if (!IsValid(Actor) || !Actor->Implements<UAHSavableActor>())
	{
		return false;
	}
	const FGuid PersistentId = IAHSavableActor::Execute_GetPersistentId(Actor);
	if (!PersistentId.IsValid())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[WorldState] cannot capture actor=%s without a persistent ID"), *Actor->GetPathName());
		return false;
	}

	FAHWorldActorState Record;
	Record.PersistentId = PersistentId;
	Record.ActorClass = FSoftClassPath(Actor->GetClass());
	Record.LevelPackageName = GetActorLevelPackageName(Actor);
	Record.StateVersion = FMath::Max(0, IAHSavableActor::Execute_GetWorldStateVersion(Actor));
	Record.SerializationMode = IAHSavableActor::Execute_GetWorldStateSerializationMode(Actor);
	Record.bHasTransform = IAHSavableActor::Execute_ShouldSaveWorldTransform(Actor);
	Record.Transform = Record.bHasTransform ? Actor->GetActorTransform() : FTransform::Identity;
	Record.bDestroyedOrConsumed = bDestroyedOrConsumed;

	if (!bDestroyedOrConsumed)
	{
		bool bCaptured = false;
		if (Record.SerializationMode == EAHWorldStateSerializationMode::ExplicitPayload)
		{
			bCaptured = IAHSavableActor::Execute_CaptureWorldState(Actor, Record.Payload);
		}
		else
		{
			FMemoryWriter Writer(Record.Payload, true);
			FObjectAndNameAsStringProxyArchive Archive(Writer, true);
			Archive.ArIsSaveGame = true;
			Actor->Serialize(Archive);
			bCaptured = !Archive.IsError();
		}
		if (!bCaptured)
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] actor=%s rejected state capture; prior record retained"), *Actor->GetPathName());
			return false;
		}
	}
	Record.PayloadCrc = ComputePayloadCrc(Record.Payload);
	ActorStates.Add(PersistentId, MoveTemp(Record));
	return true;
}

bool UAHWorldStateSubsystem::ApplyStateToActor(AActor* Actor, const FAHWorldActorState& Record)
{
	if (!Actor || Record.ActorClass.IsNull() || Record.ActorClass.ToString() != FSoftClassPath(Actor->GetClass()).ToString())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] class mismatch id=%s saved=%s live=%s; actor state skipped"),
			*Record.PersistentId.ToString(), *Record.ActorClass.ToString(), Actor ? *Actor->GetClass()->GetPathName() : TEXT("None"));
		return false;
	}
	if (!IsRecordPayloadValid(Record))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] corrupt payload id=%s skipped"), *Record.PersistentId.ToString());
		return false;
	}
	if (Record.bDestroyedOrConsumed)
	{
		Actor->Destroy();
		return true;
	}
	const int32 CurrentVersion = IAHSavableActor::Execute_GetWorldStateVersion(Actor);
	if (Record.StateVersion > CurrentVersion)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] newer actor state id=%s savedVersion=%d currentVersion=%d skipped"),
			*Record.PersistentId.ToString(), Record.StateVersion, CurrentVersion);
		return false;
	}
	if (Record.SerializationMode != IAHSavableActor::Execute_GetWorldStateSerializationMode(Actor))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] serialization mode mismatch id=%s skipped"), *Record.PersistentId.ToString());
		return false;
	}
	if (Record.bHasTransform && !Record.Transform.ContainsNaN())
	{
		Actor->SetActorTransform(Record.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	}

	bool bRestored = false;
	if (Record.SerializationMode == EAHWorldStateSerializationMode::ExplicitPayload)
	{
		bRestored = IAHSavableActor::Execute_RestoreWorldState(Actor, Record.Payload, Record.StateVersion);
	}
	else
	{
		TArray<uint8> MutablePayload = Record.Payload;
		FMemoryReader Reader(MutablePayload, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, true);
		Archive.ArIsSaveGame = true;
		Actor->Serialize(Archive);
		bRestored = !Archive.IsError();
	}
	if (bRestored)
	{
		IAHSavableActor::Execute_OnWorldStateRestored(Actor);
	}
	else
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] actor rejected state id=%s version=%d"), *Record.PersistentId.ToString(), Record.StateVersion);
	}
	return bRestored;
}

FAHWorldStateSaveData UAHWorldStateSubsystem::BuildSaveData()
{
	CaptureDirtyActors();
	FAHWorldStateSaveData Result;
	Result.SchemaVersion = AHWorldStateConstants::CurrentSchemaVersion;
	ActorStates.GenerateValueArray(Result.Actors);
	Result.Actors.Sort([](const FAHWorldActorState& Left, const FAHWorldActorState& Right)
	{
		return Left.PersistentId.ToString() < Right.PersistentId.ToString();
	});
	return Result;
}

void UAHWorldStateSubsystem::ImportSaveData(const FAHWorldStateSaveData& SaveData)
{
	ActorStates.Reset();
	if (SaveData.SchemaVersion > AHWorldStateConstants::CurrentSchemaVersion)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] save schema=%d is newer than supported=%d; optional world state ignored"),
			SaveData.SchemaVersion, AHWorldStateConstants::CurrentSchemaVersion);
		return;
	}
	for (const FAHWorldActorState& Record : SaveData.Actors)
	{
		if (!Record.PersistentId.IsValid() || Record.ActorClass.IsNull() || !IsRecordPayloadValid(Record))
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[WorldState] invalid optional actor record skipped id=%s"), *Record.PersistentId.ToString());
			continue;
		}
		ActorStates.Add(Record.PersistentId, Record);
	}
}

void UAHWorldStateSubsystem::ResetWorldState()
{
	ActorStates.Reset();
	DirtyActorIds.Reset();
	AppliedActorInstances.Reset();
	bPlatformStateLoaded = true;
	for (const TPair<FGuid, TWeakObjectPtr<AActor>>& Pair : RegisteredActors)
	{
		if (Pair.Value.IsValid())
		{
			AppliedActorInstances.Add(FObjectKey(Pair.Value.Get()));
		}
	}
}

const FAHWorldActorState* UAHWorldStateSubsystem::FindActorState(const FGuid& PersistentId) const
{
	return ActorStates.Find(PersistentId);
}

uint32 UAHWorldStateSubsystem::ComputePayloadCrc(const TArray<uint8>& Payload)
{
	return Payload.IsEmpty() ? 0u : FCrc::MemCrc32(Payload.GetData(), Payload.Num());
}

bool UAHWorldStateSubsystem::IsRecordPayloadValid(const FAHWorldActorState& Record)
{
	return Record.PayloadCrc == ComputePayloadCrc(Record.Payload);
}

FName UAHWorldStateSubsystem::GetActorLevelPackageName(const AActor* Actor)
{
	const ULevel* Level = Actor ? Actor->GetLevel() : nullptr;
	const UPackage* Package = Level ? Level->GetOutermost() : nullptr;
	return Package ? Package->GetFName() : NAME_None;
}
