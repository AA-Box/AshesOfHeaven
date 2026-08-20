#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHProjectileBase.generated.h"

class USphereComponent;
class UProjectileMovementComponent;

UCLASS()
class ASHESOFHEAVEN_API AAHProjectileBase : public AActor
{
	GENERATED_BODY()

public:
	AAHProjectileBase();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> CollisionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	float Damage = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Projectile")
	float LifeSeconds = 8.0f;

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	virtual void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);
};
