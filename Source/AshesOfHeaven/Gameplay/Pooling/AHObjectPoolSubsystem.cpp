#include "Gameplay/Pooling/AHObjectPoolSubsystem.h"

#include "AshesOfHeaven.h"
#include "Components/AudioComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/LatentActionManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

namespace
{
	bool bAHPoolDebug = false;

	void DumpAHPools(UWorld* World)
	{
		if (UAHObjectPoolSubsystem* Pool = World ? World->GetSubsystem<UAHObjectPoolSubsystem>() : nullptr)
		{
			Pool->DumpMetrics();
		}
	}

	void SetAHPoolDebug(const TArray<FString>& Args, UWorld* World)
	{
		bAHPoolDebug = Args.IsEmpty() ? !bAHPoolDebug : FCString::Atoi(*Args[0]) != 0;
		UE_LOG(LogAshesOfHeaven, Display, TEXT("ah.Pool.Debug=%d"), bAHPoolDebug ? 1 : 0);
		if (bAHPoolDebug)
		{
			DumpAHPools(World);
		}
	}

	FAutoConsoleCommandWithWorld AHPoolDumpCommand(
		TEXT("ah.Pool.Dump"),
		TEXT("Dumps capacity, active, available, misses, growth, peak use, and hard max for every Ashes actor pool."),
		FConsoleCommandWithWorldDelegate::CreateStatic(&DumpAHPools));

	FAutoConsoleCommandWithWorldAndArgs AHPoolDebugCommand(
		TEXT("ah.Pool.Debug"),
		TEXT("Toggles Ashes pool lifecycle logging, or sets it with ah.Pool.Debug 0|1."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&SetAHPoolDebug));
}

void UAHObjectPoolSubsystem::Deinitialize()
{
	Pools.Empty();
	Super::Deinitialize();
}

bool UAHObjectPoolSubsystem::IsConcreteClassPoolable(TSubclassOf<AActor> ActorClass) const
{
	if (!ActorClass || ActorClass->HasAnyClassFlags(CLASS_Abstract) || !ActorClass->ImplementsInterface(UAHPoolable::StaticClass()))
	{
		return false;
	}
	const AActor* DefaultActor = ActorClass->GetDefaultObject<AActor>();
	if (!DefaultActor || !IAHPoolable::Execute_CanBePooled(DefaultActor))
	{
		return false;
	}
	// Replicated actors need a replicated activation protocol so clients restart their own
	// components and collision. Until a type provides one, keep network play on normal spawning.
	return !DefaultActor->GetIsReplicated() || !GetWorld() || GetWorld()->GetNetMode() == NM_Standalone;
}

bool UAHObjectPoolSubsystem::PrimePool(TSubclassOf<AActor> ActorClass, int32 InitialCapacity, int32 HardMax)
{
	if (!GetWorld() || !IsConcreteClassPoolable(ActorClass) || InitialCapacity < 0 || HardMax <= 0 || InitialCapacity > HardMax)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("Pool registration rejected class=%s initial=%d hardMax=%d"),
			*GetNameSafe(ActorClass), InitialCapacity, HardMax);
		return false;
	}

	FAHObjectPoolState& Pool = Pools.FindOrAdd(ActorClass);
	const int32 ExistingCapacity = Pool.Active.Num() + Pool.Available.Num();
	Pool.HardMax = FMath::Max(ExistingCapacity, HardMax);
	const int32 TargetCapacity = FMath::Min(InitialCapacity, Pool.HardMax);
	const FAHObjectPoolAcquireContext DormantContext;
	for (int32 Index = ExistingCapacity; Index < TargetCapacity; ++Index)
	{
		AActor* Actor = SpawnManagedActor(ActorClass, DormantContext, true);
		if (!Actor)
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("Pool priming stopped class=%s capacity=%d target=%d"),
				*GetNameSafe(ActorClass), Pool.Available.Num() + Pool.Active.Num(), TargetCapacity);
			return false;
		}
		Pool.Active.Add(Actor);
		ReleaseActor(Actor);
	}
	return true;
}

bool UAHObjectPoolSubsystem::PrimeProjectilePool(TSubclassOf<AActor> ActorClass)
{
	int32 InitialCapacity = 0;
	int32 HardMax = 128;
	if (const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		InitialCapacity = Platform->GetPerformanceProfile().InitialProjectilePoolSize;
		HardMax = Platform->GetPerformanceProfile().MaxProjectilePoolSize;
	}
	return PrimePool(ActorClass, InitialCapacity, HardMax);
}

