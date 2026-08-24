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

	/** Seconds out of fire before health starts coming back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin=0.0))
	float RegenerationDelay = 5.0f;

	/** Zero is off, and off is what every AI combatant wants: only the player regenerates.
	 * Without this, armour recharged and health did not, so incoming damage was a one-way
	 * clock and a long fight killed the player no matter how well it was played. Regenerating
	 * player health after a break in contact is the shooter default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Health", meta=(ClampMin=0.0))
	float RegenerationPerSecond = 0.0f;

	UPROPERTY(BlueprintAssignable, Category="Health")
	FAHHealthChangedDelegate OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Health")
	FAHDeathDelegate OnDeath;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float ApplyDamage(float Damage);
	void SetHealth(float NewHealth);
	void ResetHealth();

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="Health")
	bool IsDead() const { return bDead; }

	UFUNCTION(BlueprintPure, Category="Health")
	float GetTimeUntilRegeneration(float CurrentTime) const;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Health", meta=(AllowPrivateAccess="true"))
	float CurrentHealth = 0.0f;

	float LastDamageTime = -BIG_NUMBER;
	bool bDead = false;
};
