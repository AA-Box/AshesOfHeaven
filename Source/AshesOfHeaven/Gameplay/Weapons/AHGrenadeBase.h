#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Weapons/AHProjectileBase.h"
#include "NiagaraSystem.h"
#include "AHGrenadeBase.generated.h"

class USoundBase;

UCLASS()
class ASHESOFHEAVEN_API AAHGrenadeBase : public AAHProjectileBase
{
	GENERATED_BODY()

public:
	AAHGrenadeBase();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade", meta=(ClampMin=0.1))
	float FuseSeconds = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade", meta=(ClampMin=0.0))
	float ExplosionRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade", meta=(ClampMin=0.0))
	float MaximumExplosionDamage = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade", meta=(ClampMin=0.0))
	float MinimumExplosionDamage = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grenade", meta=(ClampMin=0.0))
	float ExplosionImpulse = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="VFX")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	void PrimeAndThrow(const FVector& Direction);

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);
	void Explode();

	FTimerHandle FuseTimer;
	bool bPrimed = false;
};
