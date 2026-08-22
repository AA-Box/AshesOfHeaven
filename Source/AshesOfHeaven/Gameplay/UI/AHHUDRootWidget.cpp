#include "Gameplay/UI/AHHUDRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "GameFramework/PlayerController.h"
#include "UObject/WeakObjectPtr.h"

namespace
{
	const FLinearColor Red(0.82f, 0.14f, 0.10f, 1.0f);
	const FLinearColor Amber(0.94f, 0.62f, 0.22f, 1.0f);
	const FLinearColor Cyan(0.42f, 0.68f, 0.71f, 1.0f);
	const FLinearColor Bone(0.84f, 0.85f, 0.81f, 1.0f);

	template <typename WidgetType>
	WidgetType* FindChildWidget(UUserWidget* Parent, const TCHAR* Name)
	{
		return Parent ? Cast<WidgetType>(Parent->GetWidgetFromName(FName(Name))) : nullptr;
	}
}

void UAHHUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	ResolveAuthoredWidgets();
	bPresentationReady = ObjectiveText && ObjectiveIndexText && HealthValueText && ArmorValueText
		&& HealthBar && ArmorBar && WeaponNameText && AmmoText && GrenadeText && CrosshairText
		&& InteractionText && DamageText && DialogueSpeakerText && DialogueSubtitleText
		&& MissionCompleteText && VehicleText && VehicleHealthBar;
	// Widget Blueprint defaults are layout defaults, not gameplay state. Explicitly clear
	// every transient presentation element before the first delegate can arrive.
	ApplyVisibility(CrosshairText, false);
	ApplyVisibility(CrosshairCore, false);
	ApplyVisibility(CrosshairTop, false);
	ApplyVisibility(CrosshairBottom, false);
	ApplyVisibility(CrosshairLeft, false);
	ApplyVisibility(CrosshairRight, false);
	ApplyVisibility(CrosshairHit, false);
	ApplyVisibility(OpeningCurtain, false);
	ApplyVisibility(MissionCompleteText, false);
	ApplyVisibility(ChapterTitleWidget, false);
	ApplyVisibility(DamageText, false);
	ApplyVisibility(DamageRule, false);
	ApplyVisibility(InteractionText, false);
	ApplyVisibility(DialogueSpeakerText, false);
	ApplyVisibility(DialogueSubtitleText, false);
	ApplyVisibility(CountdownText, false);
	ApplyVisibility(VehicleText, false);
	ApplyVisibility(VehicleHealthBar, false);
	ApplyVisibility(WeaponNameText, false);
	if (UWorld* World = GetWorld())
	{
		if (UAHChapterSubsystem* Chapter = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr)
		{
			Chapter->OnCountdownChanged.AddDynamic(this, &UAHHUDRootWidget::HandleCountdownChanged);
			Chapter->OnStageChanged.AddDynamic(this, &UAHHUDRootWidget::HandleChapterStageChanged);
			HandleCountdownChanged(Chapter->GetCountdownSeconds(), Chapter->IsCountdownActive());
			HandleChapterStageChanged(Chapter->GetStage());
		}
		if (UAHDialogueSubsystem* Dialogue = World->GetSubsystem<UAHDialogueSubsystem>())
		{
			Dialogue->OnLineChanged.AddDynamic(this, &UAHHUDRootWidget::HandleDialogueLine);
			Dialogue->OnSequenceComplete.AddDynamic(this, &UAHHUDRootWidget::HandleDialogueSequenceComplete);
			const bool bOpeningDialogue = Dialogue->HasActiveDialogue() && Dialogue->GetCurrentSequence() == FName(TEXT("Ch01_Opening"));
			SetGameplayPresentationVisible(!bOpeningDialogue);
		}
	}
	if (APlayerController* PC = GetOwningPlayer())
	{
		SetPossessedPawn(PC->GetPawn());
	}
}

