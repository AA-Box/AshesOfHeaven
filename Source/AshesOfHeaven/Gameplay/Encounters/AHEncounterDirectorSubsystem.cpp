#include "Gameplay/Encounters/AHEncounterDirectorSubsystem.h"

#include "AshesOfHeaven.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Enemies/AHEncounterDefinition.h"
#include "Gameplay/Enemies/AHEnemyAssetSubsystem.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "AI/NavigationSystemBase.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/StreamableManager.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
#if !UE_BUILD_SHIPPING
	int32 GAHEncounterDebug = 0;
	int32 GAHEncounterNoSpawn = 0;

	FAutoConsoleVariableRef EncounterDebugCVar(
		TEXT("ah.Encounter.Debug"),
		GAHEncounterDebug,
		TEXT("Displays the active authored encounter state."),
		ECVF_Cheat);

	FAutoConsoleVariableRef EncounterNoSpawnCVar(
		TEXT("ah.Encounter.NoSpawn"),
		GAHEncounterNoSpawn,
		TEXT("Prevents new encounter combatants from spawning while preserving director state."),
		ECVF_Cheat);

	FAutoConsoleCommandWithWorldAndArgs EncounterForceCommand(
		TEXT("ah.Encounter.Force"),
		TEXT("Begin an authored encounter by ID. Usage: ah.Encounter.Force <EncounterId>"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World && !Args.IsEmpty())
			{
				World->GetSubsystem<UAHEncounterDirectorSubsystem>()->BeginEncounter(FName(*Args[0]));
			}
		}));

	FAutoConsoleCommandWithWorldAndArgs EncounterDumpCommand(
		TEXT("ah.Encounter.Dump"),
		TEXT("Writes the active authored encounter state to the log."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (World)
			{
				World->GetSubsystem<UAHEncounterDirectorSubsystem>()->DumpToLog();
			}
		}));
#endif

	constexpr float SpawnRetryDelay = 0.75f;
	constexpr float SpawnCadence = 0.20f;
}

void UAHEncounterDirectorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Collection.InitializeDependency<UAHObjectiveSubsystem>();
	if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
	{
		Objectives->OnObjectiveCompleted.AddDynamic(this, &UAHEncounterDirectorSubsystem::HandleObjectiveCompleted);
	}
}

void UAHEncounterDirectorSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
		{
			Objectives->OnObjectiveCompleted.RemoveAll(this);
		}
	}
	ResetRuntime(true);
	Super::Deinitialize();
}

TStatId UAHEncounterDirectorSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAHEncounterDirectorSubsystem, STATGROUP_Tickables);
}

bool UAHEncounterDirectorSubsystem::IsTickable() const
{
	return !HasAnyFlags(RF_ClassDefaultObject) && GetWorld() != nullptr;
}

void UAHEncounterDirectorSubsystem::Tick(float DeltaTime)
{
#if !UE_BUILD_SHIPPING
	if (GAHEncounterDebug != 0 && GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0x0A4E7C01, 0.0f, FColor::Cyan, BuildDebugString());
	}
#endif
	if (!ActiveDefinition || bCompletingObjective)
	{
		return;
	}

	const float RemainingBudget = FMath::Max(0.0f, EffectiveBudget - TotalSpent);
	if (ActiveDefinition->CreditRegenerationPerSecond > 0.0f && RemainingBudget > CurrentCredits)
	{
		CurrentCredits = FMath::Min(RemainingBudget, CurrentCredits + ActiveDefinition->CreditRegenerationPerSecond * FMath::Max(0.0f, DeltaTime));
	}

	EvaluatePhaseProgress();
	if (!ActiveDefinition || bNextPhaseArmed || bQueryPending || bRosterLoadPending || PendingLoadHandle.IsValid())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	if (GAHEncounterNoSpawn != 0)
	{
		return;
	}
#endif
	if (GetWorld()->GetTimeSeconds() >= NextSpawnAttemptTime)
	{
		TryDispatchSpawn();
	}
}

bool UAHEncounterDirectorSubsystem::BeginEncounter(FName EncounterId)
{
	if (EncounterId.IsNone() || !GetWorld())
	{
		return false;
	}
	if (const UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		if (Checkpoints->IsEncounterCompleted(EncounterId))
		{
			return true;
		}
	}
	if ((ActiveDefinition && ActiveDefinition->EncounterId == EncounterId)
		|| (PendingEncounterId.IsValid() && PendingEncounterId.PrimaryAssetName == EncounterId))
	{
		return true;
	}

	if (ActiveDefinition || PendingEncounterId.IsValid())
	{
		AbortEncounter(FName(TEXT("Superseded")));
	}

	UAssetManager& AssetManager = UAssetManager::Get();
	const FPrimaryAssetId AssetId(AHEnemyAssets::EncounterType, EncounterId);
	if (!AssetManager.GetPrimaryAssetPath(AssetId).IsValid())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] unavailable encounter Primary Asset id=%s"), *AssetId.ToString());
		return false;
	}

	PendingEncounterId = AssetId;
	const int32 ThisRequest = ++RequestSerial;
	if (AssetManager.GetPrimaryAssetObject(AssetId))
	{
		HandleEncounterLoaded(AssetId, ThisRequest);
		return true;
	}

	PendingLoadHandle = AssetManager.LoadPrimaryAsset(
		AssetId,
		{},
		FStreamableDelegate::CreateUObject(this, &UAHEncounterDirectorSubsystem::HandleEncounterLoaded, AssetId, ThisRequest));
	if (!PendingLoadHandle.IsValid())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] failed to request encounter load id=%s"), *AssetId.ToString());
		PendingEncounterId = FPrimaryAssetId();
		return false;
	}

	UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector] load encounter=%s"), *EncounterId.ToString());
	return true;
}

FName UAHEncounterDirectorSubsystem::GetActiveEncounterId() const
{
	return ActiveDefinition ? ActiveDefinition->EncounterId : PendingEncounterId.PrimaryAssetName;
}

