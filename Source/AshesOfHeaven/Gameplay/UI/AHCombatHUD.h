#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AHCombatHUD.generated.h"

UCLASS()
class ASHESOFHEAVEN_API AAHCombatHUD : public AHUD
{
	GENERATED_BODY()

public:
	AAHCombatHUD();

	virtual void DrawHUD() override;

	void ShowHitMarker(bool bHeadshot);
	void ShowDamageFeedback(bool bArmorBreak, float DirectionAngle);
	void SetObjective(const FText& NewObjective, int32 NewIndex, int32 Count);
	void ShowMissionComplete();
	const FText& GetCurrentObjective() const { return CurrentObjective; }
	int32 GetObjectiveIndex() const { return ObjectiveIndex; }
	int32 GetObjectiveCount() const { return ObjectiveCount; }
	bool IsMissionCompleteDisplayed() const { return bMissionComplete; }

private:
	void DrawPanel(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const;
	void DrawBar(const FVector2D& Position, const FVector2D& Size, float Percent, const FLinearColor& Color) const;
	void DrawTextAt(const FText& Text, const FVector2D& Position, const FLinearColor& Color, float Scale = 1.0f) const;

	float HitMarkerUntil = 0.0f;
	float DamageFeedbackUntil = 0.0f;
	bool bHitWasHeadshot = false;
	bool bArmorBreak = false;
	float DamageDirectionAngle = 0.0f;
	bool bMissionComplete = false;
	FText CurrentObjective;
	int32 ObjectiveIndex = 0;
	int32 ObjectiveCount = 5;
	float ObjectivePulseUntil = 0.0f;
};
