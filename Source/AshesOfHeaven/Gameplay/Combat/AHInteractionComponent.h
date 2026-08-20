#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AHInteractionComponent.generated.h"

UINTERFACE(BlueprintType)
class ASHESOFHEAVEN_API UAHInteractable : public UInterface
{
	GENERATED_BODY()
};

class ASHESOFHEAVEN_API IAHInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	void Interact(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interaction")
	FText GetInteractionPrompt() const;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHInteractionTargetChangedDelegate, AActor*, Target);

UCLASS(ClassGroup=(AshesOfHeaven), meta=(BlueprintSpawnableComponent))
class ASHESOFHEAVEN_API UAHInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAHInteractionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction", meta=(ClampMin=50.0))
	float InteractionDistance = 275.0f;

	UPROPERTY(BlueprintAssignable, Category="Interaction")
	FAHInteractionTargetChangedDelegate OnTargetChanged;

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void Interact();

	UFUNCTION(BlueprintPure, Category="Interaction")
	AActor* GetCurrentTarget() const { return CurrentTarget.Get(); }

	UFUNCTION(BlueprintPure, Category="Interaction")
	FText GetPrompt() const { return CurrentPrompt; }

private:
	TWeakObjectPtr<AActor> CurrentTarget;
	FText CurrentPrompt;
};