void UAHHUDRootWidget::NativeDestruct()
{
	UnbindPlayerState();
	if (UWorld* World = GetWorld())
	{
		if (UAHChapterSubsystem* Chapter = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr)
		{
			Chapter->OnCountdownChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleCountdownChanged);
			Chapter->OnStageChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleChapterStageChanged);
		}
		if (UAHDialogueSubsystem* Dialogue = World->GetSubsystem<UAHDialogueSubsystem>())
		{
			Dialogue->OnLineChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleDialogueLine);
			Dialogue->OnSequenceComplete.RemoveDynamic(this, &UAHHUDRootWidget::HandleDialogueSequenceComplete);
		}
	}
	Super::NativeDestruct();
}

void UAHHUDRootWidget::SetGameplayPresentationVisible(bool bVisible)
{
	bGameplayPresentationVisible = bVisible;
	ApplyVisibility(OpeningCurtain, !bVisible);

	if (!bVisible)
	{
		ApplyVisibility(ObjectiveWidget, false);
		ApplyVisibility(PlayerStatusWidget, false);
		ApplyVisibility(WeaponStatusWidget, false);
		ApplyVisibility(CrosshairWidget, false);
		ApplyVisibility(InteractionWidget, false);
		ApplyVisibility(DamageIndicatorWidget, false);
		ApplyVisibility(CountdownWidget, false);
		ApplyVisibility(ManticoreWidget, false);
		ApplyVisibility(ChapterTitleWidget, false);
		return;
	}

	ApplyVisibility(CrosshairWidget, true);
	ApplyVisibility(PlayerStatusWidget, true);
	ApplyVisibility(WeaponStatusWidget, true);
	ApplyVisibility(ObjectiveWidget, true);
	ApplyVisibility(InteractionWidget, InteractionText == nullptr || !InteractionText->GetText().IsEmpty());
	ApplyVisibility(DialogueWidget, false);
	if (UWorld* World = GetWorld())
	{
		if (UAHChapterSubsystem* Chapter = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr)
		{
			if (Chapter->IsChapterComplete())
			{
				ApplyVisibility(ObjectiveWidget, false);
			}
		}
	}
	RefreshPlayerState();
}

void UAHHUDRootWidget::ResolveAuthoredWidgets()
{
	ObjectiveText = FindChildWidget<UTextBlock>(ObjectiveWidget, TEXT("ObjectiveText"));
	ObjectiveIndexText = FindChildWidget<UTextBlock>(ObjectiveWidget, TEXT("ObjectiveIndex"));
	CountdownText = FindChildWidget<UTextBlock>(CountdownWidget, TEXT("Countdown"));
	ArmorValueText = FindChildWidget<UTextBlock>(PlayerStatusWidget, TEXT("ArmorValue"));
	HealthValueText = FindChildWidget<UTextBlock>(PlayerStatusWidget, TEXT("HealthValue"));
	ArmorBar = FindChildWidget<UProgressBar>(PlayerStatusWidget, TEXT("ArmorBar"));
	HealthBar = FindChildWidget<UProgressBar>(PlayerStatusWidget, TEXT("HealthBar"));
	WeaponNameText = FindChildWidget<UTextBlock>(WeaponStatusWidget, TEXT("WeaponName"));
	AmmoText = FindChildWidget<UTextBlock>(WeaponStatusWidget, TEXT("Ammo"));
	GrenadeText = FindChildWidget<UTextBlock>(WeaponStatusWidget, TEXT("Grenades"));
	CrosshairText = FindChildWidget<UTextBlock>(CrosshairWidget, TEXT("Crosshair"));
	CrosshairCore = FindChildWidget<UWidget>(CrosshairWidget, TEXT("CrosshairCore"));
	CrosshairTop = FindChildWidget<UWidget>(CrosshairWidget, TEXT("CrosshairTop"));
	CrosshairBottom = FindChildWidget<UWidget>(CrosshairWidget, TEXT("CrosshairBottom"));
	CrosshairLeft = FindChildWidget<UWidget>(CrosshairWidget, TEXT("CrosshairLeft"));
	CrosshairRight = FindChildWidget<UWidget>(CrosshairWidget, TEXT("CrosshairRight"));
	CrosshairHit = FindChildWidget<UWidget>(CrosshairWidget, TEXT("CrosshairHit"));
	InteractionText = FindChildWidget<UTextBlock>(InteractionWidget, TEXT("InteractionPrompt"));
	DamageText = FindChildWidget<UTextBlock>(DamageIndicatorWidget, TEXT("DamageIndicator"));
	DamageRule = FindChildWidget<UBorder>(DamageIndicatorWidget, TEXT("DamageRule"));
	DialogueSpeakerText = FindChildWidget<UTextBlock>(DialogueWidget, TEXT("DialogueSpeaker"));
	DialogueSubtitleText = FindChildWidget<UTextBlock>(DialogueWidget, TEXT("DialogueSubtitle"));
	MissionCompleteText = FindChildWidget<UTextBlock>(ChapterTitleWidget, TEXT("MissionComplete"));
	VehicleText = FindChildWidget<UTextBlock>(ManticoreWidget, TEXT("ManticoreHUD"));
	VehicleHealthBar = FindChildWidget<UProgressBar>(ManticoreWidget, TEXT("ManticoreHealth"));
}