void UAHEncounterDirectorSubsystem::HandleEncounterLoaded(FPrimaryAssetId EncounterAssetId, int32 ThisRequest)
{
	if (ThisRequest != RequestSerial || PendingEncounterId != EncounterAssetId)
	{
		return;
	}
	PendingLoadHandle.Reset();
	ActiveDefinition = Cast<UAHEncounterDefinition>(UAssetManager::Get().GetPrimaryAssetObject(EncounterAssetId));
	if (!ActiveDefinition || ActiveDefinition->EncounterId.IsNone() || ActiveDefinition->Phases.IsEmpty())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] invalid or unavailable encounter asset=%s"), *EncounterAssetId.ToString());
		ResetRuntime(false);
		return;
	}

	ActiveDefinition->GetPredictedEnemySet(RequestedArchetypeIds);
	if (RequestedArchetypeIds.IsEmpty())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] encounter=%s has no archetypes"), *ActiveDefinition->EncounterId.ToString());
		ResetRuntime(false);
		return;
	}

	UGameInstance* GameInstance = GetWorld()->GetGameInstance();
	UAHEnemyAssetSubsystem* EnemyAssets = GameInstance ? GameInstance->GetSubsystem<UAHEnemyAssetSubsystem>() : nullptr;
	if (!EnemyAssets)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] enemy asset subsystem unavailable encounter=%s"), *ActiveDefinition->EncounterId.ToString());
		ResetRuntime(false);
		return;
	}
	bRosterLoadPending = true;
	const FGuid RequestedLeaseId = EnemyAssets->PreloadEncounterAssets(
		ActiveDefinition,
		FName(*FString::Printf(TEXT("Encounter.%s"), *ActiveDefinition->EncounterId.ToString())),
		FAHEnemyAssetsReady::CreateUObject(this, &UAHEncounterDirectorSubsystem::HandleRosterLoaded, ThisRequest));
	// A resident roster may invoke the callback synchronously. Do not resurrect a failed
	// request ID after HandleRosterLoaded has already reset the encounter.
	if (bRosterLoadPending && ThisRequest == RequestSerial)
	{
		EnemyAssetLeaseId = RequestedLeaseId;
	}
}

void UAHEncounterDirectorSubsystem::HandleRosterLoaded(
	FGuid RequestId,
	bool bSuccess,
	const TArray<UAHEnemyDefinition*>& Definitions,
	const FString& Error,
	int32 ThisRequest)
{
	if (ThisRequest != RequestSerial || !ActiveDefinition)
	{
		return;
	}
	EnemyAssetLeaseId = RequestId;
	bRosterLoadPending = false;
	if (!bSuccess)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] archetype preload failed encounter=%s error=%s"), *ActiveDefinition->EncounterId.ToString(), *Error);
		ResetRuntime(false);
		return;
	}
	LoadedArchetypes.Reset();
	for (UAHEnemyDefinition* Archetype : Definitions)
	{
		if (IsArchetypeAvailable(Archetype))
		{
			LoadedArchetypes.Add(Archetype->GetPrimaryAssetId(), Archetype);
		}
		else
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] unavailable archetype in streamed roster encounter=%s"), *ActiveDefinition->EncounterId.ToString());
		}
	}
	if (LoadedArchetypes.IsEmpty())
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] no usable archetypes encounter=%s"), *ActiveDefinition->EncounterId.ToString());
		ResetRuntime(false);
		return;
	}
	StartLoadedEncounter();
}

void UAHEncounterDirectorSubsystem::StartLoadedEncounter()
{
	PendingEncounterId = FPrimaryAssetId();
	ActiveDifficulty = ResolveDifficulty();
	const FAHEncounterDifficultyModifier& Modifier = ActiveDefinition->GetDifficultyModifier(ActiveDifficulty);
	EffectiveBudget = FMath::Max(0.0f, ActiveDefinition->EnemyBudget * Modifier.TacticalBudgetMultiplier);
	RandomStream.Initialize(ActiveDefinition->DeterministicSeed);
	RandomDrawCount = 0;
	CurrentCredits = FMath::Min(EffectiveBudget, ActiveDefinition->StartingCredits * Modifier.TacticalBudgetMultiplier);
	TotalSpent = 0.0f;
	TotalSpawned = 0;
	OpeningForceSize = 0;
	SpawnCounts.Reset();
	TriggeredScripts.Reset();

	const bool bRestore = PendingRestoreState.bValid && PendingRestoreState.EncounterId == ActiveDefinition->EncounterId;
	if (bRestore)
	{
		RandomStream.Initialize(PendingRestoreState.DeterministicSeed);
		AdvanceRandomStream(PendingRestoreState.RandomDrawCountAtPhaseStart);
		CurrentCredits = FMath::Clamp(PendingRestoreState.CreditsAtPhaseStart, 0.0f, EffectiveBudget);
		TotalSpent = FMath::Clamp(PendingRestoreState.TotalSpentBeforePhase, 0.0f, EffectiveBudget);
		TotalSpawned = FMath::Max(0, PendingRestoreState.TotalSpawnedBeforePhase);
		OpeningForceSize = FMath::Max(0, PendingRestoreState.OpeningForceSize);
		for (const FName TriggerId : PendingRestoreState.ScriptedTriggers)
		{
			TriggeredScripts.Add(TriggerId);
		}
		for (const FAHEncounterSpawnCount& Count : PendingRestoreState.SpawnCountsBeforePhase)
		{
			SpawnCounts.Add(Count.ArchetypeId, FMath::Max(0, Count.Count));
		}
		EnterPhase(FMath::Clamp(PendingRestoreState.PhaseIndex, 0, ActiveDefinition->Phases.Num() - 1), true);
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector] restore encounter=%s phase=%d seed=%d spent=%.1f draws=%d"),
			*ActiveDefinition->EncounterId.ToString(), CurrentPhaseIndex, PendingRestoreState.DeterministicSeed, TotalSpent, RandomDrawCount);
	}
	else
	{
		EnterPhase(0, false);
	}
	PendingRestoreState = FAHEncounterCheckpointState();
	OnEncounterStarted.Broadcast(ActiveDefinition->EncounterId);
}

