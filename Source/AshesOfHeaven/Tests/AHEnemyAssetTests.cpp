#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Enemies/AHEnemyAssetSubsystem.h"
#include "Gameplay/Enemies/AHEncounterDefinition.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Animation/AnimSequenceBase.h"
#include "Materials/MaterialInterface.h"
#include "Misc/AutomationTest.h"
#include "Tests/AHEnemyAssetValidationCommandlet.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHEnemyBodyMaterialsAreProjectAssetsTest,
	"AshesOfHeaven.Assets.Enemies.BodyMaterialsAreProjectAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHEnemyBodyMaterialsAreProjectAssetsTest::RunTest(const FString& Parameters)
{
	// The mobile tier used to author enemy bodies to /Engine/BasicShapes/BasicShapeMaterial, which
	// UE substitutes with an engine default at runtime: an engine material presenting a character
	// on every mobile device. Surfaces must come from authored /Game materials on both tiers.
	const TArray<FString> DefinitionPaths = {
		TEXT("/Game/Ashes/Data/Enemies/DA_Enemy_Pilgrim.DA_Enemy_Pilgrim"),
		TEXT("/Game/Ashes/Data/Enemies/DA_Enemy_Hound.DA_Enemy_Hound")
	};
	for (const FString& DefinitionPath : DefinitionPaths)
	{
		UAHEnemyDefinition* Definition = LoadObject<UAHEnemyDefinition>(nullptr, *DefinitionPath);
		TestNotNull(*FString::Printf(TEXT("enemy definition resolves: %s"), *DefinitionPath), Definition);
		if (!Definition)
		{
			continue;
		}
		for (const bool bMobile : {false, true})
		{
			const FAHEnemyVisualPayload Payload = Definition->ResolveVisuals(bMobile);
			for (const TSoftObjectPtr<UMaterialInterface>& Material : Payload.Materials)
			{
				const FString MaterialPath = Material.ToSoftObjectPath().ToString();
				TestFalse(*FString::Printf(TEXT("%s %s body material is not an engine asset: %s"),
					*DefinitionPath, bMobile ? TEXT("mobile") : TEXT("desktop"), *MaterialPath),
					MaterialPath.StartsWith(TEXT("/Engine/")));
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHEnemyDeathsAvoidGenericFireVFXTest,
	"AshesOfHeaven.Assets.Enemies.DeathsAvoidGenericFireVFX",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHEnemyDeathsAvoidGenericFireVFXTest::RunTest(const FString& Parameters)
{
	const TArray<FString> Names = {TEXT("Pilgrim"), TEXT("Hound"), TEXT("Spider")};
	for (const FString& Name : Names)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/Ashes/Data/Enemies/DA_Enemy_%s.DA_Enemy_%s"), *Name, *Name);
		UAHEnemyDefinition* Definition = LoadObject<UAHEnemyDefinition>(nullptr, *Path);
		TestNotNull(*FString::Printf(TEXT("%s definition resolves"), *Name), Definition);
		if (!Definition)
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("%s desktop death remains a physical collapse"), *Name),
			Definition->ResolveVFX(false).DeathEffect.IsNull());
		TestTrue(*FString::Printf(TEXT("%s mobile death remains a physical collapse"), *Name),
			Definition->ResolveVFX(true).DeathEffect.IsNull());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHRosterWeaponAndLocomotionTest,
	"AshesOfHeaven.Assets.Enemies.RosterArmamentAndLocomotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHRosterWeaponAndLocomotionTest::RunTest(const FString& Parameters)
{
	// One archetype carries a rifle and the rest close and strike. That is an authored design
	// decision with nothing in the engine holding it in place - the loadout is a soft array in a
	// data asset, and re-running the authoring script with a stray "ranged" block would quietly
	// arm a hound. It is also invisible from a screenshot, because an enemy shooting from 30m
	// looks like an enemy shooting from 30m whichever archetype it is.
	const TArray<FString> Names = { TEXT("Pilgrim"), TEXT("Hound"), TEXT("Spider") };
	int32 Armed = 0;
	for (const FString& Name : Names)
	{
		const FString Path = FString::Printf(
			TEXT("/Game/Ashes/Data/Enemies/DA_Enemy_%s.DA_Enemy_%s"), *Name, *Name);
		UAHEnemyDefinition* Definition = LoadObject<UAHEnemyDefinition>(nullptr, *Path);
		TestNotNull(*FString::Printf(TEXT("%s definition resolves"), *Name), Definition);
		if (!Definition)
		{
			continue;
		}

		const bool bHasWeapon = !Definition->Loadout.WeaponClasses.IsEmpty();
		Armed += bHasWeapon ? 1 : 0;
		TestEqual(*FString::Printf(TEXT("%s is either armed or melee-only, never both or neither"), *Name),
			bHasWeapon, !Definition->AISettings.bMeleeOnly);

		// Every body needs somewhere to stand, somewhere to walk and somewhere to run. A missing
		// take is not a crash: AAHCombatantCharacter simply keeps playing the last clip, so a
		// creature charging at 640 cm/s in its idle loop is the only symptom.
		const FAHCreatureAnimationSet& Set = Definition->Visuals.Locomotion;
		TestFalse(*FString::Printf(TEXT("%s has an idle take"), *Name), Set.Idle.IsNull());
		TestFalse(*FString::Printf(TEXT("%s has a walk take"), *Name), Set.Walk.IsNull());
		TestFalse(*FString::Printf(TEXT("%s has a run take"), *Name), Set.Run.IsNull());
		TestFalse(*FString::Printf(TEXT("%s has a death take"), *Name), Set.Death.IsNull());
		if (Definition->AISettings.bMeleeOnly)
		{
			TestFalse(*FString::Printf(TEXT("%s bites with something the player can see"), *Name),
				Set.Attack.IsNull());
		}
		TestTrue(*FString::Printf(TEXT("%s run threshold sits above its walk threshold"), *Name),
			Set.RunSpeed > Set.WalkSpeed);
	}
	TestEqual(TEXT("exactly one archetype in the roster carries a weapon"), Armed, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHCreatureLocomotionStateTest,
	"AshesOfHeaven.Assets.Enemies.LocomotionStateHysteresis",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHCreatureLocomotionStateTest::RunTest(const FString& Parameters)
{
	using namespace AHCreatureLocomotion;

	FAHCreatureAnimationSet Set;
	Set.Idle = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Fake/Idle.Idle")));
	Set.Walk = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Fake/Walk.Walk")));
	Set.Run = TSoftObjectPtr<UAnimSequenceBase>(FSoftObjectPath(TEXT("/Game/Fake/Run.Run")));
	Set.WalkSpeed = 40.0f;
	Set.RunSpeed = 400.0f;

	TestEqual(TEXT("a standing body idles"),
		SelectLocomotionState(Set, 0.0f, EAHCreatureAnimState::Idle), EAHCreatureAnimState::Idle);
	TestEqual(TEXT("a body well past the walk threshold walks"),
		SelectLocomotionState(Set, 120.0f, EAHCreatureAnimState::Idle), EAHCreatureAnimState::Walk);
	TestEqual(TEXT("a body well past the run threshold runs"),
		SelectLocomotionState(Set, 600.0f, EAHCreatureAnimState::Walk), EAHCreatureAnimState::Run);

	// The point of the hysteresis: a body hovering on a threshold must not swap clip every frame.
	// 380 is under RunSpeed and over RunSpeed*0.8, so it holds whichever state it arrived in.
	TestEqual(TEXT("a running body holds its run just under the threshold"),
		SelectLocomotionState(Set, 380.0f, EAHCreatureAnimState::Run), EAHCreatureAnimState::Run);
	TestEqual(TEXT("a walking body does not break into a run at the same speed"),
		SelectLocomotionState(Set, 380.0f, EAHCreatureAnimState::Walk), EAHCreatureAnimState::Walk);
	TestEqual(TEXT("a walking body keeps walking just under the walk threshold"),
		SelectLocomotionState(Set, 36.0f, EAHCreatureAnimState::Walk), EAHCreatureAnimState::Walk);
	TestEqual(TEXT("a standing body does not start walking at the same speed"),
		SelectLocomotionState(Set, 36.0f, EAHCreatureAnimState::Idle), EAHCreatureAnimState::Idle);

	// A body with one movement take uses it at every speed rather than sliding in its idle.
	FAHCreatureAnimationSet RunOnly;
	RunOnly.Run = Set.Run;
	RunOnly.WalkSpeed = 40.0f;
	RunOnly.RunSpeed = 400.0f;
	TestEqual(TEXT("a body with only a run take uses it at walking speed"),
		SelectLocomotionState(RunOnly, 120.0f, EAHCreatureAnimState::Idle), EAHCreatureAnimState::Run);

	TestEqual(TEXT("walk cadence is native in the middle of the authored walk band"),
		CalculateLocomotionPlayRate(Set, 300.0f, 640.0f, EAHCreatureAnimState::Walk), 1.0f);
	TestEqual(TEXT("run cadence is native at maximum movement speed"),
		CalculateLocomotionPlayRate(Set, 640.0f, 640.0f, EAHCreatureAnimState::Run), 1.0f);
	TestEqual(TEXT("idle never inherits a locomotion rate"),
		CalculateLocomotionPlayRate(Set, 0.0f, 640.0f, EAHCreatureAnimState::Idle), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHEnemyAssetManifestTest,
	"AshesOfHeaven.Assets.Enemies.ManifestAndCookRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHEnemyAssetManifestTest::RunTest(const FString& Parameters)
{
	int32 ValidatedAssets = 0;
	TestTrue(TEXT("Every enemy and encounter manifest entry resolves and is cook-safe"),
		UAHEnemyAssetValidationCommandlet::ValidateEnemyAssetManifest(*GLog, ValidatedAssets));
	TestTrue(TEXT("Pilgrim, the beasts and their encounter manifests were validated"), ValidatedAssets >= 4);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHEncounterSpawnSequenceTest,
	"AshesOfHeaven.Assets.Enemies.SpawnSequenceMixesRoster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHEncounterSpawnSequenceTest::RunTest(const FString& Parameters)
{
	UAHEncounterDefinition* Definition = NewObject<UAHEncounterDefinition>();
	Definition->EncounterId = TEXT("SpawnSequenceTest");
	Definition->DeterministicSeed = 4242;
	Definition->PrimaryEnemy = AHEnemyAssets::EnemyId(TEXT("Pilgrim"));
	Definition->AdditionalEnemies = {
		AHEnemyAssets::EnemyId(TEXT("Spider")),
		AHEnemyAssets::EnemyId(TEXT("Hound")),
	};

	TArray<FPrimaryAssetId> Sequence;
	Definition->BuildSpawnSequence(8, Sequence);
	TestEqual(TEXT("Sequence fills the requested count"), Sequence.Num(), 8);
	for (const FPrimaryAssetId& Expected : Definition->AdditionalEnemies)
	{
		TestTrue(TEXT("Every listed archetype appears at least once"), Sequence.Contains(Expected));
	}
	TestTrue(TEXT("The primary archetype still appears"), Sequence.Contains(Definition->PrimaryEnemy));

	// A checkpoint restores an encounter by rebuilding its draws, so the same inputs have to
	// produce the same line-up.
	TArray<FPrimaryAssetId> Repeat;
	Definition->BuildSpawnSequence(8, Repeat);
	TestTrue(TEXT("The same seed rebuilds the same line-up"), Sequence == Repeat);

	// Different encounters carry different seeds; that is what makes two fights differ.
	Definition->DeterministicSeed = 99;
	TArray<FPrimaryAssetId> Reseeded;
	Definition->BuildSpawnSequence(8, Reseeded);
	TestTrue(TEXT("A different seed draws a different line-up"), Sequence != Reseeded);

	// A single-archetype encounter must stay exactly what it was authored as.
	Definition->AdditionalEnemies.Reset();
	TArray<FPrimaryAssetId> SoloRoster;
	Definition->BuildSpawnSequence(4, SoloRoster);
	TestEqual(TEXT("A lone archetype fills every slot"), SoloRoster.Num(), 4);
	for (const FPrimaryAssetId& EnemyId : SoloRoster)
	{
		TestEqual(TEXT("A lone archetype is the only thing spawned"), EnemyId, Definition->PrimaryEnemy);
	}
	return true;
}

namespace AHEnemyAssetTests
{
	struct FAsyncState
	{
		FAutomationTestBase* Test = nullptr;
		TObjectPtr<UGameInstance> GameInstance = nullptr;
		TObjectPtr<UWorld> World = nullptr;
		TObjectPtr<UAHEnemyAssetSubsystem> Assets = nullptr;
		TObjectPtr<AAHCombatEncounter> Encounter = nullptr;
		FGuid CoreLease;
		FGuid VisualLease;
		FGuid EncounterLease;
		FGuid ChapterLease;
		int32 Stage = 0;
		int32 CompletedRepeatedRequests = 0;
		int32 LastDefinitionCount = 0;
		bool bLastCallback = false;
		bool bLastSuccess = false;
		FString LastError;
		double Deadline = 0.0;

		void ResetCallback()
		{
			bLastCallback = false;
			bLastSuccess = false;
			LastDefinitionCount = 0;
			LastError.Reset();
		}

		FAHEnemyAssetsReady MakeCallback()
		{
			return FAHEnemyAssetsReady::CreateLambda([this](FGuid, bool bSuccess, const TArray<UAHEnemyDefinition*>& Definitions, const FString& Error)
			{
				bLastCallback = true;
				bLastSuccess = bSuccess;
				LastDefinitionCount = Definitions.Num();
				LastError = Error;
			});
		}

		void Teardown()
		{
			if (Assets)
			{
				if (CoreLease.IsValid()) Assets->ReleaseEncounterAssets(CoreLease);
				if (VisualLease.IsValid()) Assets->ReleaseEncounterAssets(VisualLease);
				if (EncounterLease.IsValid()) Assets->ReleaseEncounterAssets(EncounterLease);
				if (ChapterLease.IsValid()) Assets->ReleaseEncounterAssets(ChapterLease);
			}
			if (Encounter) Encounter->Destroy();
			if (World)
			{
				for (TActorIterator<AAHCombatantCharacter> It(World); It; ++It) It->Destroy();
				if (GEngine) GEngine->DestroyWorldContext(World);
				World->DestroyWorld(false);
				World = nullptr;
			}
			if (GameInstance)
			{
				GameInstance->Shutdown();
				GameInstance->RemoveFromRoot();
				GameInstance = nullptr;
			}
		}
	};

	// Per-stage budget for the streaming waits below. 15s was measured against a Visual bundle
	// that held one mesh, one physics asset and a single animation take per archetype; the
	// roster now carries five takes each plus 4K source maps, and stage 1 alone crossed 15s on
	// a machine with an editor open beside it. Generous rather than tight on purpose - this is
	// a timeout guarding against a hang, not an assertion about load speed, and a flaky failure
	// here reads as a broken asset and costs an afternoon.
	constexpr double StageTimeoutSeconds = 45.0;

	class FAsyncLifecycleCommand final : public IAutomationLatentCommand
	{
	public:
		explicit FAsyncLifecycleCommand(TSharedRef<FAsyncState> InState) : State(MoveTemp(InState)) {}

		virtual bool Update() override
		{
			FAsyncState& S = *State;
			auto TimedOut = [&S]()
			{
				if (FPlatformTime::Seconds() <= S.Deadline) return false;
				S.Test->AddError(FString::Printf(TEXT("enemy asset async stage %d timed out"), S.Stage));
				S.Teardown();
				return true;
			};

			switch (S.Stage)
			{
			case 0:
				S.GameInstance = NewObject<UGameInstance>(GEngine);
				S.GameInstance->AddToRoot();
				S.GameInstance->InitializeStandalone(FName(TEXT("AHEnemyAssetAsyncWorld")));
				S.World = S.GameInstance->GetWorld();
				S.Assets = S.GameInstance->GetSubsystem<UAHEnemyAssetSubsystem>();
				S.Test->TestNotNull(TEXT("standalone world exists"), S.World.Get());
				S.Test->TestNotNull(TEXT("enemy asset subsystem exists"), S.Assets.Get());
				if (!S.World || !S.Assets)
				{
					S.Teardown();
					return true;
				}
				S.World->InitializeActorsForPlay(FURL());
				if (AWorldSettings* Settings = S.World->GetWorldSettings()) Settings->NotifyBeginPlay();
				S.World->BeginPlay();
				S.CoreLease = S.Assets->PreloadEnemyAssets(
					{ AHEnemyAssets::EnemyId(TEXT("Pilgrim")) }, { AHEnemyAssets::CoreBundle }, TEXT("Test.Core"),
					FAHEnemyAssetsReady::CreateLambda([&S](FGuid, bool bSuccess, const TArray<UAHEnemyDefinition*>& Definitions, const FString& Error)
					{
						S.Test->TestTrue(TEXT("core async request succeeds"), bSuccess);
						S.Test->TestEqual(TEXT("core request returns Pilgrim"), Definitions.Num(), 1);
						if (!bSuccess) S.Test->AddError(Error);
						++S.CompletedRepeatedRequests;
					}));
				S.VisualLease = S.Assets->PreloadEnemyAssets(
					{ AHEnemyAssets::EnemyId(TEXT("Pilgrim")) },
					{ AHEnemyAssets::CoreBundle, AHEnemyAssets::VisualBundle, AHEnemyAssets::DesktopBundle }, TEXT("Test.Visual"),
					FAHEnemyAssetsReady::CreateLambda([&S](FGuid, bool bSuccess, const TArray<UAHEnemyDefinition*>& Definitions, const FString& Error)
					{
						S.Test->TestTrue(TEXT("repeated union-bundle request succeeds"), bSuccess);
						S.Test->TestEqual(TEXT("repeated request returns one definition"), Definitions.Num(), 1);
						if (!bSuccess) S.Test->AddError(Error);
						++S.CompletedRepeatedRequests;
					}));
				S.Stage = 1;
				S.Deadline = FPlatformTime::Seconds() + AHEnemyAssetTests::StageTimeoutSeconds;
				return false;

			case 1:
				if (S.CompletedRepeatedRequests < 2) return !TimedOut() ? false : true;
				S.Test->TestEqual(TEXT("repeated requests share one resident enemy"), S.Assets->GetResidentEnemyCount(), 1);
				S.Assets->ReleaseEncounterAssets(S.CoreLease);
				S.CoreLease.Invalidate();
				S.Test->TestEqual(TEXT("releasing one lease preserves the shared resident enemy"), S.Assets->GetResidentEnemyCount(), 1);
				S.Assets->ReleaseEncounterAssets(S.VisualLease);
				S.VisualLease.Invalidate();
				S.Test->TestEqual(TEXT("releasing the last lease unloads the enemy"), S.Assets->GetResidentEnemyCount(), 0);

				S.ResetCallback();
				S.Assets->PreloadEnemyAssets(
					{ AHEnemyAssets::EnemyId(TEXT("DefinitelyMissing")) }, { AHEnemyAssets::CoreBundle }, TEXT("Test.Missing"), S.MakeCallback());
				S.Test->TestTrue(TEXT("missing Primary Asset completes with an error"), S.bLastCallback && !S.bLastSuccess && !S.LastError.IsEmpty());

				S.ResetCallback();
				S.EncounterLease = S.Assets->PreloadEncounterAssets(
					AHEnemyAssets::EncounterId(TEXT("PilgrimHound")), TEXT("Test.EncounterPreload"), S.MakeCallback());
				S.Stage = 2;
				S.Deadline = FPlatformTime::Seconds() + AHEnemyAssetTests::StageTimeoutSeconds;
				return false;

			case 2:
				if (!S.bLastCallback) return !TimedOut() ? false : true;
				S.Test->TestTrue(TEXT("encounter prediction preload succeeds"), S.bLastSuccess);
				// The mixed encounter is the whole creature roster: Pilgrim, Hound, Spider.
				S.Test->TestEqual(TEXT("mixed encounter predicts the full roster"), S.LastDefinitionCount, 3);
				S.Assets->ReleaseEncounterAssets(S.EncounterLease);
				S.EncounterLease.Invalidate();
				S.Test->TestEqual(TEXT("encounter release drops both enemy references"), S.Assets->GetResidentEnemyCount(), 0);

				S.ResetCallback();
				S.CoreLease = S.Assets->PreloadEnemyAssets(
					{ AHEnemyAssets::EnemyId(TEXT("Hound")) }, { AHEnemyAssets::CoreBundle, AHEnemyAssets::VisualBundle },
					TEXT("Test.Cancel"), S.MakeCallback());
				{
					const bool bWasPending = S.Assets->HasRequest(S.CoreLease)
						&& S.Assets->GetRequestStatus(S.CoreLease) == EAHEnemyAssetRequestStatus::Pending;
					const FGuid CanceledLease = S.CoreLease;
					S.Assets->CancelAssetRequest(CanceledLease);
					S.CoreLease.Invalidate();
					S.Test->TestFalse(TEXT("canceled request is removed"), S.Assets->HasRequest(CanceledLease));
					if (bWasPending)
					{
						S.Test->TestTrue(TEXT("pending cancellation reports failure to its callback"), S.bLastCallback && !S.bLastSuccess);
					}
				}
				S.Test->TestEqual(TEXT("cancellation releases resident references"), S.Assets->GetResidentEnemyCount(), 0);

				S.ResetCallback();
				S.ChapterLease = S.Assets->PreloadChapterAssets(
					TEXT("ChapterOne"),
					{ AHEnemyAssets::EnemyId(TEXT("Pilgrim")), AHEnemyAssets::EnemyId(TEXT("Hound")) },
					true, true, S.MakeCallback());
				S.Stage = 3;
				S.Deadline = FPlatformTime::Seconds() + AHEnemyAssetTests::StageTimeoutSeconds;
				return false;

			case 3:
				if (!S.bLastCallback) return !TimedOut() ? false : true;
				S.Test->TestTrue(TEXT("chapter preload succeeds"), S.bLastSuccess);
				S.Test->TestEqual(TEXT("chapter preload keeps both definitions resident"), S.Assets->GetResidentEnemyCount(), 2);
				S.Assets->ReleaseEncounterAssets(S.ChapterLease);
				S.ChapterLease.Invalidate();

				S.Encounter = S.World->SpawnActorDeferred<AAHCombatEncounter>(
					AAHCombatEncounter::StaticClass(), FTransform::Identity, nullptr, nullptr,
					ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
				S.Encounter->EncounterId = TEXT("Test_AsyncEncounter");
				S.Encounter->EncounterDefinitionId = AHEnemyAssets::EncounterId(TEXT("PilgrimPatrol"));
				S.Encounter->EnemyCount = 2;
				S.Encounter->bActivateOnPlayerOverlap = false;
				S.Encounter->bPreloadOnBeginPlay = false;
				S.Encounter->SpawnLocations = { FVector::ZeroVector, FVector(600.0, 0.0, 0.0) };
				UGameplayStatics::FinishSpawningActor(S.Encounter, FTransform::Identity);
				S.Encounter->ActivateEncounter();
				S.Stage = 4;
				S.Deadline = FPlatformTime::Seconds() + AHEnemyAssetTests::StageTimeoutSeconds;
				return false;

			case 4:
				if (!S.Encounter || S.Encounter->GetActiveEnemyCount() != 2) return !TimedOut() ? false : true;
				S.Test->TestTrue(TEXT("encounter spawns only after its async lease is ready"), S.Encounter->IsActive());
				{
					TArray<AAHCombatantCharacter*> Spawned;
					for (TActorIterator<AAHCombatantCharacter> It(S.World); It; ++It) Spawned.Add(*It);
					S.Test->TestEqual(TEXT("encounter integration spawned the requested enemy count"), Spawned.Num(), 2);
					for (AAHCombatantCharacter* Enemy : Spawned)
					{
						Enemy->TakeDamage(100000.0f, FDamageEvent(), nullptr, nullptr);
					}
				}
				S.Test->TestTrue(TEXT("async-spawned encounter completes when its enemies die"), S.Encounter->IsComplete());
				S.Test->TestEqual(TEXT("completed encounter releases its asset lease"), S.Assets->GetActiveRequestCount(), 0);
				S.Teardown();
				return true;
			}
			return true;
		}

	private:
		TSharedRef<FAsyncState> State;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHEnemyAssetAsyncLifecycleTest,
	"AshesOfHeaven.Assets.Enemies.AsyncLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHEnemyAssetAsyncLifecycleTest::RunTest(const FString& Parameters)
{
	TSharedRef<AHEnemyAssetTests::FAsyncState> State = MakeShared<AHEnemyAssetTests::FAsyncState>();
	State->Test = this;
	ADD_LATENT_AUTOMATION_COMMAND(AHEnemyAssetTests::FAsyncLifecycleCommand(State));
	return true;
}

#endif