void UAHHUDRootWidget::SetPossessedPawn(APawn* NewPawn)
{
	if (PossessedPawn.Get() == NewPawn)
	{
		return;
	}
	UnbindPlayerState();
	PossessedPawn = NewPawn;
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(NewPawn))
	{
		BindPlayerState(Player);
		ApplyVisibility(VehicleText, false);
		ApplyVisibility(VehicleHealthBar, false);
	}
	else if (AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(NewPawn))
	{
		Vehicle->OnPresentationStateChanged.AddDynamic(this, &UAHHUDRootWidget::HandleVehiclePresentationChanged);
		RefreshVehicleState(Vehicle);
	}
}

void UAHHUDRootWidget::BindPlayerState(AAHCombatPlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}
	if (UAHHealthComponent* Health = Player->GetHealthComponent())
	{
		Health->OnHealthChanged.AddDynamic(this, &UAHHUDRootWidget::HandleHealthChanged);
	}
	if (UAHArmorComponent* Armor = Player->GetArmorComponent())
	{
		Armor->OnArmorChanged.AddDynamic(this, &UAHHUDRootWidget::HandleArmorChanged);
	}
	if (UAHInventoryComponent* Inventory = Player->GetInventoryComponent())
	{
		Inventory->OnInventoryChanged.AddDynamic(this, &UAHHUDRootWidget::HandleInventoryChanged);
		if (AAHWeaponBase* Weapon = Inventory->GetCurrentWeapon())
		{
			BindWeapon(Weapon);
		}
	}
	if (UAHInteractionComponent* Interaction = Player->GetInteractionComponent())
	{
		Interaction->OnTargetChanged.AddDynamic(this, &UAHHUDRootWidget::HandleInteractionTargetChanged);
		HandleInteractionTargetChanged(Interaction->GetCurrentTarget());
	}
	RefreshPlayerState();
}

void UAHHUDRootWidget::UnbindPlayerState()
{
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(PossessedPawn.Get()))
	{
		if (Player->GetHealthComponent()) Player->GetHealthComponent()->OnHealthChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleHealthChanged);
		if (Player->GetArmorComponent()) Player->GetArmorComponent()->OnArmorChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleArmorChanged);
		if (Player->GetInventoryComponent()) Player->GetInventoryComponent()->OnInventoryChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleInventoryChanged);
		if (Player->GetInteractionComponent()) Player->GetInteractionComponent()->OnTargetChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleInteractionTargetChanged);
	}
	if (AAHWeaponBase* Weapon = BoundWeapon.Get())
	{
		Weapon->OnAmmoChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleAmmoChanged);
		Weapon->OnReloaded.RemoveDynamic(this, &UAHHUDRootWidget::HandleWeaponEvent);
	}
	if (AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(PossessedPawn.Get()))
	{
		Vehicle->OnPresentationStateChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleVehiclePresentationChanged);
	}
	BoundWeapon.Reset();
}