void UAHEncounterDirectorSubsystem::EnterPhase(int32 PhaseIndex, bool bRestoring)
{
	if (!ActiveDefinition || !ActiveDefinition->Phases.IsValidIndex(PhaseIndex))
	{
		return;
	}
	CurrentPhaseIndex = PhaseIndex;
	PhaseSpawned = 0;
	bNextPhaseArmed = false;
	ArmedPhaseStartTime = 0.0f;
	FixedSpawnQueue.Reset();
	const FAHEncounterPhaseDefinition& Phase = ActiveDefinition->Phases[CurrentPhaseIndex];
	if (!bRestoring)
	{
		const float Bonus = Phase.BonusCredits * ActiveDefinition->GetDifficultyModifier(ActiveDifficulty).TacticalBudgetMultiplier;
		CurrentCredits = FMath::Min(FMath::Max(0.0f, EffectiveBudget - TotalSpent), CurrentCredits + Bonus);
	}
	PhaseStartCredits = CurrentCredits;
	PhaseStartSpent = TotalSpent;
	PhaseStartTotalSpawned = TotalSpawned;
	PhaseStartRandomDrawCount = RandomDrawCount;
	PhaseStartSpawnCounts = SpawnCounts;
	BuildFixedSpawnQueue(Phase);
	NextSpawnAttemptTime = GetWorld()->GetTimeSeconds();
	PersistPhaseBoundary();
	OnEncounterPhaseStarted.Broadcast(ActiveDefinition->EncounterId, Phase.PhaseId);
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector] phase encounter=%s index=%d id=%s credits=%.1f active=%d total=%d"),
		*ActiveDefinition->EncounterId.ToString(), CurrentPhaseIndex, *Phase.PhaseId.ToString(), CurrentCredits, ActiveEnemies.Num(), TotalSpawned);
}

void UAHEncounterDirectorSubsystem::BuildFixedSpawnQueue(const FAHEncounterPhaseDefinition& Phase)
{
	auto AppendSlots = [this](const TArray<FAHEncounterSpawnSlot>& Slots)
	{
		for (const FAHEncounterSpawnSlot& Slot : Slots)
		{
			for (int32 Index = 0; Index < FMath::Max(0, Slot.Count); ++Index)
			{
				FPendingSpawn& Request = FixedSpawnQueue.AddDefaulted_GetRef();
				Request.ArchetypeId = Slot.ArchetypeId;
				Request.AllowedRegions = Slot.AllowedRegions;
			}
		}
	};
	AppendSlots(Phase.FixedComposition);
	if (CurrentPhaseIndex == 0)
	{
		AppendSlots(ActiveDefinition->BossHeroSlots);
	}
}

void UAHEncounterDirectorSubsystem::TryDispatchSpawn()
{
	if (!ActiveDefinition || bQueryPending || ActiveEnemies.Num() >= ResolveActiveEnemyCap()
		|| TotalSpawned >= ActiveDefinition->MaximumTotalEnemies || IsCurrentPhasePlanComplete())
	{
		return;
	}

	FPendingSpawn Spawn;
	if (SelectNextSpawn(Spawn))
	{
		BeginSpawnQuery(Spawn);
	}
}

bool UAHEncounterDirectorSubsystem::SelectNextSpawn(FPendingSpawn& OutSpawn)
{
	if (!ActiveDefinition)
	{
		return false;
	}

	while (!FixedSpawnQueue.IsEmpty())
	{
		FPendingSpawn Candidate = FixedSpawnQueue[0];
		const TObjectPtr<UAHEnemyDefinition>* FoundArchetype = LoadedArchetypes.Find(Candidate.ArchetypeId);
		if (!FoundArchetype || !IsArchetypeAvailable(*FoundArchetype))
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] skip unavailable fixed archetype=%s"), *Candidate.ArchetypeId.ToString());
			FixedSpawnQueue.RemoveAt(0);
			continue;
		}

		Candidate.Cost = (*FoundArchetype)->Difficulty.ThreatCost;
		for (const FAHEncounterEnemyPoolEntry& Entry : ActiveDefinition->EnemyArchetypePool)
		{
			if (Entry.ArchetypeId == Candidate.ArchetypeId)
			{
				Candidate.Cost = CalculateSpawnCost(Entry, *FoundArchetype);
				break;
			}
		}
		if (Candidate.Cost > EffectiveBudget - TotalSpent + KINDA_SMALL_NUMBER)
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] skip permanently over-budget fixed archetype=%s cost=%.1f remaining=%.1f"),
				*Candidate.ArchetypeId.ToString(), Candidate.Cost, EffectiveBudget - TotalSpent);
			FixedSpawnQueue.RemoveAt(0);
			continue;
		}
		if (!CanSpend(CurrentCredits, TotalSpent, EffectiveBudget, Candidate.Cost))
		{
			return false;
		}
		OutSpawn = MoveTemp(Candidate);
		return true;
	}

	const FAHEncounterPhaseDefinition& Phase = ActiveDefinition->Phases[CurrentPhaseIndex];
	const int32 PhaseLimit = Phase.MaximumSpawns > 0 ? Phase.MaximumSpawns : PhaseSpawned;
	if (!Phase.bFillFromEnemyPool || PhaseSpawned >= PhaseLimit)
	{
		return false;
	}

	const int32 SelectedIndex = SelectWeightedPoolEntry(
		ActiveDefinition->EnemyArchetypePool,
		LoadedArchetypes,
		SpawnCounts,
		CurrentPhaseIndex,
		ActiveDifficulty,
		CurrentCredits,
		TotalSpent,
		EffectiveBudget,
		RandomStream);
	if (SelectedIndex == INDEX_NONE)
	{
		return false;
	}
	++RandomDrawCount;
	const FAHEncounterEnemyPoolEntry& Entry = ActiveDefinition->EnemyArchetypePool[SelectedIndex];
	OutSpawn.ArchetypeId = Entry.ArchetypeId;
	OutSpawn.Cost = CalculateSpawnCost(Entry, LoadedArchetypes.FindRef(Entry.ArchetypeId));
	OutSpawn.AllowedRegions = Phase.AllowedRegions;
	return true;
}

