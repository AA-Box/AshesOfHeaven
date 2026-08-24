#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Pooling/AHPoolable.h"
#include "Subsystems/WorldSubsystem.h"
#include "AHObjectPoolSubsystem.generated.h"

/** Runtime counters for one concrete actor class. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHObjectPoolMetrics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	TObjectPtr<UClass> ActorClass = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 Capacity = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 Active = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 Available = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 Misses = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 Growth = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 PeakUse = 0;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	int32 HardMax = 0;
};

USTRUCT()
struct FAHObjectPoolState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> Available;

	UPROPERTY(Transient)
	TSet<TObjectPtr<AActor>> Active;

	int32 HardMax = 0;
	int32 Misses = 0;
	int32 Growth = 0;
	int32 PeakUse = 0;
};

/**
 * Opt-in, per-world actor pools keyed by concrete class. Only explicit IAHPoolable classes are accepted;
 * unregistered and exhausted pools always use a normal spawn fallback so gameplay objects are never lost.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHObjectPoolSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	/** Registers a concrete class and creates its measured initial capacity. */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	bool PrimePool(TSubclassOf<AActor> ActorClass, int32 InitialCapacity, int32 HardMax);

	/** Registers a projectile pool using the active Ashes performance profile. */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	bool PrimeProjectilePool(TSubclassOf<AActor> ActorClass);

	/** Acquires a registered actor or safely performs a normal spawn fallback. */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	AActor* AcquireActor(TSubclassOf<AActor> ActorClass, const FAHObjectPoolAcquireContext& Context);

	template <typename T>
	T* AcquireActor(TSubclassOf<T> ActorClass, const FAHObjectPoolAcquireContext& Context)
	{
		return Cast<T>(AcquireActor(TSubclassOf<AActor>(ActorClass), Context));
	}

	/** Returns an active managed actor to its class pool. False means the caller must destroy it normally. */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	bool ReleaseActor(AActor* Actor);

	/** Releases every active managed actor, used for checkpoint and transient-world resets. */
	UFUNCTION(BlueprintCallable, Category="Pooling")
	int32 ReleaseAllActive();

	UFUNCTION(BlueprintPure, Category="Pooling")
	bool IsPoolRegistered(TSubclassOf<AActor> ActorClass) const;

	UFUNCTION(BlueprintPure, Category="Pooling")
	TArray<FAHObjectPoolMetrics> GetMetrics() const;

	void DumpMetrics() const;

private:
	AActor* SpawnManagedActor(TSubclassOf<AActor> ActorClass, const FAHObjectPoolAcquireContext& Context, bool bForPriming);
	AActor* SpawnFallbackActor(TSubclassOf<AActor> ActorClass, const FAHObjectPoolAcquireContext& Context) const;
	void PrepareAcquiredActor(AActor* Actor, const FAHObjectPoolAcquireContext& Context);
	void PrepareReleasedActor(AActor* Actor) const;
	bool IsConcreteClassPoolable(TSubclassOf<AActor> ActorClass) const;
	void LogDebugState(const TCHAR* Operation, const AActor* Actor, const FAHObjectPoolState& Pool) const;

	UPROPERTY(Transient)
	TMap<TSubclassOf<AActor>, FAHObjectPoolState> Pools;
};