void UAHHUDRootWidget::BindWeapon(AAHWeaponBase* Weapon)
{
	if (BoundWeapon.Get() == Weapon)
	{
		return;
	}
	if (AAHWeaponBase* Previous = BoundWeapon.Get())
	{
		Previous->OnAmmoChanged.RemoveDynamic(this, &UAHHUDRootWidget::HandleAmmoChanged);
		Previous->OnReloaded.RemoveDynamic(this, &UAHHUDRootWidget::HandleWeaponEvent);
	}
	BoundWeapon = Weapon;
	if (Weapon)
	{
		Weapon->OnAmmoChanged.AddDynamic(this, &UAHHUDRootWidget::HandleAmmoChanged);
		Weapon->OnReloaded.AddDynamic(this, &UAHHUDRootWidget::HandleWeaponEvent);
		HandleAmmoChanged(Weapon->GetAmmoState());
		// The weapon model already identifies the weapon. Keeping its name permanently on screen
		// crowds the lower-right viewmodel and is not part of the gameplay HUD contract.
		SetText(WeaponNameText, FText::GetEmpty());
		ApplyVisibility(WeaponNameText, false);
	}
}

void UAHHUDRootWidget::RefreshPlayerState()
{
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(PossessedPawn.Get());
	if (!Player)
	{
		return;
	}
	HandleHealthChanged(Player->GetHealthComponent() ? Player->GetHealthComponent()->GetHealth() : 0.0f, Player->GetHealthComponent() ? Player->GetHealthComponent()->MaxHealth : 1.0f);
	HandleArmorChanged(Player->GetArmorComponent() ? Player->GetArmorComponent()->GetArmor() : 0.0f, Player->GetArmorComponent() ? Player->GetArmorComponent()->MaxArmor : 1.0f);
	HandleInventoryChanged();
}

void UAHHUDRootWidget::RefreshVehicleState(AAHManticoreVehicle* Vehicle)
{
	if (!Vehicle)
	{
		return;
	}
	ApplyVisibility(VehicleText, true);
	ApplyVisibility(VehicleHealthBar, true);
	SetText(VehicleText, FText::FromString(FString::Printf(TEXT("MANTICORE  //  HULL %03d  //  SPEED %03d"), FMath::RoundToInt(Vehicle->GetHealthPercent() * 100.0f), FMath::RoundToInt(FMath::Abs(Vehicle->GetSpeed())))));
	SetBar(VehicleHealthBar, Vehicle->GetHealthPercent(), Amber);
	ApplyVisibility(HealthBar, false);
	ApplyVisibility(ArmorBar, false);
}

void UAHHUDRootWidget::SetText(UTextBlock* Text, const FText& Value) const
{
	if (Text)
	{
		Text->SetText(Value);
	}
}

