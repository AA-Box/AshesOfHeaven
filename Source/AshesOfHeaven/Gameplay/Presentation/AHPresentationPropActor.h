#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AHPresentationPropActor.generated.h"

class UStaticMeshComponent;

/** Reusable authored presentation prop base used by the art-target Blueprint library. */
UCLASS(Blueprintable)
class ASHESOFHEAVEN_API AAHPresentationPropActor : public AActor
{
	GENERATED_BODY()

public:
	AAHPresentationPropActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Presentation")
	TObjectPtr<UStaticMeshComponent> PropMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
	FName PresentationStyle = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Presentation")
	FText DisplayLabel;

protected:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> DetailMeshes;
};