void UAHEncounterDirectorSubsystem::BeginSpawnQuery(const FPendingSpawn& Spawn)
{
	PendingSpawn = Spawn;
	LastSelectedArchetype = Spawn.ArchetypeId;
	if (!ActiveDefinition->SpawnQuery)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[EncounterDirector] missing spawn query encounter=%s"), *ActiveDefinition->EncounterId.ToString());
		NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnRetryDelay;
		return;
	}

	AActor* QueryOwner = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!QueryOwner)
	{
		NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnRetryDelay;
		return;
	}
	UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	if (!Platform || !Platform->TryAcquireEQSQuerySlot())
	{
		NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnRetryDelay;
		return;
	}

	bPlatformQuerySlotHeld = true;
	bQueryPending = true;
	const int32 ThisRequest = RequestSerial;
	FEnvQueryRequest QueryRequest(ActiveDefinition->SpawnQuery, QueryOwner);
	for (const TPair<FName, float>& Parameter : ActiveDefinition->SpawnQueryFloatParams)
	{
		QueryRequest.SetFloatParam(Parameter.Key, Parameter.Value);
	}
	QueryRequest.Execute(
		EEnvQueryRunMode::AllMatching,
		FQueryFinishedSignature::CreateUObject(this, &UAHEncounterDirectorSubsystem::HandleSpawnQueryFinished, ThisRequest));
}

void UAHEncounterDirectorSubsystem::HandleSpawnQueryFinished(TSharedPtr<FEnvQueryResult> Result, int32 ThisRequest)
{
	if (ThisRequest != RequestSerial || !ActiveDefinition)
	{
		return;
	}
	bQueryPending = false;
	if (bPlatformQuerySlotHeld)
	{
		if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
		{
			Platform->ReleaseEQSQuerySlot();
		}
		bPlatformQuerySlotHeld = false;
	}
	TArray<FVector> Locations;
	const bool bSuccessful = Result.IsValid() && Result->IsSuccessful();
	if (bSuccessful)
	{
		Result->GetAllAsLocations(Locations);
	}
	if (!IsQueryResultUsable(bSuccessful, Locations.Num()))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[EncounterDirector] invalid EQS result encounter=%s selected=%s valid=%s successful=%s items=%d"),
			*ActiveDefinition->EncounterId.ToString(), *PendingSpawn.ArchetypeId.ToString(), Result.IsValid() ? TEXT("true") : TEXT("false"),
			bSuccessful ? TEXT("true") : TEXT("false"), Locations.Num());
		NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnRetryDelay;
		return;
	}

	TArray<FVector> SafeLocations;
	for (const FVector& Location : Locations)
	{
		FVector SpawnLocation;
		if (TryResolveSafeSpawnLocation(Location, PendingSpawn.AllowedRegions, SpawnLocation))
		{
			SafeLocations.Add(SpawnLocation);
		}
	}
	if (SafeLocations.IsEmpty())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[EncounterDirector] EQS returned no approved safe location encounter=%s candidates=%d"),
			*ActiveDefinition->EncounterId.ToString(), Locations.Num());
		NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnRetryDelay;
		return;
	}

	PendingSpawnLocation = SafeLocations[DrawRandomIndex(SafeLocations.Num() - 1)];
	if (!SpawnSelectedEnemy(PendingSpawnLocation))
	{
		NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnRetryDelay;
	}
}

const FAHEncounterSpawnRegion* UAHEncounterDirectorSubsystem::FindContainingRegion(const FVector& Location, const TArray<FName>& RegionRestriction) const
{
	if (!ActiveDefinition || !ActiveDefinition->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return nullptr;
	}
	const FAHEncounterPhaseDefinition& Phase = ActiveDefinition->Phases[CurrentPhaseIndex];
	const TArray<FName>& EffectiveRegions = !RegionRestriction.IsEmpty() ? RegionRestriction : Phase.AllowedRegions;
	const int32 EffectiveDirections = ActiveDefinition->AllowedDirections & Phase.AllowedDirections;
	for (const FAHEncounterSpawnRegion& Region : ActiveDefinition->AllowedSpawnRegions)
	{
		const bool bRegionAllowed = EffectiveRegions.IsEmpty() || EffectiveRegions.Contains(Region.RegionId);
		const bool bDirectionAllowed = (EffectiveDirections & static_cast<int32>(Region.Direction)) != 0;
		if (bRegionAllowed && bDirectionAllowed && Region.Contains(Location))
		{
			return &Region;
		}
	}
	return nullptr;
}

bool UAHEncounterDirectorSubsystem::TryResolveSafeSpawnLocation(
	const FVector& QueryLocation,
	const TArray<FName>& RegionRestriction,
	FVector& OutSpawnLocation) const
{
	OutSpawnLocation = FVector::ZeroVector;
	if (!ActiveDefinition || !GetWorld() || QueryLocation.ContainsNaN() || !FindContainingRegion(QueryLocation, RegionRestriction))
	{
		return false;
	}
	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!Player || FVector::DistSquared(Player->GetActorLocation(), QueryLocation) < FMath::Square(ActiveDefinition->MinimumDistanceFromPlayer))
	{
		return false;
	}

	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	FNavLocation Projected;
	if (!Navigation || !Navigation->ProjectPointToNavigation(QueryLocation, Projected, FVector(120.0f, 120.0f, 250.0f))
		|| FVector::DistSquared(Projected.Location, QueryLocation) > FMath::Square(180.0f))
	{
		return false;
	}
	// EQS/nav locations lie on the walkable floor. Character origins sit at capsule center;
	// testing or spawning the raw point embeds the capsule in the floor and rejects every item.
	const FVector SpawnLocation = Projected.Location + FVector(0.0f, 0.0f, 96.0f);
	if (!FindContainingRegion(SpawnLocation, RegionRestriction))
	{
		return false;
	}
	const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Player->GetActorLocation(), Projected.Location, const_cast<APawn*>(Player));
	if (!Path || !Path->IsValid() || Path->IsPartial())
	{
		return false;
	}

	FCollisionQueryParams CollisionParams(SCENE_QUERY_STAT(AHEncounterSpawnCollision), false, Player);
	if (GetWorld()->OverlapBlockingTestByChannel(SpawnLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(42.0f, 96.0f), CollisionParams))
	{
		return false;
	}

	if (ActiveDefinition->LOSRestriction != EAHEncounterLOSRule::Any)
	{
		FHitResult VisibilityHit;
		FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(AHEncounterSpawnVisibility), true, Player);
		const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			VisibilityHit,
			Player->GetPawnViewLocation(),
			SpawnLocation + FVector(0.0f, 0.0f, 70.0f),
			ECC_Visibility,
			VisibilityParams);
		const bool bVisible = !bBlocked;
		if ((ActiveDefinition->LOSRestriction == EAHEncounterLOSRule::HiddenFromPlayer && bVisible)
			|| (ActiveDefinition->LOSRestriction == EAHEncounterLOSRule::VisibleToPlayer && !bVisible))
		{
			return false;
		}
	}
	OutSpawnLocation = SpawnLocation;
	return true;
}

