#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHArmorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAHArmorChangedDelegate, float, Armor, float, MaxArmor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHArmorBrokenDelegate);

UCLASS(ClassGroup=(AshesOfHeaven), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHArmorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHArmorComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor", meta=(ClampMin=0.0))
	float MaxArmor = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor", meta=(ClampMin=0.0))
	float RegenerationDelay = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Armor", meta=(ClampMin=0.0))
	float RegenerationPerSecond = 18.0f;

	UPROPERTY(BlueprintAssignable, Category="Armor")
	FAHArmorChangedDelegate OnArmorChanged;

	UPROPERTY(BlueprintAssignable, Category="Armor")
	FAHArmorBrokenDelegate OnArmorBroken;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float AbsorbDamage(float IncomingDamage);
	void SetArmor(float NewArmor);
	void ResetArmor();

	UFUNCTION(BlueprintPure, Category="Armor")
	float GetArmor() const { return CurrentArmor; }

	UFUNCTION(BlueprintPure, Category="Armor")
	float GetArmorPercent() const;

	UFUNCTION(BlueprintPure, Category="Armor")
	float GetTimeUntilRegeneration(float CurrentTime) const;

private:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Armor", meta=(AllowPrivateAccess="true"))
	float CurrentArmor = 0.0f;

	float LastDamageTime = -BIG_NUMBER;
	bool bWasBroken = false;
};
