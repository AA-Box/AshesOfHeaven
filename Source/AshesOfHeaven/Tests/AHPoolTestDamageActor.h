#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHPoolTestDamageActor.generated.h"

/** Minimal damage sink used to verify pooled projectile damage without gameplay-character dependencies. */
UCLASS()
class ASHESOFHEAVEN_API AAHPoolTestDamageActor : public AActor
{
	GENERATED_BODY()

public:
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	float LastDamage = 0.0f;
	int32 DamageEvents = 0;
};