bool UAHEncounterDirectorSubsystem::SpawnSelectedEnemy(const FVector& Location)
{
	UAHEnemyDefinition* Archetype = LoadedArchetypes.FindRef(PendingSpawn.ArchetypeId);
	if (!IsArchetypeAvailable(Archetype) || !CanSpend(CurrentCredits, TotalSpent, EffectiveBudget, PendingSpawn.Cost))
	{
		return false;
	}
	UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	if (!Platform || !Platform->TryRegisterActiveCombatant())
	{
		return false;
	}

	const APawn* Player = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	const FRotator Rotation = Player ? (Player->GetActorLocation() - Location).Rotation() : FRotator::ZeroRotator;
	FActorSpawnParameters Parameters;
	Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	AAHCombatantCharacter* Enemy = GetWorld()->SpawnActor<AAHCombatantCharacter>(Archetype->CombatClass.Get(), Location, Rotation, Parameters);
	if (!Enemy)
	{
		Platform->UnregisterActiveCombatant();
		return false;
	}

	Enemy->Tags.Add(FName(TEXT("AH.DirectedEncounter")));
	Enemy->Tags.Add(ActiveDefinition->EncounterId);
	Enemy->ApplyEnemyDefinition(Archetype);
	Enemy->OnCombatantDeath.AddDynamic(this, &UAHEncounterDirectorSubsystem::HandleEnemyDied);
	Enemy->OnDestroyed.AddDynamic(this, &UAHEncounterDirectorSubsystem::HandleEnemyDestroyed);
	ActiveEnemies.Add(Enemy);
	CurrentCredits = FMath::Max(0.0f, CurrentCredits - PendingSpawn.Cost);
	TotalSpent += PendingSpawn.Cost;
	++TotalSpawned;
	++PhaseSpawned;
	SpawnCounts.FindOrAdd(PendingSpawn.ArchetypeId)++;
	if (!FixedSpawnQueue.IsEmpty() && FixedSpawnQueue[0].ArchetypeId == PendingSpawn.ArchetypeId)
	{
		FixedSpawnQueue.RemoveAt(0);
	}
	ApplyEnemyTacticalProfile(Enemy, *Archetype);
	NextSpawnAttemptTime = GetWorld()->GetTimeSeconds() + SpawnCadence;
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector] spawn encounter=%s phase=%d archetype=%s cost=%.1f credits=%.1f active=%d total=%d location=%s"),
		*ActiveDefinition->EncounterId.ToString(), CurrentPhaseIndex, *PendingSpawn.ArchetypeId.ToString(), PendingSpawn.Cost,
		CurrentCredits, ActiveEnemies.Num(), TotalSpawned, *Enemy->GetActorLocation().ToCompactString());
	return true;
}

void UAHEncounterDirectorSubsystem::ApplyEnemyTacticalProfile(AAHCombatantCharacter* Enemy, const UAHEnemyDefinition& Archetype) const
{
	if (!Enemy)
	{
		return;
	}
	Enemy->SpawnDefaultController();
	if (AAHCombatAIController* AI = Cast<AAHCombatAIController>(Enemy->GetController()))
	{
		AI->ApplyEnemySettings(Archetype.AISettings);
		const float DifficultyMultiplier = ActiveDefinition
			? ActiveDefinition->GetDifficultyModifier(ActiveDifficulty).AISophisticationMultiplier
			: 1.0f;
		AI->ApplyEncounterSophistication(FMath::Max(0.1f, Archetype.Difficulty.AccuracyScale) * DifficultyMultiplier);
	}
}

void UAHEncounterDirectorSubsystem::EvaluatePhaseProgress()
{
	if (!ActiveDefinition || !ActiveDefinition->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return;
	}
	const bool bPlanComplete = IsCurrentPhasePlanComplete();
	if (CurrentPhaseIndex == 0 && bPlanComplete && OpeningForceSize == 0)
	{
		OpeningForceSize = FMath::Max(1, PhaseSpawned);
	}

	UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>();
	const bool bObjectiveComplete = Objectives && ActiveDefinition->ObjectiveId != NAME_None
		&& Objectives->GetCompletedObjectiveIds().Contains(ActiveDefinition->ObjectiveId);
	if (ShouldCompleteEncounter(*ActiveDefinition, CurrentPhaseIndex, bPlanComplete, ActiveEnemies.Num(), bObjectiveComplete, TriggeredScripts))
	{
		CompleteEncounter();
		return;
	}

	const int32 NextPhaseIndex = CurrentPhaseIndex + 1;
	if (!bPlanComplete || !ActiveDefinition->Phases.IsValidIndex(NextPhaseIndex))
	{
		return;
	}
	const FAHEncounterPhaseDefinition& NextPhase = ActiveDefinition->Phases[NextPhaseIndex];
	if (!bNextPhaseArmed && IsNextPhaseTriggerSatisfied(NextPhase))
	{
		const float Delay = (NextPhase.ReinforcementDelay > 0.0f ? NextPhase.ReinforcementDelay : ActiveDefinition->DefaultReinforcementDelay)
			* ActiveDefinition->GetDifficultyModifier(ActiveDifficulty).ReinforcementDelayMultiplier;
		bNextPhaseArmed = true;
		ArmedPhaseStartTime = GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, Delay);
	}
	if (bNextPhaseArmed && GetWorld()->GetTimeSeconds() >= ArmedPhaseStartTime)
	{
		EnterPhase(NextPhaseIndex, false);
	}
}

