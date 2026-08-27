#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AHCombatHUD.generated.h"

class UAHHUDRootWidget;
class APawn;

UCLASS()
class ASHESOFHEAVEN_API AAHCombatHUD : public AHUD
{
	GENERATED_BODY()

public:
	AAHCombatHUD();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void DrawHUD() override;

	void SetPossessedPawn(APawn* NewPawn);
	void ShowHitMarker(bool bHeadshot);
	void ShowDamageFeedback(bool bArmorBreak, float DirectionAngle);
	void SetObjective(const FText& NewObjective, int32 NewIndex, int32 Count);
	void ShowMissionComplete();
	void ShowMissionFailed(const FText& Headline);
	void HideMissionComplete();
	const FText& GetCurrentObjective() const { return CurrentObjective; }
	int32 GetObjectiveIndex() const { return ObjectiveIndex; }
	int32 GetObjectiveCount() const { return ObjectiveCount; }
	bool IsMissionCompleteDisplayed() const { return bMissionComplete; }
	UAHHUDRootWidget* GetRootWidget() const { return RootWidget; }

private:
	UPROPERTY(Transient)
	TObjectPtr<UAHHUDRootWidget> RootWidget;

	bool bMissionComplete = false;
	FText CurrentObjective;
	int32 ObjectiveIndex = 0;
	int32 ObjectiveCount = 5;
};
