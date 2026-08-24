#include "Gameplay/Combat/AHCorpseManagerSubsystem.h"

#include "AshesOfHeaven.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "ProfilingDebugging/ResourceSize.h"

const FName FAHCorpseTags::Persistent(TEXT("Corpse.Persistent"));
const FName FAHCorpseTags::Narrative(TEXT("Corpse.Narrative"));
const FName FAHCorpseTags::Lootable(TEXT("Corpse.Lootable"));
const FName FAHCorpseTags::AllowCleanup(TEXT("Corpse.AllowCleanup"));
const FName FAHCorpseTags::ObjectiveCritical(TEXT("Corpse.ObjectiveCritical"));
const FName FAHCorpseTags::ScriptedCivilian(TEXT("Corpse.ScriptedCivilian"));

void FAHCorpseBudget::Sanitize()
{
	SoftLimit = FMath::Max(0, SoftLimit);
	HardLimit = FMath::Max(SoftLimit, HardLimit);
	MinimumLifetimeSeconds = FMath::Max(0.0f, MinimumLifetimeSeconds);
	EmergencyMinimumLifetimeSeconds = FMath::Clamp(EmergencyMinimumLifetimeSeconds, 0.0f, MinimumLifetimeSeconds);
	DeathReactionSeconds = FMath::Max(0.0f, DeathReactionSeconds);
	SettleDelaySeconds = FMath::Max(DeathReactionSeconds, SettleDelaySeconds);
	MaximumRagdollSeconds = FMath::Max(SettleDelaySeconds, MaximumRagdollSeconds);
	ReducedCostDelaySeconds = FMath::Max(0.0f, ReducedCostDelaySeconds);
	RecentlyRenderedGraceSeconds = FMath::Max(0.0f, RecentlyRenderedGraceSeconds);
	ViewPaddingDegrees = FMath::Clamp(ViewPaddingDegrees, 0.0f, 45.0f);
	CleanupIntervalSeconds = FMath::Max(0.01f, CleanupIntervalSeconds);
	MaximumCleanupPerPass = FMath::Max(1, MaximumCleanupPerPass);
	DistanceScoreScale = FMath::Max(1.0f, DistanceScoreScale);
	AgeScoreWeight = FMath::Max(0.0f, AgeScoreWeight);
	DistanceScoreWeight = FMath::Max(0.0f, DistanceScoreWeight);
	OffscreenScoreWeight = FMath::Max(0.0f, OffscreenScoreWeight);
	ImportanceScoreWeight = FMath::Max(0.0f, ImportanceScoreWeight);
}

bool UAHCorpseManagerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game
		|| World->WorldType == EWorldType::PIE
		|| World->WorldType == EWorldType::GamePreview);
}

void UAHCorpseManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(GetWorld()))
	{
		Budget = Platform->GetPerformanceProfile().CorpseBudget;
	}
	else
	{
		Budget = UAHPlatformManagerSubsystem::SelectCorpseBudget(false, true);
	}
	Budget.Sanitize();
}

void UAHCorpseManagerSubsystem::Deinitialize()
{
	CorpseQueue.Reset();
	OnCorpseStateChanged.Clear();
	Super::Deinitialize();
}

void UAHCorpseManagerSubsystem::Tick(float DeltaTime)
{
	CleanupAccumulator += DeltaTime;
	if (CleanupAccumulator < Budget.CleanupIntervalSeconds || !GetWorld())
	{
		return;
	}

	CleanupAccumulator = FMath::Fmod(CleanupAccumulator, Budget.CleanupIntervalSeconds);
	ProcessLifecycle(GetWorld()->GetTimeSeconds());
}

TStatId UAHCorpseManagerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAHCorpseManagerSubsystem, STATGROUP_Tickables);
}

