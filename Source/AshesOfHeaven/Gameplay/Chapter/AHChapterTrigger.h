#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHChapterTrigger.generated.h"

class UBoxComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHChapterTriggerDelegate, FName, TriggerId);

UCLASS()
class ASHESOFHEAVEN_API AAHChapterTrigger : public AActor
{
	GENERATED_BODY()

public:
	AAHChapterTrigger();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UBoxComponent> Trigger;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	FName TriggerId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	bool bOneShot = true;

	UPROPERTY(BlueprintAssignable, Category="Chapter")
	FAHChapterTriggerDelegate OnTriggered;

	void ResetTrigger();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	bool bTriggered = false;
};
