#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AHPoolable.generated.h"

/** Immutable activation data supplied before a pooled actor is made visible or collidable. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHObjectPoolAcquireContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	FTransform Transform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	TObjectPtr<AActor> Owner = nullptr;

	UPROPERTY(BlueprintReadOnly, Category="Pooling")
	TObjectPtr<APawn> Instigator = nullptr;
};

UINTERFACE(BlueprintType)
class ASHESOFHEAVEN_API UAHPoolable : public UInterface
{
	GENERATED_BODY()
};

/**
 * Strict lifecycle contract for actors that can safely survive repeated activation.
 * Implementations own all type-specific reset work; BeginPlay and EndPlay are never replayed.
 */
class ASHESOFHEAVEN_API IAHPoolable
{
	GENERATED_BODY()

public:
	/**
	 * Explicitly opts the concrete class into pooling. Derived classes must opt in again when they add state.
	 * @return True only when this concrete class implements a complete acquire/release reset contract.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Pooling")
	bool CanBePooled() const;

	/** Restores every runtime default needed for a new activation. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Pooling")
	void OnAcquireFromPool(const FAHObjectPoolAcquireContext& Context);

	/** Clears all per-activation state and references before the actor becomes dormant. */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Pooling")
	void OnReleaseToPool();
};
