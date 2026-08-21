#include "Gameplay/UI/AHHUDRootWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
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
#include "Engine/Engine.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor Bone(0.84f, 0.85f, 0.81f, 1.0f);
	const FLinearColor Muted(0.48f, 0.53f, 0.53f, 1.0f);
	const FLinearColor Amber(0.94f, 0.62f, 0.22f, 1.0f);
	const FLinearColor Red(0.82f, 0.14f, 0.10f, 1.0f);
	const FLinearColor Cyan(0.42f, 0.68f, 0.71f, 1.0f);

	UTextBlock* MakeText(UWidgetTree* Tree, FName Name, const FText& Text, int32 Size, const FLinearColor& Color)
	{
		UTextBlock* Result = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Result->SetText(Text);
		Result->SetColorAndOpacity(Color);
		FSlateFontInfo Font = Result->GetFont();
		Font.Size = Size;
		Result->SetFont(Font);
		Result->SetShadowOffset(FVector2D(1.0f, 1.0f));
		Result->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
		return Result;
	}

	void Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors, const FMargin& Offsets, const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget))
		{
			Slot->SetAnchors(Anchors);
			Slot->SetOffsets(Offsets);
			Slot->SetAlignment(Alignment);
		}
	}
}

void UAHHUDRootWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildNativeLayout();
	bPresentationReady = LayoutRoot != nullptr;
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
		}
	}
	Super::NativeDestruct();
}