AActor* UAHObjectPoolSubsystem::SpawnManagedActor(TSubclassOf<AActor> ActorClass, const FAHObjectPoolAcquireContext& Context, bool bForPriming)
{
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Context.Owner;
	SpawnParameters.Instigator = Context.Instigator;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FTransform SpawnTransform = bForPriming
		? FTransform(FRotator::ZeroRotator, FVector(0.0, 0.0, -1000000.0))
		: Context.Transform;
	return GetWorld()->SpawnActor<AActor>(ActorClass, SpawnTransform, SpawnParameters);
}

AActor* UAHObjectPoolSubsystem::SpawnFallbackActor(TSubclassOf<AActor> ActorClass, const FAHObjectPoolAcquireContext& Context) const
{
	if (!GetWorld() || !ActorClass)
	{
		return nullptr;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = Context.Owner;
	SpawnParameters.Instigator = Context.Instigator;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	// This is deliberately a normal Unreal spawn. Replaying a pool acquire callback here would
	// double-initialize pool-aware actors after BeginPlay and could reset an unapproved subclass.
	return GetWorld()->SpawnActor<AActor>(ActorClass, Context.Transform, SpawnParameters);
}

AActor* UAHObjectPoolSubsystem::AcquireActor(TSubclassOf<AActor> ActorClass, const FAHObjectPoolAcquireContext& Context)
{
	if (!GetWorld() || !ActorClass)
	{
		return nullptr;
	}
	FAHObjectPoolState* Pool = Pools.Find(ActorClass);
	if (!Pool)
	{
		return SpawnFallbackActor(ActorClass, Context);
	}

	while (!Pool->Available.IsEmpty() && !IsValid(Pool->Available.Last()))
	{
		Pool->Available.Pop(EAllowShrinking::No);
	}

	AActor* Actor = Pool->Available.IsEmpty() ? nullptr : Pool->Available.Pop(EAllowShrinking::No);
	if (!Actor)
	{
		const int32 Capacity = Pool->Active.Num() + Pool->Available.Num();
		if (Capacity < Pool->HardMax)
		{
			Actor = SpawnManagedActor(ActorClass, Context, false);
			if (Actor)
			{
				++Pool->Growth;
			}
		}
		else
		{
			++Pool->Misses;
			return SpawnFallbackActor(ActorClass, Context);
		}
	}

	if (!Actor)
	{
		++Pool->Misses;
		return SpawnFallbackActor(ActorClass, Context);
	}

	PrepareAcquiredActor(Actor, Context);
	Pool->Active.Add(Actor);
	Pool->PeakUse = FMath::Max(Pool->PeakUse, Pool->Active.Num());
	LogDebugState(TEXT("acquire"), Actor, *Pool);
	return Actor;
}

void UAHObjectPoolSubsystem::PrepareAcquiredActor(AActor* Actor, const FAHObjectPoolAcquireContext& Context)
{
	Actor->SetActorTransform(Context.Transform, false, nullptr, ETeleportType::TeleportPhysics);
	Actor->SetOwner(Context.Owner);
	Actor->SetInstigator(Context.Instigator);
	const AActor* DefaultActor = Actor->GetClass()->GetDefaultObject<AActor>();
	Actor->SetActorTickEnabled(DefaultActor && DefaultActor->PrimaryActorTick.bStartWithTickEnabled);
	IAHPoolable::Execute_OnAcquireFromPool(Actor, Context);
	Actor->SetActorHiddenInGame(false);
	Actor->ForceNetUpdate();
}

void UAHObjectPoolSubsystem::PrepareReleasedActor(AActor* Actor) const
{
	if (UWorld* World = Actor->GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(Actor);
		World->GetLatentActionManager().RemoveActionsForObject(Actor);
	}
	Actor->SetLifeSpan(0.0f);

	TInlineComponentArray<UActorComponent*> Components(Actor);
	for (UActorComponent* Component : Components)
	{
		if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Primitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Primitive->ClearMoveIgnoreActors();
			if (Primitive->IsSimulatingPhysics())
			{
				Primitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
				Primitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				if (FBodyInstance* BodyInstance = Primitive->GetBodyInstance())
				{
					BodyInstance->ClearForces(false);
					BodyInstance->ClearTorques(false);
				}
			}
		}
		if (UMeshComponent* Mesh = Cast<UMeshComponent>(Component))
		{
			Mesh->EmptyOverrideMaterials();
		}
		if (UNiagaraComponent* Niagara = Cast<UNiagaraComponent>(Component))
		{
			Niagara->DeactivateImmediate();
			Niagara->ResetSystem();
		}
		if (UAudioComponent* Audio = Cast<UAudioComponent>(Component))
		{
			Audio->Stop();
		}
	}

	Actor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	Actor->SetActorEnableCollision(false);
	Actor->SetActorTickEnabled(false);
	Actor->SetActorHiddenInGame(true);
	Actor->SetOwner(nullptr);
	Actor->SetInstigator(nullptr);
	Actor->SetActorTransform(FTransform::Identity, false, nullptr, ETeleportType::TeleportPhysics);
}

