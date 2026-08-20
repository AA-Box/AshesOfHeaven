#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHCheckpointActor.generated.h"

class UBoxComponent;

UCLASS()
class ASHESOFHEAVEN_API AAHCheckpointActor : public AActor
{
	GENERATED_BODY()

public:
	AAHCheckpointActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FName CheckpointId = NAME_None;

	virtual void BeginPlay() override;

protected:
	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
