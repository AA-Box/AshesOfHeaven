#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "AHWeaponPickup.generated.h"

class AAHWeaponBase;
class USphereComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EAHResourcePickupType : uint8
{
	Weapon,
	Ammo,
	Grenades
};

UCLASS()
class ASHESOFHEAVEN_API AAHWeaponPickup : public AActor, public IAHInteractable
{
	GENERATED_BODY()

public:
	AAHWeaponPickup();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	EAHResourcePickupType PickupType = EAHResourcePickupType::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	TSubclassOf<AAHWeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup", meta=(ClampMin=1))
	int32 Amount = 36;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
};