bool UAHObjectPoolSubsystem::ReleaseActor(AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	FAHObjectPoolState* Pool = Pools.Find(Actor->GetClass());
	if (!Pool || !Pool->Active.Remove(Actor))
	{
		return false;
	}

	IAHPoolable::Execute_OnReleaseToPool(Actor);
	if (!IsValid(Actor))
	{
		return true;
	}
	PrepareReleasedActor(Actor);
	Pool->Available.Add(Actor);
	LogDebugState(TEXT("release"), Actor, *Pool);
	return true;
}

int32 UAHObjectPoolSubsystem::ReleaseAllActive()
{
	TArray<TObjectPtr<AActor>> ActiveActors;
	for (const TPair<TSubclassOf<AActor>, FAHObjectPoolState>& Pair : Pools)
	{
		ActiveActors.Append(Pair.Value.Active.Array());
	}
	int32 Released = 0;
	for (AActor* Actor : ActiveActors)
	{
		Released += ReleaseActor(Actor) ? 1 : 0;
	}
	return Released;
}

bool UAHObjectPoolSubsystem::IsPoolRegistered(TSubclassOf<AActor> ActorClass) const
{
	return Pools.Contains(ActorClass);
}

TArray<FAHObjectPoolMetrics> UAHObjectPoolSubsystem::GetMetrics() const
{
	TArray<FAHObjectPoolMetrics> Result;
	Result.Reserve(Pools.Num());
	for (const TPair<TSubclassOf<AActor>, FAHObjectPoolState>& Pair : Pools)
	{
		FAHObjectPoolMetrics& Metrics = Result.AddDefaulted_GetRef();
		Metrics.ActorClass = Pair.Key.Get();
		Metrics.Active = Pair.Value.Active.Num();
		Metrics.Available = Pair.Value.Available.Num();
		Metrics.Capacity = Metrics.Active + Metrics.Available;
		Metrics.Misses = Pair.Value.Misses;
		Metrics.Growth = Pair.Value.Growth;
		Metrics.PeakUse = Pair.Value.PeakUse;
		Metrics.HardMax = Pair.Value.HardMax;
	}
	Result.Sort([](const FAHObjectPoolMetrics& A, const FAHObjectPoolMetrics& B)
	{
		return GetNameSafe(A.ActorClass) < GetNameSafe(B.ActorClass);
	});
	return Result;
}

void UAHObjectPoolSubsystem::DumpMetrics() const
{
	const TArray<FAHObjectPoolMetrics> Metrics = GetMetrics();
	UE_LOG(LogAshesOfHeaven, Display, TEXT("Ashes object pools: %d registered"), Metrics.Num());
	for (const FAHObjectPoolMetrics& Pool : Metrics)
	{
		UE_LOG(LogAshesOfHeaven, Display,
			TEXT("  %s capacity=%d active=%d available=%d misses=%d growth=%d peak=%d hardMax=%d"),
			*GetNameSafe(Pool.ActorClass), Pool.Capacity, Pool.Active, Pool.Available,
			Pool.Misses, Pool.Growth, Pool.PeakUse, Pool.HardMax);
	}
}

void UAHObjectPoolSubsystem::LogDebugState(const TCHAR* Operation, const AActor* Actor, const FAHObjectPoolState& Pool) const
{
	if (bAHPoolDebug)
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("Pool %s actor=%s active=%d available=%d misses=%d growth=%d peak=%d"),
			Operation, *GetNameSafe(Actor), Pool.Active.Num(), Pool.Available.Num(), Pool.Misses, Pool.Growth, Pool.PeakUse);
	}
}