bool UAHEncounterDirectorSubsystem::IsNextPhaseTriggerSatisfied(const FAHEncounterPhaseDefinition& NextPhase) const
{
	switch (NextPhase.Trigger)
	{
	case EAHEncounterPhaseTrigger::Immediate:
		return true;
	case EAHEncounterPhaseTrigger::ForceRemainingRatio:
		return ShouldTriggerForceRemaining(ActiveEnemies.Num(), OpeningForceSize, NextPhase.ForceRemainingRatio);
	case EAHEncounterPhaseTrigger::PreviousPhaseCleared:
		return ActiveEnemies.IsEmpty();
	case EAHEncounterPhaseTrigger::ScriptedTrigger:
		return TriggeredScripts.Contains(NextPhase.ScriptedTriggerId);
	default:
		return false;
	}
}

bool UAHEncounterDirectorSubsystem::IsCurrentPhasePlanComplete() const
{
	if (!ActiveDefinition || !ActiveDefinition->Phases.IsValidIndex(CurrentPhaseIndex) || !FixedSpawnQueue.IsEmpty() || bQueryPending)
	{
		return false;
	}
	const FAHEncounterPhaseDefinition& Phase = ActiveDefinition->Phases[CurrentPhaseIndex];
	if (!Phase.bFillFromEnemyPool)
	{
		return true;
	}
	return Phase.MaximumSpawns <= 0 || PhaseSpawned >= Phase.MaximumSpawns
		|| TotalSpawned >= ActiveDefinition->MaximumTotalEnemies
		|| TotalSpent >= EffectiveBudget - KINDA_SMALL_NUMBER;
}

void UAHEncounterDirectorSubsystem::HandleEnemyDied()
{
	for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(ActiveEnemies[Index]) || ActiveEnemies[Index]->IsCombatantDead())
		{
			ActiveEnemies.RemoveAt(Index);
			ReleasePlatformSlot();
		}
	}
	EvaluatePhaseProgress();
}

void UAHEncounterDirectorSubsystem::HandleEnemyDestroyed(AActor* DestroyedActor)
{
	for (int32 Index = ActiveEnemies.Num() - 1; Index >= 0; --Index)
	{
		if (!IsValid(ActiveEnemies[Index]) || ActiveEnemies[Index].Get() == DestroyedActor)
		{
			ActiveEnemies.RemoveAt(Index);
			ReleasePlatformSlot();
		}
	}
	EvaluatePhaseProgress();
}

void UAHEncounterDirectorSubsystem::NotifyScriptedTrigger(FName TriggerId)
{
	if (!ActiveDefinition || TriggerId.IsNone() || (!ActiveDefinition->ScriptedTriggers.Contains(TriggerId)
		&& ActiveDefinition->CompletionTriggerId != TriggerId))
	{
		return;
	}
	TriggeredScripts.Add(TriggerId);
	EvaluatePhaseProgress();
}

void UAHEncounterDirectorSubsystem::ResolveEncounter()
{
	if (ActiveDefinition)
	{
		CompleteEncounter();
	}
}

void UAHEncounterDirectorSubsystem::HandleObjectiveCompleted(FName ObjectiveId)
{
	if (!ActiveDefinition || bCompletingObjective || ObjectiveId != ActiveDefinition->ObjectiveId)
	{
		return;
	}
	// The story owns objective completion. If it resolves the objective first, tactical execution
	// resolves immediately and cannot leak reinforcements into the next beat.
	bCompletingObjective = true;
	CompleteEncounter();
}

void UAHEncounterDirectorSubsystem::CompleteEncounter()
{
	if (!ActiveDefinition)
	{
		return;
	}
	const FName CompletedId = ActiveDefinition->EncounterId;
	const FName ObjectiveId = ActiveDefinition->ObjectiveId;
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		Checkpoints->MarkEncounterCompleted(CompletedId);
		Checkpoints->PersistEncounterState(FAHEncounterCheckpointState());
	}
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector] complete encounter=%s spent=%.1f total=%d"), *CompletedId.ToString(), TotalSpent, TotalSpawned);

	const bool bObjectiveAlreadyCompleting = bCompletingObjective;
	const bool bShouldCompleteObjective = !bObjectiveAlreadyCompleting && ObjectiveId != NAME_None;
	// Clear the tactical state before story delegates run. Chapter stage transitions are
	// synchronous; leaving this encounter active during CompleteObjective would make the
	// chapter director report a spurious StoryStageChanged abort after a valid completion.
	ResetRuntime(true);
	OnEncounterCompleted.Broadcast(CompletedId);
	if (bShouldCompleteObjective)
	{
		if (UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>())
		{
			if (Objectives->IsCurrentObjective(ObjectiveId))
			{
				Objectives->CompleteObjective(ObjectiveId);
			}
		}
	}
}

void UAHEncounterDirectorSubsystem::AbortEncounter(FName Reason)
{
	if (!ActiveDefinition && !PendingEncounterId.IsValid())
	{
		return;
	}
	const FName AbortedId = GetActiveEncounterId();
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector] abort encounter=%s reason=%s"), *AbortedId.ToString(), *Reason.ToString());
	OnEncounterAborted.Broadcast(AbortedId);
	ResetRuntime(true);
}

