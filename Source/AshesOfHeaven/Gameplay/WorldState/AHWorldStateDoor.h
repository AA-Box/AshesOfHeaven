#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/WorldState/AHSavableActor.h"
#include "AHWorldStateDoor.generated.h"

class UAHPersistentIdComponent;
class USceneComponent;
class UStaticMeshComponent;

/** Representative persistent door with compact, versioned explicit state. */
UCLASS()
class ASHESOFHEAVEN_API AAHWorldStateDoor : public AActor, public IAHInteractable, public IAHSavableActor
{
	GENERATED_BODY()

public:
	AAHWorldStateDoor();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> DoorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHPersistentIdComponent> PersistentIdComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	bool bStartsOpen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	float OpenAngleDegrees = 95.0f;

	UFUNCTION(BlueprintCallable, Category="Door")
	void SetDoorOpen(bool bNewOpen);

	UFUNCTION(BlueprintPure, Category="Door")
	bool IsDoorOpen() const { return bOpen; }

	void SetPersistentId(const FGuid& PersistentId);

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual FGuid GetPersistentId_Implementation() const override;
	virtual int32 GetWorldStateVersion_Implementation() const override { return 1; }
	virtual EAHWorldStateSerializationMode GetWorldStateSerializationMode_Implementation() const override { return EAHWorldStateSerializationMode::ExplicitPayload; }
	virtual bool ShouldSaveWorldTransform_Implementation() const override { return true; }
	virtual bool CaptureWorldState_Implementation(TArray<uint8>& OutPayload) const override;
	virtual bool RestoreWorldState_Implementation(const TArray<uint8>& Payload, int32 SavedStateVersion) override;
	virtual void OnWorldStateRestored_Implementation() override;

private:
	void ApplyDoorPresentation();

	bool bOpen = false;
	FTransform ClosedMeshTransform = FTransform::Identity;
};