void UAHHUDRootWidget::ApplyVisibility(UWidget* Widget, bool bVisible) const
{
	if (Widget)
	{
		Widget->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UAHHUDRootWidget::SetBar(UProgressBar* Bar, float Percent, const FLinearColor& Color) const
{
	if (Bar)
	{
		Bar->SetPercent(FMath::Clamp(Percent, 0.0f, 1.0f));
		Bar->SetFillColorAndOpacity(Color);
	}
}

void UAHHUDRootWidget::SetReticleVisibility(bool bVisible, bool bAimingDownSights, bool bVehicle, bool bInteraction) const
{
	const bool bShowCore = bVisible && (bAimingDownSights || bVehicle || bInteraction);
	const bool bShowArms = bVisible && !bAimingDownSights && !bVehicle && !bInteraction;
	ApplyVisibility(CrosshairCore, bShowCore);
	ApplyVisibility(CrosshairTop, bShowArms);
	ApplyVisibility(CrosshairBottom, bShowArms);
	ApplyVisibility(CrosshairLeft, bShowArms);
	ApplyVisibility(CrosshairRight, bShowArms);
	ApplyVisibility(CrosshairHit, false);
}

void UAHHUDRootWidget::SetReticleColor(const FLinearColor& Color) const
{
	for (UWidget* Widget : { CrosshairCore.Get(), CrosshairTop.Get(), CrosshairBottom.Get(), CrosshairLeft.Get(), CrosshairRight.Get(), CrosshairHit.Get() })
	{
		if (UBorder* Border = Cast<UBorder>(Widget))
		{
			Border->SetBrushColor(Color);
		}
	}
}

void UAHHUDRootWidget::PlayPresentationAnimation(UWidgetAnimation* Animation)
{
	if (Animation)
	{
		PlayAnimation(Animation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);
	}
}

void UAHHUDRootWidget::SetObjective(const FText& Objective, int32 Index, int32 Count)
{
	if (UWorld* World = GetWorld())
	{
		if (UAHChapterSubsystem* Chapter = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr)
		{
			if (Chapter->IsChapterComplete())
			{
				HideMissionComplete();
				return;
			}
		}
	}
	const bool bChanged = !CurrentObjective.EqualTo(Objective) || CurrentObjectiveIndex != Index;
	CurrentObjective = Objective;
	CurrentObjectiveIndex = Index;
	CurrentObjectiveCount = Count;
	ApplyVisibility(ObjectiveWidget, bGameplayPresentationVisible);
	ApplyVisibility(ChapterTitleWidget, false);
	SetText(ObjectiveIndexText, bChanged ? NSLOCTEXT("AshesHUD", "ObjectiveUpdated", "OBJECTIVE UPDATED") : FText::GetEmpty());
	SetText(ObjectiveText, Objective.IsEmpty() ? NSLOCTEXT("AshesHUD", "AwaitingOrders2", "AWAITING ORDERS") : Objective);
	if (bChanged)
	{
		PlayPresentationAnimation(ObjectiveRevealAnimation);
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ObjectiveMetadataTimer);
			World->GetTimerManager().SetTimer(ObjectiveMetadataTimer, [WeakThis = TWeakObjectPtr<UAHHUDRootWidget>(this)]()
			{
				if (UAHHUDRootWidget* Widget = WeakThis.Get())
				{
					Widget->SetText(Widget->ObjectiveIndexText, FText::GetEmpty());
				}
			}, 2.6f, false);
		}
	}
}

void UAHHUDRootWidget::ShowHitMarker(bool bHeadshot)
{
	SetText(DamageText, bHeadshot ? NSLOCTEXT("AshesHUD", "HeadshotLabel", "HEADSHOT CONFIRMED") : NSLOCTEXT("AshesHUD", "HitLabel", "HIT CONFIRMED"));
	ApplyVisibility(CrosshairHit, true);
	if (UBorder* HitBorder = Cast<UBorder>(CrosshairHit))
	{
		HitBorder->SetBrushColor(bHeadshot ? Amber : Bone);
	}
	ApplyVisibility(DamageText, true);
	ApplyVisibility(DamageRule, true);
	ApplyVisibility(DamageIndicatorWidget, true);
	if (DamageRule)
	{
		DamageRule->SetBrushColor(bHeadshot ? Amber : Bone);
	}
	PlayPresentationAnimation(DamagePulseAnimation);
	if (GetWorld())
	{
		FTimerHandle Timer;
		GetWorld()->GetTimerManager().SetTimer(Timer, [WeakThis = TWeakObjectPtr<UAHHUDRootWidget>(this)]()
		{
			if (UAHHUDRootWidget* Widget = WeakThis.Get())
			{
				Widget->ApplyVisibility(Widget->DamageText, false);
				Widget->ApplyVisibility(Widget->DamageRule, false);
				Widget->ApplyVisibility(Widget->DamageIndicatorWidget, false);
				Widget->ApplyVisibility(Widget->CrosshairHit, false);
			}
		}, 0.22f, false);
	}
}

