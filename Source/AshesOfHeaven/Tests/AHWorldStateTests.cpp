#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Gameplay/WorldState/AHPersistentIdComponent.h"
#include "Gameplay/WorldState/AHWorldStateDoor.h"
#include "Gameplay/WorldState/AHWorldStateSubsystem.h"
#include "Gameplay/Weapons/AHWeaponPickup.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

namespace AHWorldStateTestSupport
{
	struct FWorldFixture
	{
		UWorld* World = nullptr;
		UAHWorldStateSubsystem* WorldState = nullptr;

		bool Create(bool bBeginPlay = true)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.InitializeScenes(false)
				.AllowAudioPlayback(false)
				.RequiresHitProxies(false)
				.CreatePhysicsScene(false)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.EnableTraceCollision(false)
				.SetTransactional(false)
				.CreateFXSystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values, false);
			WorldState = World ? World->GetSubsystem<UAHWorldStateSubsystem>() : nullptr;
			if (!World || !WorldState)
			{
				return false;
			}
			FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
			WorldContext.SetCurrentWorld(World);
			WorldState->ResetWorldState();
			if (bBeginPlay)
			{
				World->InitializeActorsForPlay(FURL());
				World->SetBegunPlay(true);
				World->BeginPlay();
			}
			return true;
		}

		void Destroy()
		{
			if (World)
			{
				World->BeginTearingDown();
				GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
				WorldState = nullptr;
			}
		}

		template <typename T>
		T* SpawnWithId(const FGuid& Id, const FTransform& Transform = FTransform::Identity)
		{
			T* Actor = World->SpawnActorDeferred<T>(T::StaticClass(), Transform);
			if (Actor)
			{
				Actor->SetPersistentId(Id);
				Actor->FinishSpawning(Transform);
				WorldState->RegisterSavableActor(Actor);
			}
			return Actor;
		}
	};

	template <typename T>
	FAHWorldActorState MakeRecord(const FGuid& Id, int32 Version, TArray<uint8> Payload)
	{
		FAHWorldActorState Record;
		Record.PersistentId = Id;
		Record.ActorClass = FSoftClassPath(T::StaticClass());
		Record.StateVersion = Version;
		Record.SerializationMode = EAHWorldStateSerializationMode::ExplicitPayload;
		Record.Payload = MoveTemp(Payload);
		Record.PayloadCrc = UAHWorldStateSubsystem::ComputePayloadCrc(Record.Payload);
		return Record;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateSaveLoadTest, "AshesOfHeaven.WorldState.SaveLoad", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateSaveLoadTest::RunTest(const FString& Parameters)
{
	const FGuid Id(0x10000001, 0x20000002, 0x30000003, 0x40000004);
	UAHSaveGame* Save = NewObject<UAHSaveGame>();
	Save->WorldState.Actors.Add(AHWorldStateTestSupport::MakeRecord<AAHWorldStateDoor>(Id, 1, { 1u }));
	TArray<uint8> Bytes;
	TestTrue(TEXT("world state save serializes"), UGameplayStatics::SaveGameToMemory(Save, Bytes));
	const UAHSaveGame* Loaded = Cast<UAHSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	TestNotNull(TEXT("world state save deserializes"), Loaded);
	if (Loaded)
	{
		TestEqual(TEXT("actor record count survives"), Loaded->WorldState.Actors.Num(), 1);
		TestEqual(TEXT("persistent ID survives"), Loaded->WorldState.Actors[0].PersistentId, Id);
		TestTrue(TEXT("payload survives"), Loaded->WorldState.Actors[0].Payload == TArray<uint8>({ 1u }));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateStableAndDuplicateIdTest, "AshesOfHeaven.WorldState.StableAndDuplicateIds", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateStableAndDuplicateIdTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create(false));
	const FGuid SharedId(0x20000001, 0x20000002, 0x20000003, 0x20000004);
	AAHWorldStateDoor* First = Fixture.World->SpawnActor<AAHWorldStateDoor>();
	AAHWorldStateDoor* Second = Fixture.World->SpawnActor<AAHWorldStateDoor>();
	First->SetPersistentId(SharedId);
	Second->SetPersistentId(SharedId);
	First->Rename(TEXT("RenamedDoor"));
	TestEqual(TEXT("identity is independent of actor name"), First->GetPersistentId_Implementation(), SharedId);
	const TArray<FGuid> Duplicates = UAHPersistentIdComponent::FindDuplicateIds({ First, Second });
	TestEqual(TEXT("one duplicate ID is detected"), Duplicates.Num(), 1);
	if (!Duplicates.IsEmpty())
	{
		TestEqual(TEXT("the duplicated GUID is reported"), Duplicates[0], SharedId);
	}
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateStreamedApplicationTest, "AshesOfHeaven.WorldState.StreamedActorApplication", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateStreamedApplicationTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create());
	const FGuid Id(0x30000001, 0x30000002, 0x30000003, 0x30000004);
	FAHWorldStateSaveData SaveData;
	SaveData.Actors.Add(MakeRecord<AAHWorldStateDoor>(Id, 1, { 1u }));
	Fixture.WorldState->ImportSaveData(SaveData);
	AAHWorldStateDoor* LateDoor = Fixture.SpawnWithId<AAHWorldStateDoor>(Id);
	TestNotNull(TEXT("late streamed actor spawns"), LateDoor);
	TestTrue(TEXT("saved state applies when actor appears"), LateDoor && LateDoor->IsDoorOpen());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateDestroyedActorTest, "AshesOfHeaven.WorldState.DestroyedOrConsumedActor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateDestroyedActorTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create());
	const FGuid Id(0x40000001, 0x40000002, 0x40000003, 0x40000004);
	FAHWorldActorState Tombstone = MakeRecord<AAHWeaponPickup>(Id, 1, {});
	Tombstone.bDestroyedOrConsumed = true;
	FAHWorldStateSaveData SaveData;
	SaveData.Actors.Add(Tombstone);
	Fixture.WorldState->ImportSaveData(SaveData);
	AAHWeaponPickup* Pickup = Fixture.SpawnWithId<AAHWeaponPickup>(Id);
	TestTrue(TEXT("consumed pickup is destroyed as soon as it reappears"), Pickup && Pickup->IsActorBeingDestroyed());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateTransformTest, "AshesOfHeaven.WorldState.Transform", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateTransformTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create());
	const FGuid Id(0x50000001, 0x50000002, 0x50000003, 0x50000004);
	const FTransform SavedTransform(FRotator(0.0f, 37.0f, 0.0f), FVector(123.0f, -456.0f, 78.0f), FVector(1.2f));
	AAHWorldStateDoor* Door = Fixture.SpawnWithId<AAHWorldStateDoor>(Id, SavedTransform);
	Door->SetDoorOpen(true);
	const FAHWorldStateSaveData Captured = Fixture.WorldState->BuildSaveData();
	TestEqual(TEXT("dirty actor is captured once"), Captured.Actors.Num(), 1);
	Fixture.Destroy();

	FWorldFixture RestoredFixture;
	TestTrue(TEXT("restore world creates"), RestoredFixture.Create());
	RestoredFixture.WorldState->ImportSaveData(Captured);
	AAHWorldStateDoor* RestoredDoor = RestoredFixture.SpawnWithId<AAHWorldStateDoor>(Id, FTransform::Identity);
	TestTrue(TEXT("door transform restores"), RestoredDoor && RestoredDoor->GetActorTransform().Equals(SavedTransform, 0.01f));
	TestTrue(TEXT("door explicit payload restores with transform"), RestoredDoor && RestoredDoor->IsDoorOpen());
	RestoredFixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateMissingActorTest, "AshesOfHeaven.WorldState.MissingActorPreserved", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateMissingActorTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create());
	const FGuid Id(0x60000001, 0x60000002, 0x60000003, 0x60000004);
	FAHWorldStateSaveData SaveData;
	SaveData.Actors.Add(MakeRecord<AAHWorldStateDoor>(Id, 1, { 1u }));
	Fixture.WorldState->ImportSaveData(SaveData);
	const FAHWorldStateSaveData Rewritten = Fixture.WorldState->BuildSaveData();
	TestEqual(TEXT("unloaded or missing actor record remains authoritative"), Rewritten.Actors.Num(), 1);
	TestEqual(TEXT("missing actor ID remains available for a future stream"), Rewritten.Actors[0].PersistentId, Id);
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateVersionAndCorruptionTest, "AshesOfHeaven.WorldState.VersionAndCorruptionIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateVersionAndCorruptionTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create());
	const FGuid FutureId(0x70000001, 0x70000002, 0x70000003, 0x70000004);
	const FGuid CorruptId(0x70000005, 0x70000006, 0x70000007, 0x70000008);
	FAHWorldActorState Future = MakeRecord<AAHWorldStateDoor>(FutureId, 99, { 1u });
	FAHWorldActorState Corrupt = MakeRecord<AAHWorldStateDoor>(CorruptId, 1, { 1u });
	Corrupt.PayloadCrc++;
	FAHWorldStateSaveData SaveData;
	SaveData.Actors = { Future, Corrupt };
	Fixture.WorldState->ImportSaveData(SaveData);
	AAHWorldStateDoor* FutureDoor = Fixture.SpawnWithId<AAHWorldStateDoor>(FutureId);
	AAHWorldStateDoor* CorruptDoor = Fixture.SpawnWithId<AAHWorldStateDoor>(CorruptId);
	TestFalse(TEXT("newer actor version falls back to authored default"), FutureDoor && FutureDoor->IsDoorOpen());
	TestFalse(TEXT("corrupt optional actor payload does not apply"), CorruptDoor && CorruptDoor->IsDoorOpen());
	TestNull(TEXT("corrupt record is discarded without affecting other state"), Fixture.WorldState->FindActorState(CorruptId));
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateOldSaveMigrationTest, "AshesOfHeaven.WorldState.OldSaveMigration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateOldSaveMigrationTest::RunTest(const FString& Parameters)
{
	UAHSaveGame* OldSave = NewObject<UAHSaveGame>();
	OldSave->SaveVersion = AHWorldStateConstants::FirstSaveVersion - 1;
	OldSave->CheckpointId = FName(TEXT("Ch01_Transit"));
	OldSave->WorldState.SchemaVersion = 0;
	OldSave->WorldState.Actors.Add(AHWorldStateTestSupport::MakeRecord<AAHWorldStateDoor>(FGuid::NewGuid(), 1, { 1u }));
	TestTrue(TEXT("legacy save reports a migration"), UAHPlatformSaveSubsystem::MigrateSaveObject(OldSave));
	TestEqual(TEXT("global save version advances"), OldSave->SaveVersion, AHChapterStateConstants::CurrentSaveVersion);
	TestEqual(TEXT("world-state schema defaults to current"), OldSave->WorldState.SchemaVersion, AHWorldStateConstants::CurrentSchemaVersion);
	TestTrue(TEXT("legacy checkpoint is preserved"), OldSave->CheckpointId == FName(TEXT("Ch01_Transit")));
	TestTrue(TEXT("pre-world-state actor data safely defaults empty"), OldSave->WorldState.Actors.IsEmpty());

	AHWorldStateTestSupport::FWorldFixture Fixture;
	TestTrue(TEXT("migration test world creates"), Fixture.Create(false));
	AAHChapterTerminal* Terminal = Fixture.World ? Fixture.World->SpawnActor<AAHChapterTerminal>() : nullptr;
	TestNotNull(TEXT("legacy terminal actor spawns"), Terminal);
	if (Terminal)
	{
		TestTrue(TEXT("terminal version zero confirmed state migrates"), Terminal->RestoreWorldState_Implementation({ 1u }, 0));
		TestTrue(TEXT("legacy terminal confirmation is preserved"), Terminal->IsConfirmed() && Terminal->IsInspected());
	}
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHWorldStateResetTest, "AshesOfHeaven.WorldState.ResetProgress", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHWorldStateResetTest::RunTest(const FString& Parameters)
{
	using namespace AHWorldStateTestSupport;
	FWorldFixture Fixture;
	TestTrue(TEXT("test world creates"), Fixture.Create());
	FAHWorldStateSaveData SaveData;
	SaveData.Actors.Add(MakeRecord<AAHWorldStateDoor>(FGuid::NewGuid(), 1, { 1u }));
	Fixture.WorldState->ImportSaveData(SaveData);
	Fixture.WorldState->ResetWorldState();
	TestTrue(TEXT("new expedition clears authoritative actor records"), Fixture.WorldState->BuildSaveData().Actors.IsEmpty());
	TestEqual(TEXT("new expedition clears dirty actors"), Fixture.WorldState->GetDirtyActorCount(), 0);
	Fixture.Destroy();
	return true;
}

#endif