void UAHEncounterDirectorSubsystem::ResetRuntime(bool bDestroyActiveEnemies)
{
	++RequestSerial;
	PendingLoadHandle.Reset();
	if (EnemyAssetLeaseId.IsValid())
	{
		if (UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
		{
			if (UAHEnemyAssetSubsystem* EnemyAssets = GameInstance->GetSubsystem<UAHEnemyAssetSubsystem>())
			{
				if (EnemyAssets->HasRequest(EnemyAssetLeaseId))
				{
					if (EnemyAssets->GetRequestStatus(EnemyAssetLeaseId) == EAHEnemyAssetRequestStatus::Pending)
					{
						EnemyAssets->CancelAssetRequest(EnemyAssetLeaseId);
					}
					else
					{
						EnemyAssets->ReleaseEncounterAssets(EnemyAssetLeaseId);
					}
				}
			}
		}
		EnemyAssetLeaseId.Invalidate();
	}
	bRosterLoadPending = false;
	bQueryPending = false;
	if (bPlatformQuerySlotHeld)
	{
		if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
		{
			Platform->ReleaseEQSQuerySlot();
		}
		bPlatformQuerySlotHeld = false;
	}
	for (AAHCombatantCharacter* Enemy : ActiveEnemies)
	{
		if (IsValid(Enemy))
		{
			Enemy->OnCombatantDeath.RemoveAll(this);
			Enemy->OnDestroyed.RemoveAll(this);
			if (bDestroyActiveEnemies)
			{
				Enemy->Destroy();
			}
		}
		ReleasePlatformSlot();
	}
	ActiveEnemies.Reset();
	ActiveDefinition = nullptr;
	LoadedArchetypes.Reset();
	RequestedArchetypeIds.Reset();
	PendingEncounterId = FPrimaryAssetId();
	FixedSpawnQueue.Reset();
	PendingSpawn = FPendingSpawn();
	LastSelectedArchetype = FPrimaryAssetId();
	SpawnCounts.Reset();
	TriggeredScripts.Reset();
	CurrentPhaseIndex = INDEX_NONE;
	PhaseSpawned = 0;
	OpeningForceSize = 0;
	TotalSpawned = 0;
	RandomDrawCount = 0;
	CurrentCredits = 0.0f;
	TotalSpent = 0.0f;
	EffectiveBudget = 0.0f;
	bNextPhaseArmed = false;
	bCompletingObjective = false;
}

FAHEncounterCheckpointState UAHEncounterDirectorSubsystem::CaptureCheckpointState() const
{
	FAHEncounterCheckpointState State;
	if (!ActiveDefinition || !ActiveDefinition->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return State;
	}
	State.bValid = true;
	State.EncounterId = ActiveDefinition->EncounterId;
	State.PhaseIndex = CurrentPhaseIndex;
	State.DeterministicSeed = RandomStream.GetInitialSeed();
	State.RandomDrawCountAtPhaseStart = PhaseStartRandomDrawCount;
	State.CreditsAtPhaseStart = PhaseStartCredits;
	State.TotalSpentBeforePhase = PhaseStartSpent;
	State.TotalSpawnedBeforePhase = PhaseStartTotalSpawned;
	State.OpeningForceSize = OpeningForceSize;
	State.ScriptedTriggers = TriggeredScripts.Array();
	for (const TPair<FPrimaryAssetId, int32>& Pair : PhaseStartSpawnCounts)
	{
		FAHEncounterSpawnCount& Count = State.SpawnCountsBeforePhase.AddDefaulted_GetRef();
		Count.ArchetypeId = Pair.Key;
		Count.Count = Pair.Value;
	}
	return State;
}

void UAHEncounterDirectorSubsystem::RestoreCheckpointState(const FAHEncounterCheckpointState& State)
{
	if (!State.bValid || State.EncounterId.IsNone())
	{
		return;
	}
	PendingRestoreState = State;
	if (ActiveDefinition && ActiveDefinition->EncounterId == State.EncounterId)
	{
		ResetRuntime(true);
		PendingRestoreState = State;
	}
	BeginEncounter(State.EncounterId);
}

bool UAHEncounterDirectorSubsystem::IsActiveEncounterForStage(EAHChapterStage Stage) const
{
	return ActiveDefinition && ActiveDefinition->Stage == Stage;
}

void UAHEncounterDirectorSubsystem::PersistPhaseBoundary()
{
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		Checkpoints->PersistEncounterState(CaptureCheckpointState());
	}
}

void UAHEncounterDirectorSubsystem::AdvanceRandomStream(int32 DrawCount)
{
	RandomDrawCount = 0;
	for (int32 Index = 0; Index < FMath::Max(0, DrawCount); ++Index)
	{
		RandomStream.FRand();
		++RandomDrawCount;
	}
}

int32 UAHEncounterDirectorSubsystem::DrawRandomIndex(int32 MaximumInclusive)
{
	if (MaximumInclusive <= 0)
	{
		return 0;
	}
	++RandomDrawCount;
	return RandomStream.RandRange(0, MaximumInclusive);
}

EAHEncounterDifficulty UAHEncounterDirectorSubsystem::ResolveDifficulty() const
{
	const UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	const UAHPlatformSaveSubsystem* Saves = GameInstance ? GameInstance->GetSubsystem<UAHPlatformSaveSubsystem>() : nullptr;
	return DifficultyFromSaveValue(Saves ? Saves->GetDifficulty() : 1);
}

int32 UAHEncounterDirectorSubsystem::ResolveActiveEnemyCap() const
{
	if (!ActiveDefinition)
	{
		return 0;
	}
	const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	const int32 PlatformCap = Platform ? Platform->GetPerformanceProfile().MaxActiveCombatants : ActiveDefinition->MaximumActiveEnemies;
	const bool bMobile = Platform && Platform->GetCapabilities().bIsMobile;
	return CalculateActiveEnemyCap(*ActiveDefinition, ActiveDefinition->GetDifficultyModifier(ActiveDifficulty), PlatformCap, bMobile);
}

void UAHEncounterDirectorSubsystem::ReleasePlatformSlot()
{
	if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		Platform->UnregisterActiveCombatant();
	}
}

FString UAHEncounterDirectorSubsystem::BuildDebugString() const
{
	const FString PhaseId = ActiveDefinition && ActiveDefinition->Phases.IsValidIndex(CurrentPhaseIndex)
		? ActiveDefinition->Phases[CurrentPhaseIndex].PhaseId.ToString()
		: TEXT("none");
	const FString PendingState = PendingLoadHandle.IsValid() ? TEXT("encounter load")
		: bRosterLoadPending ? TEXT("roster load")
		: bQueryPending ? TEXT("EQS query")
		: TEXT("none");
	return FString::Printf(
		TEXT("Encounter: %s\nPhase: %d %s\nCredits: %.1f / %.1f (spent %.1f)\nActive/Total: %d / %d\nSelected: %s\nPending: %s"),
		*GetActiveEncounterId().ToString(), CurrentPhaseIndex, *PhaseId, CurrentCredits, EffectiveBudget, TotalSpent,
		ActiveEnemies.Num(), TotalSpawned, *LastSelectedArchetype.ToString(), *PendingState);
}

