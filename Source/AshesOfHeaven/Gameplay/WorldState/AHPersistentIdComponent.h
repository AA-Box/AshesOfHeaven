#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHPersistentIdComponent.generated.h"

/** Owns an authored actor identity that is serialized into the level and survives cook. */
UCLASS(ClassGroup=(Ashes), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHPersistentIdComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHPersistentIdComponent();

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|World State")
	FGuid GetPersistentId() const { return PersistentId; }

	/** Intended for deterministic runtime-spawn IDs and migration tooling. */
	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|World State")
	void SetPersistentId(const FGuid& NewPersistentId);

	/** Returns each ID that appears more than once among the supplied actors. */
	static TArray<FGuid> FindDuplicateIds(const TArray<AActor*>& Actors);

	virtual void OnComponentCreated() override;

#if WITH_EDITOR
	virtual void PostEditImport() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
	void EnsureAuthoredId();

	UPROPERTY(EditInstanceOnly, Category="World State", meta=(DisplayName="Persistent ID"))
	FGuid PersistentId;
};