void UAHCorpseManagerSubsystem::RegisterCorpse(AAHCombatantCharacter* Corpse)
{
	if (!IsValid(Corpse) || !Corpse->IsCombatantDead())
	{
		return;
	}
	for (const FManagedCorpse& Entry : CorpseQueue)
	{
		if (Entry.Actor.Get() == Corpse)
		{
			return;
		}
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	FManagedCorpse& Entry = CorpseQueue.Emplace_GetRef();
	Entry.Actor = Corpse;
	Entry.DeathTimeSeconds = Now;
	Entry.StateEnteredTimeSeconds = Now;
	Entry.QueueSequence = NextQueueSequence++;
	++TotalRegistered;

	Corpse->PrepareForCorpseManagement();
	OnCorpseStateChanged.Broadcast(Corpse, EAHCorpseLifecycleState::Alive, Entry.State);

	// Hard-cap cleanup is evaluated immediately, while the emergency lifetime still guarantees
	// the newly registered body cannot disappear in its death reaction.
	CleanupUnderPressure(Now);
}

void UAHCorpseManagerSubsystem::ProcessLifecycle(float CurrentTimeSeconds)
{
	CompactInvalidEntries();
	for (FManagedCorpse& Entry : CorpseQueue)
	{
		AdvanceLifecycle(Entry, CurrentTimeSeconds);
	}
	CleanupUnderPressure(CurrentTimeSeconds);
}

void UAHCorpseManagerSubsystem::AdvanceLifecycle(FManagedCorpse& Entry, float CurrentTimeSeconds)
{
	AAHCombatantCharacter* Corpse = Entry.Actor.Get();
	if (!IsValid(Corpse))
	{
		return;
	}

	const float Age = FMath::Max(0.0f, CurrentTimeSeconds - Entry.DeathTimeSeconds);
	for (int32 TransitionCount = 0; TransitionCount < 5; ++TransitionCount)
	{
		switch (Entry.State)
		{
		case EAHCorpseLifecycleState::DeathReaction:
			if (Age < Budget.DeathReactionSeconds)
			{
				return;
			}
			TransitionState(Entry, EAHCorpseLifecycleState::ActiveCorpse, CurrentTimeSeconds);
			break;

		case EAHCorpseLifecycleState::ActiveCorpse:
		{
			USkeletalMeshComponent* Body = Corpse->GetMesh();
			const bool bCanCheckSleep = Age >= Budget.SettleDelaySeconds;
			const bool bNaturallySettled = !Body || !Body->IsSimulatingPhysics() || !Body->IsAnyRigidBodyAwake();
			if (!bCanCheckSleep || (!bNaturallySettled && Age < Budget.MaximumRagdollSeconds))
			{
				return;
			}
			TransitionState(Entry, EAHCorpseLifecycleState::SettledCorpse, CurrentTimeSeconds);
			break;
		}

		case EAHCorpseLifecycleState::SettledCorpse:
			if (CurrentTimeSeconds - Entry.StateEnteredTimeSeconds < Budget.ReducedCostDelaySeconds)
			{
				return;
			}
			TransitionState(Entry, EAHCorpseLifecycleState::ReducedCostCorpse, CurrentTimeSeconds);
			break;

		case EAHCorpseLifecycleState::ReducedCostCorpse:
			if (Age < Budget.MinimumLifetimeSeconds || !IsOrdinaryCorpse(Corpse))
			{
				return;
			}
			TransitionState(Entry, EAHCorpseLifecycleState::EligibleForCleanup, CurrentTimeSeconds);
			break;

		default:
			return;
		}
	}
}

void UAHCorpseManagerSubsystem::TransitionState(FManagedCorpse& Entry, EAHCorpseLifecycleState NewState, float CurrentTimeSeconds)
{
	if (Entry.State == NewState)
	{
		return;
	}

	AAHCombatantCharacter* Corpse = Entry.Actor.Get();
	const EAHCorpseLifecycleState PreviousState = Entry.State;
	Entry.State = NewState;
	Entry.StateEnteredTimeSeconds = CurrentTimeSeconds;

	if (IsValid(Corpse))
	{
		if (NewState == EAHCorpseLifecycleState::SettledCorpse)
		{
			Corpse->SettleCorpsePhysics();
			++TotalSettled;
		}
		else if (NewState == EAHCorpseLifecycleState::ReducedCostCorpse)
		{
			Corpse->ApplyReducedCorpseCost();
		}
		OnCorpseStateChanged.Broadcast(Corpse, PreviousState, NewState);
	}
}

bool UAHCorpseManagerSubsystem::CanCleanupCandidate(const FAHCorpseCleanupEvaluation& Evaluation, const FAHCorpseBudget& InBudget, bool bHardCapPressure)
{
	if (!Evaluation.bAllowCleanup
		|| Evaluation.bPersistent
		|| Evaluation.bNarrative
		|| Evaluation.bObjectiveCritical
		|| Evaluation.bScriptedCivilian
		|| Evaluation.bCurrentlyVisible
		|| Evaluation.bTargetedInteractable
		|| Evaluation.bHasImportantLoot)
	{
		return false;
	}
	if (!bHardCapPressure && Evaluation.bRecentlyRendered)
	{
		return false;
	}
	const float RequiredLifetime = bHardCapPressure
		? InBudget.EmergencyMinimumLifetimeSeconds
		: InBudget.MinimumLifetimeSeconds;
	return Evaluation.AgeSeconds >= RequiredLifetime;
}

float UAHCorpseManagerSubsystem::CalculateCleanupScore(const FAHCorpseCleanupEvaluation& Evaluation, const FAHCorpseBudget& InBudget)
{
	const float AgeScale = FMath::Max(1.0f, InBudget.MinimumLifetimeSeconds);
	const float AgeScore = FMath::Clamp(Evaluation.AgeSeconds / AgeScale, 0.0f, 3.0f);
	const float DistanceScore = FMath::Clamp(Evaluation.DistanceToClosestPlayer / FMath::Max(1.0f, InBudget.DistanceScoreScale), 0.0f, 3.0f);
	const float OffscreenScore = Evaluation.bRecentlyRendered ? 0.0f : 1.0f;
	return AgeScore * InBudget.AgeScoreWeight
		+ DistanceScore * InBudget.DistanceScoreWeight
		+ OffscreenScore * InBudget.OffscreenScoreWeight
		- FMath::Clamp(Evaluation.Importance, 0.0f, 1.0f) * InBudget.ImportanceScoreWeight;
}

int32 UAHCorpseManagerSubsystem::SelectCleanupCandidate(const TArray<FAHCorpseCleanupEvaluation>& Evaluations, const FAHCorpseBudget& InBudget, bool bHardCapPressure)
{
	int32 BestIndex = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Lowest();
	uint64 BestSequence = TNumericLimits<uint64>::Max();
	for (int32 Index = 0; Index < Evaluations.Num(); ++Index)
	{
		const FAHCorpseCleanupEvaluation& Evaluation = Evaluations[Index];
		if (!CanCleanupCandidate(Evaluation, InBudget, bHardCapPressure))
		{
			continue;
		}
		const float Score = CalculateCleanupScore(Evaluation, InBudget);
		if (Score > BestScore || (FMath::IsNearlyEqual(Score, BestScore) && Evaluation.QueueSequence < BestSequence))
		{
			BestIndex = Index;
			BestScore = Score;
			BestSequence = Evaluation.QueueSequence;
		}
	}
	return BestIndex;
}

void UAHCorpseManagerSubsystem::CleanupUnderPressure(float CurrentTimeSeconds)
{
	for (int32 RemovedThisPass = 0; RemovedThisPass < Budget.MaximumCleanupPerPass; ++RemovedThisPass)
	{
		const int32 OrdinaryCount = CountOrdinaryCorpses();
		const bool bHardCapPressure = OrdinaryCount > Budget.HardLimit;
		if (!bHardCapPressure && OrdinaryCount <= Budget.SoftLimit)
		{
			return;
		}

		TArray<FAHCorpseCleanupEvaluation> Evaluations;
		TArray<int32> EntryIndices;
		for (int32 EntryIndex = 0; EntryIndex < CorpseQueue.Num(); ++EntryIndex)
		{
			const FManagedCorpse& Entry = CorpseQueue[EntryIndex];
			AAHCombatantCharacter* Corpse = Entry.Actor.Get();
			if (!IsValid(Corpse) || !IsOrdinaryCorpse(Corpse))
			{
				continue;
			}
			if (!bHardCapPressure && Entry.State != EAHCorpseLifecycleState::EligibleForCleanup)
			{
				continue;
			}
			Evaluations.Add(BuildEvaluation(Entry, CurrentTimeSeconds));
			EntryIndices.Add(EntryIndex);
		}

		const int32 CandidateIndex = SelectCleanupCandidate(Evaluations, Budget, bHardCapPressure);
		if (CandidateIndex == INDEX_NONE)
		{
			if (bHardCapPressure && GetWorld() && CurrentTimeSeconds - LastHardCapWarningTime >= 5.0f)
			{
				LastHardCapWarningTime = CurrentTimeSeconds;
				UE_LOG(LogAshesOfHeaven, Warning, TEXT("Corpse hard cap deferred: ordinary=%d hard=%d; all candidates are visible, too young, targeted, or protected"), OrdinaryCount, Budget.HardLimit);
			}
			return;
		}
		RemoveCorpseAt(EntryIndices[CandidateIndex], false);
	}
}

void UAHCorpseManagerSubsystem::RemoveCorpseAt(int32 EntryIndex, bool bCheckpointReset)
{
	if (!CorpseQueue.IsValidIndex(EntryIndex))
	{
		return;
	}

	FManagedCorpse& Entry = CorpseQueue[EntryIndex];
	AAHCombatantCharacter* Corpse = Entry.Actor.Get();
	if (IsValid(Corpse))
	{
		TransitionState(Entry, EAHCorpseLifecycleState::RemovedOrRecycled, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f);
		Corpse->PrepareForCorpseRemoval();
		Corpse->Destroy();
	}
	CorpseQueue.RemoveAt(EntryIndex, 1, EAllowShrinking::No);
	++TotalRemovedOrRecycled;

	if (bCheckpointReset)
	{
		UE_LOG(LogAshesOfHeaven, Verbose, TEXT("Removed ordinary combat corpse during checkpoint reset"));
	}
}

void UAHCorpseManagerSubsystem::ResetOrdinaryCorpsesForCheckpoint()
{
	for (int32 Index = CorpseQueue.Num() - 1; Index >= 0; --Index)
	{
		AAHCombatantCharacter* Corpse = CorpseQueue[Index].Actor.Get();
		if (!IsValid(Corpse))
		{
			CorpseQueue.RemoveAt(Index, 1, EAllowShrinking::No);
		}
		else if (IsOrdinaryCorpse(Corpse))
		{
			RemoveCorpseAt(Index, true);
		}
	}
}

void UAHCorpseManagerSubsystem::CompactInvalidEntries()
{
	CorpseQueue.RemoveAllSwap([](const FManagedCorpse& Entry)
	{
		return !Entry.Actor.IsValid();
	}, EAllowShrinking::No);
}

FAHCorpseCleanupEvaluation UAHCorpseManagerSubsystem::BuildEvaluation(const FManagedCorpse& Entry, float CurrentTimeSeconds) const
{
	FAHCorpseCleanupEvaluation Evaluation;
	const AAHCombatantCharacter* Corpse = Entry.Actor.Get();
	if (!IsValid(Corpse))
	{
		Evaluation.bAllowCleanup = false;
		return Evaluation;
	}

	Evaluation.AgeSeconds = FMath::Max(0.0f, CurrentTimeSeconds - Entry.DeathTimeSeconds);
	Evaluation.DistanceToClosestPlayer = DistanceToClosestPlayer(Corpse);
	Evaluation.Importance = Corpse->GetCorpseImportance();
	Evaluation.bAllowCleanup = Corpse->AllowsCorpseCleanup();
	Evaluation.bPersistent = Corpse->IsPersistentCorpse();
	Evaluation.bNarrative = Corpse->IsNarrativeCorpse();
	Evaluation.bObjectiveCritical = Corpse->IsObjectiveCriticalCorpse();
	Evaluation.bScriptedCivilian = Corpse->IsScriptedCivilianCorpse();
	Evaluation.bCurrentlyVisible = IsInAnyPlayerView(Corpse);
	Evaluation.bRecentlyRendered = Corpse->WasRecentlyRendered(Budget.RecentlyRenderedGraceSeconds);
	Evaluation.bTargetedInteractable = IsTargetedInteractable(Corpse);
	Evaluation.bHasImportantLoot = Corpse->HasUnlootedImportantWeapon();
	Evaluation.QueueSequence = Entry.QueueSequence;
	return Evaluation;
}

bool UAHCorpseManagerSubsystem::IsInAnyPlayerView(const AAHCombatantCharacter* Corpse) const
{
	if (!Corpse || !GetWorld())
	{
		return false;
	}

	const FBox Bounds = Corpse->GetComponentsBoundingBox(true);
	const FVector Target = Bounds.IsValid ? Bounds.GetCenter() : Corpse->GetActorLocation();
	const float Radius = Bounds.IsValid ? Bounds.GetExtent().Size() : 100.0f;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		if (!Controller)
		{
			continue;
		}
		FVector ViewLocation;
		FRotator ViewRotation;
		Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
		const FVector ToTarget = Target - ViewLocation;
		const float Distance = ToTarget.Size();
		if (Distance <= Radius)
		{
			return true;
		}
		const float FOV = Controller->PlayerCameraManager ? Controller->PlayerCameraManager->GetFOVAngle() : 90.0f;
		const float BoundsPadding = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Radius / Distance, 0.0f, 1.0f)));
		const float HalfAngle = FMath::Clamp(FOV * 0.5f + Budget.ViewPaddingDegrees + BoundsPadding, 0.0f, 179.0f);
		if (FVector::DotProduct(ViewRotation.Vector(), ToTarget / Distance) >= FMath::Cos(FMath::DegreesToRadians(HalfAngle)))
		{
			return true;
		}
	}
	return false;
}