void UAHEncounterDirectorSubsystem::DumpToLog() const
{
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[EncounterDirector]\n%s"), *BuildDebugString());
}

EAHEncounterDifficulty UAHEncounterDirectorSubsystem::DifficultyFromSaveValue(int32 SaveValue)
{
	return static_cast<EAHEncounterDifficulty>(FMath::Clamp(SaveValue, 0, static_cast<int32>(EAHEncounterDifficulty::Damnation)));
}

float UAHEncounterDirectorSubsystem::CalculateSpawnCost(const FAHEncounterEnemyPoolEntry& Entry, const UAHEnemyDefinition* Archetype)
{
	return FMath::Max(0.0f, Entry.SpawnCostOverride >= 0.0f ? Entry.SpawnCostOverride : Archetype ? Archetype->Difficulty.ThreatCost : 0.0f);
}

float UAHEncounterDirectorSubsystem::CalculateEffectiveWeight(const FAHEncounterEnemyPoolEntry& Entry, EAHEncounterDifficulty Difficulty)
{
	float Multiplier = 1.0f;
	if (Difficulty == EAHEncounterDifficulty::Veteran)
	{
		Multiplier = Entry.VeteranWeightMultiplier;
	}
	else if (Difficulty == EAHEncounterDifficulty::Damnation)
	{
		Multiplier = Entry.DamnationWeightMultiplier;
	}
	return FMath::Max(0.0f, Entry.Weight * Multiplier);
}

bool UAHEncounterDirectorSubsystem::CanSpend(float Credits, float TotalSpentValue, float Budget, float Cost)
{
	return Cost >= 0.0f && Credits + KINDA_SMALL_NUMBER >= Cost && TotalSpentValue + Cost <= Budget + KINDA_SMALL_NUMBER;
}

int32 UAHEncounterDirectorSubsystem::SelectWeightedPoolEntry(
	const TArray<FAHEncounterEnemyPoolEntry>& Pool,
	const TMap<FPrimaryAssetId, TObjectPtr<UAHEnemyDefinition>>& Archetypes,
	const TMap<FPrimaryAssetId, int32>& Counts,
	int32 PhaseIndex,
	EAHEncounterDifficulty Difficulty,
	float Credits,
	float Spent,
	float Budget,
	FRandomStream& Stream)
{
	TArray<int32> EligibleIndices;
	TArray<float> EligibleWeights;
	float TotalWeight = 0.0f;
	for (int32 Index = 0; Index < Pool.Num(); ++Index)
	{
		const FAHEncounterEnemyPoolEntry& Entry = Pool[Index];
		const UAHEnemyDefinition* Archetype = Archetypes.FindRef(Entry.ArchetypeId);
		const int32 ExistingCount = Counts.FindRef(Entry.ArchetypeId);
		const float Cost = CalculateSpawnCost(Entry, Archetype);
		const float Weight = CalculateEffectiveWeight(Entry, Difficulty);
		if (PhaseIndex >= Entry.MinimumPhase && IsArchetypeAvailable(Archetype) && Weight > 0.0f
			&& (Entry.MaximumPerEncounter <= 0 || ExistingCount < Entry.MaximumPerEncounter)
			&& CanSpend(Credits, Spent, Budget, Cost))
		{
			EligibleIndices.Add(Index);
			EligibleWeights.Add(Weight);
			TotalWeight += Weight;
		}
	}
	if (EligibleIndices.IsEmpty() || TotalWeight <= 0.0f)
	{
		return INDEX_NONE;
	}

	const float Selection = Stream.FRand() * TotalWeight;
	float Accumulated = 0.0f;
	for (int32 EligibleIndex = 0; EligibleIndex < EligibleIndices.Num(); ++EligibleIndex)
	{
		Accumulated += EligibleWeights[EligibleIndex];
		if (Selection < Accumulated)
		{
			return EligibleIndices[EligibleIndex];
		}
	}
	return EligibleIndices.Last();
}

int32 UAHEncounterDirectorSubsystem::CalculateActiveEnemyCap(const UAHEncounterDefinition& Definition, const FAHEncounterDifficultyModifier& Modifier, int32 PlatformCap, bool bMobile)
{
	const int32 EncounterCap = FMath::Max(1, Definition.MaximumActiveEnemies + Modifier.MaximumActiveEnemyDelta);
	const int32 MobileCap = bMobile ? FMath::Max(1, Definition.MobileMaximumActiveEnemies) : EncounterCap;
	return FMath::Max(1, FMath::Min3(EncounterCap, MobileCap, FMath::Max(1, PlatformCap)));
}

bool UAHEncounterDirectorSubsystem::ShouldTriggerForceRemaining(int32 ActiveEnemyCount, int32 InitialForceSize, float RemainingRatio)
{
	return InitialForceSize > 0 && ActiveEnemyCount <= FMath::FloorToInt(InitialForceSize * FMath::Clamp(RemainingRatio, 0.0f, 1.0f));
}

bool UAHEncounterDirectorSubsystem::IsQueryResultUsable(bool bSuccessful, int32 LocationCount)
{
	return bSuccessful && LocationCount > 0;
}

bool UAHEncounterDirectorSubsystem::IsArchetypeAvailable(const UAHEnemyDefinition* Archetype)
{
	return Archetype && !Archetype->CombatClass.IsNull() && Archetype->CombatClass.Get() != nullptr;
}

bool UAHEncounterDirectorSubsystem::ShouldCompleteEncounter(
	const UAHEncounterDefinition& Definition,
	int32 PhaseIndex,
	bool bPhasePlanComplete,
	int32 ActiveEnemyCount,
	bool bObjectiveComplete,
	const TSet<FName>& Scripts)
{
	switch (Definition.CompletionRule)
	{
	case EAHEncounterCompletionRule::AllPhasesAndEnemiesDefeated:
		return PhaseIndex == Definition.Phases.Num() - 1 && bPhasePlanComplete && ActiveEnemyCount == 0;
	case EAHEncounterCompletionRule::ObjectiveCompleted:
		return bObjectiveComplete;
	case EAHEncounterCompletionRule::ScriptedTrigger:
		return !Definition.CompletionTriggerId.IsNone() && Scripts.Contains(Definition.CompletionTriggerId);
	default:
		return false;
	}
}