void UAHHUDRootWidget::ShowDamageFeedback(bool bArmorBreak, float DirectionAngle)
{
	const TCHAR* Feedback = bArmorBreak ? TEXT("ARMOR IMPACT") : TEXT("INCOMING FIRE");
	SetText(DamageText, FText::FromString(Feedback));
	ApplyVisibility(DamageText, true);
	if (DamageRule)
	{
		ApplyVisibility(DamageRule, true);
		DamageRule->SetBrushColor(bArmorBreak ? Cyan : Red);
		DamageRule->SetRenderTransformAngle(DirectionAngle);
	}
	ApplyVisibility(DamageIndicatorWidget, true);
	PlayPresentationAnimation(DamagePulseAnimation);
	if (GetWorld())
	{
		FTimerHandle Timer;
		GetWorld()->GetTimerManager().SetTimer(Timer, [WeakThis = TWeakObjectPtr<UAHHUDRootWidget>(this)]()
		{
			if (UAHHUDRootWidget* Widget = WeakThis.Get())
			{
				Widget->ApplyVisibility(Widget->DamageText, false);
				Widget->ApplyVisibility(Widget->DamageRule, false);
				Widget->ApplyVisibility(Widget->DamageIndicatorWidget, false);
			}
		}, 0.75f, false);
	}
}

void UAHHUDRootWidget::ShowMissionComplete()
{
	if (UWorld* World = GetWorld())
	{
		if (UAHChapterSubsystem* Chapter = World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr)
		{
			if (!Chapter->IsChapterComplete())
			{
				HideMissionComplete();
				return;
			}
		}
	}
	ApplyVisibility(ChapterTitleWidget, true);
	ApplyVisibility(MissionCompleteText, true);
	ApplyVisibility(ObjectiveWidget, false);
	ApplyVisibility(InteractionText, false);
	ApplyVisibility(CountdownText, false);
	PlayPresentationAnimation(ObjectiveRevealAnimation);
}

void UAHHUDRootWidget::HideMissionComplete()
{
	ApplyVisibility(MissionCompleteText, false);
	ApplyVisibility(ChapterTitleWidget, false);
}

void UAHHUDRootWidget::SetCrosshairState(bool bAimingDownSights, float Spread, bool bHit, bool bHeadshot, bool bInteraction, bool bVehicle)
{
	SetReticleVisibility(!bHit, bAimingDownSights, bVehicle, bInteraction);
	SetReticleColor(bVehicle ? Cyan : bInteraction ? Amber : Bone);
	const float ReticleScale = bAimingDownSights ? 0.72f : FMath::Clamp(0.88f + Spread * 0.02f, 0.88f, 1.18f);
	for (UWidget* Widget : { CrosshairCore.Get(), CrosshairTop.Get(), CrosshairBottom.Get(), CrosshairLeft.Get(), CrosshairRight.Get() })
	{
		if (Widget)
		{
			Widget->SetRenderScale(FVector2D(ReticleScale, ReticleScale));
		}
	}
}

void UAHHUDRootWidget::HandleHealthChanged(float Health, float MaxHealth)
{
	const float Percent = MaxHealth > 0.0f ? Health / MaxHealth : 0.0f;
	SetBar(HealthBar, Percent, Percent < 0.25f ? Red : Amber);
	SetText(HealthValueText, FText::FromString(FString::Printf(TEXT("VITALS  %03d"), FMath::RoundToInt(FMath::Clamp(Percent, 0.0f, 1.0f) * 100.0f))));
}

void UAHHUDRootWidget::HandleArmorChanged(float Armor, float MaxArmor)
{
	const float Percent = MaxArmor > 0.0f ? Armor / MaxArmor : 0.0f;
	SetBar(ArmorBar, Percent, Cyan);
	SetText(ArmorValueText, FText::FromString(FString::Printf(TEXT("ARMOR  %03d"), FMath::RoundToInt(FMath::Clamp(Percent, 0.0f, 1.0f) * 100.0f))));
}

