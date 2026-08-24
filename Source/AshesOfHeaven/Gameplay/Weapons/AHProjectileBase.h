#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Pooling/AHPoolable.h"
#include "AHProjectileBase.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class ASHESOFHEAVEN_API AAHProjectileBase : public AActor, public IAHPoolable
{
	GENERATED_BODY()

public:
	AAHProjectileBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Optional actor-owned trail; it is explicitly reset and restarted on every activation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UNiagaraComponent> TrailComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile|VFX")
	TObjectPtr<UNiagaraSystem> TrailEffect;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	float LifeSeconds = 8.0f;

	/** Spawns through the registered class pool, with a normal-spawn fallback when unavailable or exhausted. */
	UFUNCTION(BlueprintCallable, Category="Projectile", meta=(WorldContext="WorldContextObject"))
	static AAHProjectileBase* SpawnProjectile(
		const UObject* WorldContextObject,
		TSubclassOf<AAHProjectileBase> ProjectileClass,
		const FTransform& SpawnTransform,
		// Not Owner/Instigator: UHT rejects UFUNCTION parameters that shadow AActor's own members.
		AActor* ProjectileOwner,
		APawn* ProjectileInstigator,
		const FVector& Direction,
		float ShotDamage);

	/** Applies per-shot damage and velocity after lifecycle defaults have been restored. */
	void InitializeProjectile(const FVector& Direction, float ShotDamage);

	virtual void BeginPlay() override;
	virtual void EndPlay(EEndPlayReason::Type EndPlayReason) override;

	virtual bool CanBePooled_Implementation() const override;
	virtual void OnAcquireFromPool_Implementation(const FAHObjectPoolAcquireContext& Context) override;
	virtual void OnReleaseToPool_Implementation() override;

#if WITH_DEV_AUTOMATION_TESTS
	void TestSimulateImpact(AActor* OtherActor, const FHitResult& Hit);
	bool IsHitDelegateBoundForTest() const;
	bool IsExpiryTimerActiveForTest() const;
#endif

protected:
	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	void ReleaseOrDestroy();

private:
	void BindHitDelegate();
	void RestoreLifecycleDefaults();
	void RestartTrail();
	void ScheduleExpiry();
	void ExpireProjectile();

	FTimerHandle ExpiryTimer;
	bool bLifecycleActive = false;
	bool bHasImpacted = false;
};
