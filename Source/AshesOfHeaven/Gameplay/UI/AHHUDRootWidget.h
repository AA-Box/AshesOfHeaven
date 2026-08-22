#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AHHUDRootWidget.generated.h"

class AAHCombatPlayerCharacter;
class AAHManticoreVehicle;
class AAHWeaponBase;
class UUserWidget;
class UWidget;
class UTextBlock;
class UBorder;
class UProgressBar;
class UWidgetAnimation;

/**
 * The production HUD contract.  Gameplay publishes state through delegates and this widget
 * owns presentation.  There is deliberately no per-frame gameplay polling or Canvas drawing.
 */
UCLASS(Blueprintable)
class ASHESOFHEAVEN_API UAHHUDRootWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	void SetPossessedPawn(APawn* NewPawn);
	/** Switches the authored root between the opening cinematic and normal gameplay presentation. */
	void SetGameplayPresentationVisible(bool bVisible);
	void SetObjective(const FText& Objective, int32 Index, int32 Count);
	void ShowHitMarker(bool bHeadshot);
	void ShowDamageFeedback(bool bArmorBreak, float DirectionAngle);
	void ShowMissionComplete();
	void HideMissionComplete();

	UFUNCTION(BlueprintCallable, Category="HUD|Presentation")
	void SetCrosshairState(bool bAimingDownSights, float Spread, bool bHit, bool bHeadshot, bool bInteraction, bool bVehicle);

	bool IsPresentationReady() const { return bPresentationReady; }

protected:
	void ResolveAuthoredWidgets();
	void BindPlayerState(AAHCombatPlayerCharacter* Player);
	void UnbindPlayerState();
	void BindWeapon(AAHWeaponBase* Weapon);
	void RefreshPlayerState();
	void RefreshVehicleState(AAHManticoreVehicle* Vehicle);
	void SetText(UTextBlock* Text, const FText& Value) const;
	void ApplyVisibility(UWidget* Widget, bool bVisible) const;
	void SetBar(UProgressBar* Bar, float Percent, const FLinearColor& Color) const;
	void SetReticleVisibility(bool bVisible, bool bAimingDownSights, bool bVehicle, bool bInteraction) const;
	void SetReticleColor(const FLinearColor& Color) const;
	void PlayPresentationAnimation(UWidgetAnimation* Animation);

	UFUNCTION()
	void HandleHealthChanged(float Health, float MaxHealth);
	UFUNCTION()
	void HandleArmorChanged(float Armor, float MaxArmor);
	UFUNCTION()
	void HandleInventoryChanged();
	UFUNCTION()
	void HandleAmmoChanged(const struct FAHAmmoState& Ammo);
	UFUNCTION()
	void HandleWeaponEvent();
	UFUNCTION()
	void HandleInteractionTargetChanged(AActor* Target);
	UFUNCTION()
	void HandleDialogueLine(FName Speaker, FText Subtitle, float Duration);
	UFUNCTION()
	void HandleDialogueSequenceComplete(FName SequenceId);
	UFUNCTION()
	void HandleCountdownChanged(float SecondsRemaining, bool bActive);
	UFUNCTION()
	void HandleChapterStageChanged(enum EAHChapterStage Stage);
	UFUNCTION()
	void HandleVehiclePresentationChanged();

	/** Each child is a real designer-authored Widget Blueprint, not a native-generated layout. */
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> ObjectiveWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> PlayerStatusWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> WeaponStatusWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> CrosshairWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> InteractionWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> DamageIndicatorWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> CountdownWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> DialogueWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> ChapterTitleWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UUserWidget> ManticoreWidget;
	UPROPERTY(meta=(BindWidgetOptional))
	TObjectPtr<UBorder> OpeningCurtain;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ObjectiveIndexText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthValueText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ArmorValueText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ArmorBar;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WeaponNameText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AmmoText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GrenadeText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CrosshairText;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CrosshairCore;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CrosshairTop;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CrosshairBottom;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CrosshairLeft;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CrosshairRight;
	UPROPERTY(Transient)
	TObjectPtr<UWidget> CrosshairHit;
	UPROPERTY(Transient)
	TObjectPtr<UBorder> DamageRule;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> InteractionText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DamageText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueSpeakerText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogueSubtitleText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MissionCompleteText;
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> VehicleText;
	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> VehicleHealthBar;

	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> ObjectiveRevealAnimation;
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> DamagePulseAnimation;
	UPROPERTY(Transient, meta=(BindWidgetAnimOptional))
	TObjectPtr<UWidgetAnimation> CountdownUrgencyAnimation;

	TWeakObjectPtr<APawn> PossessedPawn;
	TWeakObjectPtr<AAHWeaponBase> BoundWeapon;
	FText CurrentObjective;
	int32 CurrentObjectiveIndex = 0;
	int32 CurrentObjectiveCount = 0;
	FTimerHandle ObjectiveMetadataTimer;
	bool bGameplayPresentationVisible = true;
	bool bPresentationReady = false;
};
