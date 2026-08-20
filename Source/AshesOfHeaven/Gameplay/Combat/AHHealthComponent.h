#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAHHealthChangedDelegate, float, Health, float, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHDeathDelegate);

UCLASS(ClassGroup=(AshesOfHeaven), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHHealthComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin=1.0))
	float MaxHealth = 100.0f;

	UPROPERTY(BlueprintAssignable, Category="Health")
	FAHHealthChangedDelegate OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Health")
	FAHDeathDelegate OnDeath;

	virtual void BeginPlay() override;

	float ApplyDamage(float Damage);
	void SetHealth(float NewHealth);
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return bDead; }

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health", meta=(AllowPrivateAccess="true"))
	float CurrentHealth = 0.0f;

	bool bDead = false;
};