bool UAHCorpseManagerSubsystem::IsTargetedInteractable(const AAHCombatantCharacter* Corpse) const
{
	if (!Corpse || !GetWorld())
	{
		return false;
	}
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		const UAHInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UAHInteractionComponent>() : nullptr;
		if (Interaction && Interaction->GetCurrentTarget() == Corpse)
		{
			return true;
		}
	}
	return false;
}

float UAHCorpseManagerSubsystem::DistanceToClosestPlayer(const AAHCombatantCharacter* Corpse) const
{
	float ClosestDistanceSquared = TNumericLimits<float>::Max();
	if (Corpse && GetWorld())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			const APlayerController* Controller = It->Get();
			const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
			if (Pawn)
			{
				ClosestDistanceSquared = FMath::Min(ClosestDistanceSquared, FVector::DistSquared(Pawn->GetActorLocation(), Corpse->GetActorLocation()));
			}
		}
	}
	return ClosestDistanceSquared == TNumericLimits<float>::Max()
		? Budget.DistanceScoreScale * 2.0f
		: FMath::Sqrt(ClosestDistanceSquared);
}

bool UAHCorpseManagerSubsystem::IsPermanentPreservation(const AAHCombatantCharacter* Corpse) const
{
	return !Corpse || !Corpse->AllowsCorpseCleanup()
		|| Corpse->IsPersistentCorpse()
		|| Corpse->IsNarrativeCorpse()
		|| Corpse->IsObjectiveCriticalCorpse()
		|| Corpse->IsScriptedCivilianCorpse();
}

