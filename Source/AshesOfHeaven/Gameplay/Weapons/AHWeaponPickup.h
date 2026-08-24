#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/WorldState/AHSavableActor.h"
#include "AHWeaponPickup.generated.h"

class AAHWeaponBase;
class UAHPersistentIdComponent;
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
class ASHESOFHEAVEN_API AAHWeaponPickup : public AActor, public IAHInteractable, public IAHSavableActor
{
	GENERATED_BODY()

public:
	AAHWeaponPickup();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> PickupMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHPersistentIdComponent> PersistentIdComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	EAHResourcePickupType PickupType = EAHResourcePickupType::Weapon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup")
	TSubclassOf<AAHWeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Pickup", meta=(ClampMin=1))
	int32 Amount = 36;

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual float GetInteractionPriority_Implementation() const override;
	void SetPersistentId(const FGuid& PersistentId);

	virtual FGuid GetPersistentId_Implementation() const override;
	virtual int32 GetWorldStateVersion_Implementation() const override { return 1; }
	virtual EAHWorldStateSerializationMode GetWorldStateSerializationMode_Implementation() const override { return EAHWorldStateSerializationMode::ExplicitPayload; }
	virtual bool ShouldSaveWorldTransform_Implementation() const override { return false; }
	virtual bool CaptureWorldState_Implementation(TArray<uint8>& OutPayload) const override;
	virtual bool RestoreWorldState_Implementation(const TArray<uint8>& Payload, int32 SavedStateVersion) override;
	virtual void OnWorldStateRestored_Implementation() override;
};