void UAHHUDRootWidget::BuildNativeLayout()
{
	if (!WidgetTree || LayoutRoot)
	{
		return;
	}
	LayoutRoot = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!LayoutRoot)
	{
		LayoutRoot = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDLayout"));
		WidgetTree->RootWidget = LayoutRoot;
	}

	ObjectiveIndexText = MakeText(WidgetTree, TEXT("ObjectiveIndex"), NSLOCTEXT("AshesHUD", "ObjectiveIndex", "OBJECTIVE"), 14, Muted);
	ObjectiveText = MakeText(WidgetTree, TEXT("ObjectiveText"), NSLOCTEXT("AshesHUD", "AwaitingOrders", "AWAITING ORDERS"), 24, Bone);
	Place(LayoutRoot, ObjectiveIndexText, FAnchors(0.0f, 0.0f, 0.0f, 0.0f), FMargin(42.0f, 34.0f, 420.0f, 22.0f));
	Place(LayoutRoot, ObjectiveText, FAnchors(0.0f, 0.0f, 0.0f, 0.0f), FMargin(42.0f, 56.0f, 640.0f, 42.0f));

	CountdownText = MakeText(WidgetTree, TEXT("Countdown"), FText::GetEmpty(), 26, Amber);
	CountdownText->SetJustification(ETextJustify::Right);
	Place(LayoutRoot, CountdownText, FAnchors(1.0f, 0.0f, 1.0f, 0.0f), FMargin(-250.0f, 38.0f, 208.0f, 38.0f));

	ArmorValueText = MakeText(WidgetTree, TEXT("ArmorValue"), FText::GetEmpty(), 14, Cyan);
	HealthValueText = MakeText(WidgetTree, TEXT("HealthValue"), FText::GetEmpty(), 14, Amber);
	ArmorBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ArmorBar"));
	HealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
	Place(LayoutRoot, ArmorValueText, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FMargin(42.0f, -142.0f, 110.0f, 24.0f));
	Place(LayoutRoot, ArmorBar, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FMargin(42.0f, -116.0f, 238.0f, 8.0f));
	Place(LayoutRoot, HealthValueText, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FMargin(42.0f, -94.0f, 110.0f, 24.0f));
	Place(LayoutRoot, HealthBar, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FMargin(42.0f, -68.0f, 238.0f, 8.0f));

	WeaponNameText = MakeText(WidgetTree, TEXT("WeaponName"), FText::GetEmpty(), 14, Muted);
	AmmoText = MakeText(WidgetTree, TEXT("Ammo"), FText::GetEmpty(), 32, Bone);
	GrenadeText = MakeText(WidgetTree, TEXT("Grenades"), FText::GetEmpty(), 14, Amber);
	WeaponNameText->SetJustification(ETextJustify::Right);
	AmmoText->SetJustification(ETextJustify::Right);
	GrenadeText->SetJustification(ETextJustify::Right);
	Place(LayoutRoot, WeaponNameText, FAnchors(1.0f, 1.0f, 1.0f, 1.0f), FMargin(-360.0f, -142.0f, 318.0f, 24.0f));
	Place(LayoutRoot, AmmoText, FAnchors(1.0f, 1.0f, 1.0f, 1.0f), FMargin(-360.0f, -112.0f, 318.0f, 44.0f));
	Place(LayoutRoot, GrenadeText, FAnchors(1.0f, 1.0f, 1.0f, 1.0f), FMargin(-360.0f, -66.0f, 318.0f, 24.0f));

	CrosshairText = MakeText(WidgetTree, TEXT("Crosshair"), NSLOCTEXT("AshesHUD", "Crosshair", "+"), 20, Bone);
	CrosshairText->SetJustification(ETextJustify::Center);
	Place(LayoutRoot, CrosshairText, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FMargin(-24.0f, -20.0f, 48.0f, 40.0f), FVector2D(0.0f, 0.0f));

	InteractionText = MakeText(WidgetTree, TEXT("InteractionPrompt"), FText::GetEmpty(), 18, Amber);
	InteractionText->SetJustification(ETextJustify::Center);
	Place(LayoutRoot, InteractionText, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FMargin(-260.0f, 78.0f, 520.0f, 32.0f));

	DamageText = MakeText(WidgetTree, TEXT("DamageIndicator"), FText::GetEmpty(), 18, Red);
	DamageText->SetJustification(ETextJustify::Center);
	DamageText->SetVisibility(ESlateVisibility::Collapsed);
	Place(LayoutRoot, DamageText, FAnchors(0.5f, 0.0f, 0.5f, 0.0f), FMargin(-200.0f, 130.0f, 400.0f, 32.0f));

	DialogueSpeakerText = MakeText(WidgetTree, TEXT("DialogueSpeaker"), FText::GetEmpty(), 16, Amber);
	DialogueSubtitleText = MakeText(WidgetTree, TEXT("DialogueSubtitle"), FText::GetEmpty(), 22, Bone);
	Place(LayoutRoot, DialogueSpeakerText, FAnchors(0.5f, 1.0f, 0.5f, 1.0f), FMargin(-400.0f, -174.0f, 800.0f, 24.0f));
	Place(LayoutRoot, DialogueSubtitleText, FAnchors(0.5f, 1.0f, 0.5f, 1.0f), FMargin(-400.0f, -146.0f, 800.0f, 48.0f));

	MissionCompleteText = MakeText(WidgetTree, TEXT("MissionComplete"), NSLOCTEXT("AshesHUD", "ChapterComplete", "ASHES OF HEAVEN\nCHAPTER ONE COMPLETE"), 30, Amber);
	MissionCompleteText->SetJustification(ETextJustify::Center);
	MissionCompleteText->SetVisibility(ESlateVisibility::Collapsed);
	Place(LayoutRoot, MissionCompleteText, FAnchors(0.5f, 0.5f, 0.5f, 0.5f), FMargin(-360.0f, -60.0f, 720.0f, 120.0f));

	VehicleText = MakeText(WidgetTree, TEXT("ManticoreHUD"), FText::GetEmpty(), 18, Amber);
	VehicleText->SetVisibility(ESlateVisibility::Collapsed);
	VehicleHealthBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ManticoreHealth"));
	VehicleHealthBar->SetVisibility(ESlateVisibility::Collapsed);
	Place(LayoutRoot, VehicleText, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FMargin(42.0f, -184.0f, 360.0f, 32.0f));
	Place(LayoutRoot, VehicleHealthBar, FAnchors(0.0f, 1.0f, 0.0f, 1.0f), FMargin(42.0f, -150.0f, 238.0f, 8.0f));
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
		SetText(WeaponNameText, Weapon->DisplayName);
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

