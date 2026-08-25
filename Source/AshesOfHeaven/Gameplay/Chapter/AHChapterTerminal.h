#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/WorldState/AHSavableActor.h"
#include "AHChapterTerminal.generated.h"

class UAHPersistentIdComponent;
class UStaticMeshComponent;
class UWidgetComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHChapterTerminalConfirmedDelegate);

UCLASS()
class ASHESOFHEAVEN_API AAHChapterTerminal : public AActor, public IAHInteractable, public IAHSavableActor
{
	GENERATED_BODY()

public:
	AAHChapterTerminal();
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> TerminalMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UWidgetComponent> TerminalWidget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHPersistentIdComponent> PersistentIdComponent;

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
	/** Writes one of the authored WBP_TerminalWorld text blocks. Returns false if it is missing. */
	bool SetScreenText(FName WidgetName, const FText& Value);
	bool IsInspected() const { return bInspected; }
	void SetPersistentId(const FGuid& PersistentId);

	virtual FGuid GetPersistentId_Implementation() const override;
	virtual int32 GetWorldStateVersion_Implementation() const override { return 1; }
	virtual EAHWorldStateSerializationMode GetWorldStateSerializationMode_Implementation() const override { return EAHWorldStateSerializationMode::ExplicitPayload; }
	virtual bool ShouldSaveWorldTransform_Implementation() const override { return false; }
	virtual bool CaptureWorldState_Implementation(TArray<uint8>& OutPayload) const override;
	virtual bool RestoreWorldState_Implementation(const TArray<uint8>& Payload, int32 SavedStateVersion) override;
	virtual void OnWorldStateRestored_Implementation() override;

private:
	void MarkWorldStateDirty();

	bool bInspected = false;
	bool bConfirmed = false;
};
