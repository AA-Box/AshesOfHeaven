#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHObjectiveZone.generated.h"

class UBoxComponent;

UCLASS()
class ASHESOFHEAVEN_API AAHObjectiveZone : public AActor
{
	GENERATED_BODY()

public:
	AAHObjectiveZone();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	FName ObjectiveId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objective")
	bool bDestroyAfterCompletion = true;

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