bool UAHCorpseManagerSubsystem::IsOrdinaryCorpse(const AAHCombatantCharacter* Corpse) const
{
	return Corpse && !IsPermanentPreservation(Corpse);
}

int32 UAHCorpseManagerSubsystem::CountOrdinaryCorpses() const
{
	int32 Count = 0;
	for (const FManagedCorpse& Entry : CorpseQueue)
	{
		Count += IsOrdinaryCorpse(Entry.Actor.Get()) ? 1 : 0;
	}
	return Count;
}

EAHCorpseLifecycleState UAHCorpseManagerSubsystem::GetCorpseState(const AAHCombatantCharacter* Corpse) const
{
	for (const FManagedCorpse& Entry : CorpseQueue)
	{
		if (Entry.Actor.Get() == Corpse)
		{
			return Entry.State;
		}
	}
	return EAHCorpseLifecycleState::Alive;
}

FAHCorpsePerformanceStats UAHCorpseManagerSubsystem::GetPerformanceStats() const
{
	FAHCorpsePerformanceStats Stats;
	Stats.ManagedCorpses = CorpseQueue.Num();
	Stats.OrdinaryCorpses = CountOrdinaryCorpses();
	Stats.TotalRegistered = TotalRegistered;
	Stats.TotalSettled = TotalSettled;
	Stats.TotalRemovedOrRecycled = TotalRemovedOrRecycled;
	Stats.EstimatedExclusiveMemoryBytes = CorpseQueue.GetAllocatedSize();

	for (const FManagedCorpse& Entry : CorpseQueue)
	{
		AAHCombatantCharacter* Corpse = Entry.Actor.Get();
		if (!IsValid(Corpse))
		{
			continue;
		}
		if (Entry.State == EAHCorpseLifecycleState::DeathReaction || Entry.State == EAHCorpseLifecycleState::ActiveCorpse)
		{
			++Stats.ActiveRagdolls;
		}
		if (Entry.State == EAHCorpseLifecycleState::ReducedCostCorpse || Entry.State == EAHCorpseLifecycleState::EligibleForCleanup)
		{
			++Stats.ReducedCostCorpses;
		}
		Stats.TickingActors += Corpse->IsActorTickEnabled() ? 1 : 0;

		FResourceSizeEx ActorSize(EResourceSizeMode::Exclusive);
		Corpse->GetResourceSizeEx(ActorSize);
		Stats.EstimatedExclusiveMemoryBytes += static_cast<int64>(ActorSize.GetTotalMemoryBytes());

		if (USkeletalMeshComponent* Body = Corpse->GetMesh())
		{
			++Stats.ManagedSkeletalMeshes;
			Stats.TickingSkeletalMeshes += Body->IsComponentTickEnabled() ? 1 : 0;
			Stats.SimulatingSkeletalMeshes += Body->IsSimulatingPhysics() ? 1 : 0;
			const UPhysicsAsset* PhysicsAsset = Body->GetPhysicsAsset();
			if (PhysicsAsset && Body->IsAnyRigidBodyAwake())
			{
				Stats.AwakePhysicsBodies += PhysicsAsset->SkeletalBodySetups.Num();
			}
		}
		if (const UAHInventoryComponent* Inventory = Corpse->GetInventoryComponent())
		{
			for (const AAHWeaponBase* Weapon : Inventory->GetWeapons())
			{
				Stats.TickingWeapons += IsValid(Weapon) && Weapon->IsActorTickEnabled() ? 1 : 0;
			}
		}
	}
	return Stats;
}

