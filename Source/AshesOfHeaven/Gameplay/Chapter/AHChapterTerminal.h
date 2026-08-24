#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "AHChapterTerminal.generated.h"

class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHChapterTerminalConfirmedDelegate);

UCLASS()
class ASHESOFHEAVEN_API AAHChapterTerminal : public AActor, public IAHInteractable
{
	GENERATED_BODY()

public:
	AAHChapterTerminal();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> TerminalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWidgetComponent> TerminalWidget;

	UPROPERTY(BlueprintAssignable, Category="Chapter")
	FAHChapterTerminalConfirmedDelegate OnConfirmed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	FText CasualtyText = FText::FromString(TEXT("PROJECTED CIVILIAN CASUALTIES\n\n11,407,231\n\nCONFIRM PLANETARY FAILSAFE?"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Chapter")
	FText ConfirmationText = FText::FromString(TEXT("AUTHORIZATION ACCEPTED"));

	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual float GetInteractionPriority_Implementation() const override;
	virtual float GetObjectiveInteractionPriority_Implementation() const override;

	bool IsConfirmed() const { return bConfirmed; }
	bool IsInspected() const { return bInspected; }

private:
	bool bInspected = false;
	bool bConfirmed = false;
};