void UAHHUDRootWidget::HandleInventoryChanged()
{
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(PossessedPawn.Get()))
	{
		if (UAHInventoryComponent* Inventory = Player->GetInventoryComponent())
		{
			SetText(GrenadeText, FText::FromString(FString::Printf(TEXT("FRAG  %02d"), Inventory->GetGrenades())));
			BindWeapon(Inventory->GetCurrentWeapon());
		}
	}
}

void UAHHUDRootWidget::HandleAmmoChanged(const FAHAmmoState& Ammo)
{
	SetText(AmmoText, FText::FromString(FString::Printf(TEXT("%02d  /  %03d"), Ammo.Magazine, Ammo.Reserve)));
}

void UAHHUDRootWidget::HandleWeaponEvent()
{
	if (AAHWeaponBase* Weapon = BoundWeapon.Get())
	{
		HandleAmmoChanged(Weapon->GetAmmoState());
	}
}

void UAHHUDRootWidget::HandleInteractionTargetChanged(AActor* Target)
{
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(PossessedPawn.Get()))
	{
		SetText(InteractionText, Player->GetInteractionComponent() ? Player->GetInteractionComponent()->GetPrompt() : FText::GetEmpty());
		ApplyVisibility(InteractionText, !InteractionText->GetText().IsEmpty());
	}
}

void UAHHUDRootWidget::HandleDialogueLine(FName Speaker, FText Subtitle, float Duration)
{
	SetText(DialogueSpeakerText, FText::FromName(Speaker));
	SetText(DialogueSubtitleText, Subtitle);
	ApplyVisibility(DialogueSpeakerText, !Speaker.IsNone());
	ApplyVisibility(DialogueSubtitleText, !Subtitle.IsEmpty());
	ApplyVisibility(DialogueWidget, !Speaker.IsNone() || !Subtitle.IsEmpty());
}

void UAHHUDRootWidget::HandleDialogueSequenceComplete(FName SequenceId)
{
	SetText(DialogueSpeakerText, FText::GetEmpty());
	SetText(DialogueSubtitleText, FText::GetEmpty());
	ApplyVisibility(DialogueSpeakerText, false);
	ApplyVisibility(DialogueSubtitleText, false);
	ApplyVisibility(DialogueWidget, false);
	if (SequenceId == FName(TEXT("Ch01_Opening")))
	{
		SetGameplayPresentationVisible(true);
	}
}

void UAHHUDRootWidget::HandleCountdownChanged(float SecondsRemaining, bool bActive)
{
	if (!bActive)
	{
		ApplyVisibility(CountdownText, false);
		return;
	}
	const int32 Remaining = FMath::Max(0, FMath::CeilToInt(SecondsRemaining));
	SetText(CountdownText, FText::FromString(FString::Printf(TEXT("FAILSAFE  %02d:%02d"), Remaining / 60, Remaining % 60)));
	if (CountdownText)
	{
		CountdownText->SetColorAndOpacity(Remaining <= 10 ? Red : Amber);
	}
	ApplyVisibility(CountdownText, true);
	if (Remaining <= 10)
	{
		PlayPresentationAnimation(CountdownUrgencyAnimation);
	}
}

void UAHHUDRootWidget::HandleChapterStageChanged(EAHChapterStage Stage)
{
	if (Stage == EAHChapterStage::ChapterComplete)
	{
		ShowMissionComplete();
		SetText(ObjectiveText, FText::GetEmpty());
		SetText(ObjectiveIndexText, FText::GetEmpty());
		ApplyVisibility(DialogueSpeakerText, false);
		ApplyVisibility(DialogueSubtitleText, false);
		ApplyVisibility(InteractionText, false);
		ApplyVisibility(CountdownText, false);
	}
	else
	{
		HideMissionComplete();
		ApplyVisibility(ObjectiveWidget, bGameplayPresentationVisible);
	}
}

void UAHHUDRootWidget::HandleVehiclePresentationChanged()
{
	RefreshVehicleState(Cast<AAHManticoreVehicle>(PossessedPawn.Get()));
}