void UAHCorpseManagerSubsystem::LogPerformanceStats() const
{
	const FAHCorpsePerformanceStats Stats = GetPerformanceStats();
	UE_LOG(LogAshesOfHeaven, Display, TEXT("Corpse profile: managed=%d ordinary=%d active_ragdolls=%d reduced=%d skeletal_meshes=%d simulating=%d awake_bodies=%d actor_ticks=%d mesh_ticks=%d weapon_ticks=%d exclusive_bytes=%lld removed=%d"),
		Stats.ManagedCorpses,
		Stats.OrdinaryCorpses,
		Stats.ActiveRagdolls,
		Stats.ReducedCostCorpses,
		Stats.ManagedSkeletalMeshes,
		Stats.SimulatingSkeletalMeshes,
		Stats.AwakePhysicsBodies,
		Stats.TickingActors,
		Stats.TickingSkeletalMeshes,
		Stats.TickingWeapons,
		Stats.EstimatedExclusiveMemoryBytes,
		Stats.TotalRemovedOrRecycled);
}

#if WITH_DEV_AUTOMATION_TESTS
void UAHCorpseManagerSubsystem::SetBudgetForTesting(const FAHCorpseBudget& InBudget)
{
	Budget = InBudget;
	Budget.Sanitize();
}

void UAHCorpseManagerSubsystem::ProcessLifecycleForTesting(float CurrentTimeSeconds)
{
	ProcessLifecycle(CurrentTimeSeconds);
}
#endif