void UAHHUDRootWidget::PlayPresentationAnimation(UWidgetAnimation* Animation)
{
	if (Animation)
	{
		PlayAnimation(Animation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f);
	}
}

void UAHHUDRootWidget::SetObjective(const FText& Objective, int32 Index, int32 Count)
{
	const bool bChanged = !CurrentObjective.EqualTo(Objective) || CurrentObjectiveIndex != Index;
	CurrentObjective = Objective;
	CurrentObjectiveIndex = Index;
	CurrentObjectiveCount = Count;
	SetText(ObjectiveIndexText, FText::FromString(FString::Printf(TEXT("OBJECTIVE %02d"), Index + 1)));
	SetText(ObjectiveText, Objective.IsEmpty() ? NSLOCTEXT("AshesHUD", "AwaitingOrders2", "AWAITING ORDERS") : Objective);
	if (bChanged)
	{
		PlayPresentationAnimation(ObjectiveRevealAnimation);
	}
}

void UAHHUDRootWidget::ShowHitMarker(bool bHeadshot)
{
	SetText(CrosshairText, bHeadshot ? NSLOCTEXT("AshesHUD", "Headshot", "◆") : NSLOCTEXT("AshesHUD", "Hit", "+") );
	SetText(DamageText, bHeadshot ? NSLOCTEXT("AshesHUD", "HeadshotLabel", "HEADSHOT") : NSLOCTEXT("AshesHUD", "HitLabel", "HIT"));
		ApplyVisibility(DamageText, true);
	PlayPresentationAnimation(DamagePulseAnimation);
	if (GetWorld())
	{
		FTimerHandle Timer;
		GetWorld()->GetTimerManager().SetTimer(Timer, [WeakThis = TWeakObjectPtr<UAHHUDRootWidget>(this)]()
		{
			if (UAHHUDRootWidget* Widget = WeakThis.Get()) Widget->ApplyVisibility(Widget->DamageText, false);
		}, 0.22f, false);
	}
}

void UAHHUDRootWidget::ShowDamageFeedback(bool bArmorBreak, float DirectionAngle)
{
	const FString Direction = DirectionAngle < -35.0f ? TEXT("◀  ") : DirectionAngle > 35.0f ? TEXT("  ▶") : TEXT("  ▲");
	SetText(DamageText, FText::FromString(Direction + (bArmorBreak ? TEXT("ARMOR BREACH") : TEXT("DAMAGE"))));
	ApplyVisibility(DamageText, true);
	PlayPresentationAnimation(DamagePulseAnimation);
}

void UAHHUDRootWidget::ShowMissionComplete()
{
	ApplyVisibility(MissionCompleteText, true);
	PlayPresentationAnimation(ObjectiveRevealAnimation);
}

void UAHHUDRootWidget::SetCrosshairState(bool bAimingDownSights, float Spread, bool bHit, bool bHeadshot, bool bInteraction, bool bVehicle)
{
	if (bVehicle)
	{
		SetText(CrosshairText, NSLOCTEXT("AshesHUD", "VehicleReticle", "◇"));
	}
	else if (bInteraction)
	{
		SetText(CrosshairText, NSLOCTEXT("AshesHUD", "InteractionReticle", "·"));
	}
	else if (!bHit)
	{
		SetText(CrosshairText, bAimingDownSights ? NSLOCTEXT("AshesHUD", "ADSReticle", "·") : NSLOCTEXT("AshesHUD", "HipReticle", "+"));
	}
	if (CrosshairText)
	{
		FSlateFontInfo Font = CrosshairText->GetFont();
		Font.Size = bAimingDownSights ? 14 : FMath::Clamp(16 + FMath::RoundToInt(Spread * 2.0f), 16, 28);
		CrosshairText->SetFont(Font);
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
	}
}

void UAHHUDRootWidget::HandleVehiclePresentationChanged()
{
	RefreshVehicleState(Cast<AAHManticoreVehicle>(PossessedPawn.Get()));
}
