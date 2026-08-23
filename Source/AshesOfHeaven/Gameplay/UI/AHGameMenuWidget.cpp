#include "AHGameMenuWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "GameFramework/GameUserSettings.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Brushes/SlateColorBrush.h"
#include "Components/SizeBox.h"
#include "Styling/SlateTypes.h"

namespace AHMenuStyle
{
	// HUD target palette (References/HUDTargets): BONE, AMBER, COOL, ALERT over INK.
	const FLinearColor Ink = FLinearColor::FromSRGBColor(FColor(0x0D, 0x11, 0x14));
	const FLinearColor Bone = FLinearColor::FromSRGBColor(FColor(0xD8, 0xD1, 0x8F));
	const FLinearColor Amber = FLinearColor::FromSRGBColor(FColor(0xC2, 0x8A, 0x4B));
	const FLinearColor Cool = FLinearColor::FromSRGBColor(FColor(0x8F, 0xA1, 0xA3));

	FSlateFontInfo Font(float Size, bool bBold = false)
	{
		FSlateFontInfo Info = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		return Info;
	}
}

void UAHGameMenuWidget::InitializeMenu(EAHMenuMode InMode)
{
	Mode = InMode;
	ShowPage(EAHMenuPage::Root);
}

TSharedRef<SWidget> UAHGameMenuWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

UTextBlock* UAHGameMenuWidget::MakeText(const FText& Value, float Size, const FLinearColor& Color, float LetterSpacing)
{
	UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	FSlateFontInfo Font = AHMenuStyle::Font(Size);
	Font.LetterSpacing = LetterSpacing;
	Text->SetFont(Font);
	Text->SetText(Value);
	Text->SetColorAndOpacity(FSlateColor(Color));
	return Text;
}

UButton* UAHGameMenuWidget::MakeMenuButton(const FText& Label, FName Action, UVerticalBox* Container)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	FButtonStyle Style;
	Style.SetNormal(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
	Style.SetHovered(FSlateColorBrush(FLinearColor(AHMenuStyle::Amber.R, AHMenuStyle::Amber.G, AHMenuStyle::Amber.B, 0.14f)));
	Style.SetPressed(FSlateColorBrush(FLinearColor(AHMenuStyle::Amber.R, AHMenuStyle::Amber.G, AHMenuStyle::Amber.B, 0.26f)));
	Style.NormalPadding = FMargin(18.0f, 10.0f);
	Style.PressedPadding = FMargin(18.0f, 10.0f);
	Button->SetStyle(Style);

	UTextBlock* Text = MakeText(Label, 20.0f, AHMenuStyle::Bone, 420.0f);
	Button->AddChild(Text);
	if (UButtonSlot* ButtonSlot = Cast<UButtonSlot>(Text->Slot))
	{
		ButtonSlot->SetHorizontalAlignment(HAlign_Left);
	}

	UVerticalBoxSlot* BoxSlot = Container->AddChildToVerticalBox(Button);
	BoxSlot->SetPadding(FMargin(0.0f, 4.0f));

	if (Action == TEXT("Primary")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandlePrimary); PrimaryLabel = Text; }
	else if (Action == TEXT("Continue")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleContinue); ContinueButton = Button; }
	else if (Action == TEXT("Restart")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleRestartCheckpoint); }
	else if (Action == TEXT("Controls")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleControls); }
	else if (Action == TEXT("Options")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleOptions); }
	else if (Action == TEXT("Exit")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleExit); }
	else if (Action == TEXT("Back")) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleBack); }
	return Button;
}

void UAHGameMenuWidget::BuildTree()
{
	UOverlay* RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("MenuRoot"));
	WidgetTree->RootWidget = RootOverlay;

	// Ground: opaque ink for the front end, smoked glass over the paused world.
	UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MenuBackground"));
	FLinearColor Ground = AHMenuStyle::Ink;
	Ground.A = (Mode == EAHMenuMode::FrontEnd) ? 1.0f : 0.84f;
	Background->SetBrushColor(Ground);
	UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(Background);
	BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
	BackgroundSlot->SetVerticalAlignment(VAlign_Fill);

	// Left column: identity, rule, pages.
	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("MenuColumn"));
	UOverlaySlot* ColumnSlot = RootOverlay->AddChildToOverlay(Column);
	ColumnSlot->SetHorizontalAlignment(HAlign_Left);
	ColumnSlot->SetVerticalAlignment(VAlign_Center);
	ColumnSlot->SetPadding(FMargin(140.0f, 0.0f, 0.0f, 0.0f));

	UTextBlock* Title = MakeText(FText::FromString(TEXT("ASHES OF HEAVEN")), 44.0f, AHMenuStyle::Bone, 900.0f);
	Column->AddChildToVerticalBox(Title);
	UTextBlock* Subtitle = MakeText(
		FText::FromString(Mode == EAHMenuMode::FrontEnd ? TEXT("CHAPTER ONE  /  EREBUS") : TEXT("PAUSED  /  EREBUS")),
		13.0f, AHMenuStyle::Amber, 700.0f);
	UVerticalBoxSlot* SubtitleSlot = Column->AddChildToVerticalBox(Subtitle);
	SubtitleSlot->SetPadding(FMargin(2.0f, 6.0f, 0.0f, 0.0f));

	USizeBox* RuleBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	RuleBox->SetWidthOverride(280.0f);
	RuleBox->SetHeightOverride(2.0f);
	UBorder* Rule = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Rule->SetBrushColor(AHMenuStyle::Amber);
	RuleBox->AddChild(Rule);
	UVerticalBoxSlot* RuleSlot = Column->AddChildToVerticalBox(RuleBox);
	RuleSlot->SetPadding(FMargin(2.0f, 18.0f, 0.0f, 26.0f));
	RuleSlot->SetHorizontalAlignment(HAlign_Left);

	USizeBox* PageBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	PageBox->SetWidthOverride(700.0f);
	PageSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("MenuPages"));
	PageBox->AddChild(PageSwitcher);
	UVerticalBoxSlot* PageSlot = Column->AddChildToVerticalBox(PageBox);
	PageSlot->SetHorizontalAlignment(HAlign_Left);
	PageSwitcher->AddChild(BuildRootPage());
	PageSwitcher->AddChild(BuildControlsPage());
	PageSwitcher->AddChild(BuildOptionsPage());

	UTextBlock* Footer = MakeText(
		FText::FromString(Mode == EAHMenuMode::FrontEnd
			? TEXT("EXPEDITION 9  /  DEVELOPMENT BUILD")
			: TEXT("ESC  /  RETURN TO THE FIELD")),
		11.0f, AHMenuStyle::Cool, 500.0f);
	UVerticalBoxSlot* FooterSlot = Column->AddChildToVerticalBox(Footer);
	FooterSlot->SetPadding(FMargin(2.0f, 34.0f, 0.0f, 0.0f));
}

UWidget* UAHGameMenuWidget::BuildRootPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PageRoot"));
	const bool bHasSave = GetGameInstance() && GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>()
		&& GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>()->HasSave();

	if (Mode == EAHMenuMode::FrontEnd)
	{
		if (bHasSave)
		{
			MakeMenuButton(FText::FromString(TEXT("CONTINUE")), TEXT("Continue"), Page);
			MakeMenuButton(FText::FromString(TEXT("NEW EXPEDITION")), TEXT("Primary"), Page);
		}
		else
		{
			MakeMenuButton(FText::FromString(TEXT("BEGIN EXPEDITION")), TEXT("Primary"), Page);
		}
	}
	else
	{
		MakeMenuButton(FText::FromString(TEXT("RESUME")), TEXT("Continue"), Page);
		MakeMenuButton(FText::FromString(TEXT("RESTART CHECKPOINT")), TEXT("Restart"), Page);
	}
	MakeMenuButton(FText::FromString(TEXT("CONTROLS")), TEXT("Controls"), Page);
	MakeMenuButton(FText::FromString(TEXT("OPTIONS")), TEXT("Options"), Page);
	MakeMenuButton(FText::FromString(Mode == EAHMenuMode::FrontEnd ? TEXT("EXIT") : TEXT("EXIT TO DESKTOP")), TEXT("Exit"), Page);
	return Page;
}

UWidget* UAHGameMenuWidget::BuildControlsPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PageControls"));
	const TCHAR* Rows[][2] = {
		{TEXT("MOVE"), TEXT("W  A  S  D")},
		{TEXT("LOOK"), TEXT("MOUSE")},
		{TEXT("FIRE"), TEXT("LEFT MOUSE")},
		{TEXT("AIM DOWN SIGHTS"), TEXT("RIGHT MOUSE")},
		{TEXT("RELOAD"), TEXT("R")},
		{TEXT("SPRINT"), TEXT("LEFT SHIFT")},
		{TEXT("JUMP"), TEXT("SPACE")},
		{TEXT("CROUCH"), TEXT("LEFT CTRL")},
		{TEXT("MELEE"), TEXT("V")},
		{TEXT("GRENADE"), TEXT("G")},
		{TEXT("INTERACT"), TEXT("E")},
		{TEXT("MENU"), TEXT("ESC")},
	};
	for (const auto& Row : Rows)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Action = MakeText(FText::FromString(Row[0]), 15.0f, AHMenuStyle::Cool, 400.0f);
		UTextBlock* Key = MakeText(FText::FromString(Row[1]), 15.0f, AHMenuStyle::Bone, 400.0f);
		UHorizontalBoxSlot* ActionSlot = Line->AddChildToHorizontalBox(Action);
		ActionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		Line->AddChildToHorizontalBox(Key);
		UVerticalBoxSlot* LineSlot = Page->AddChildToVerticalBox(Line);
		LineSlot->SetPadding(FMargin(2.0f, 5.0f, 40.0f, 5.0f));
	}
	MakeMenuButton(FText::FromString(TEXT("BACK")), TEXT("Back"), Page);
	return Page;
}

UWidget* UAHGameMenuWidget::BuildOptionsPage()
{
	UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PageOptions"));

	auto AddOptionRow = [this, Page](const TCHAR* Label, TObjectPtr<UTextBlock>& ValueOut, TFunction<void(UButton*, bool)> Bind)
	{
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UTextBlock* Name = MakeText(FText::FromString(Label), 15.0f, AHMenuStyle::Cool, 400.0f);
		UHorizontalBoxSlot* NameSlot = Line->AddChildToHorizontalBox(Name);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		auto MakeArrow = [this](const TCHAR* Glyph)
		{
			UButton* Arrow = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
			FButtonStyle Style;
			Style.SetNormal(FSlateColorBrush(FLinearColor(0.0f, 0.0f, 0.0f, 0.0f)));
			Style.SetHovered(FSlateColorBrush(FLinearColor(AHMenuStyle::Amber.R, AHMenuStyle::Amber.G, AHMenuStyle::Amber.B, 0.2f)));
			Style.SetPressed(FSlateColorBrush(FLinearColor(AHMenuStyle::Amber.R, AHMenuStyle::Amber.G, AHMenuStyle::Amber.B, 0.3f)));
			Style.NormalPadding = FMargin(10.0f, 2.0f);
			Style.PressedPadding = FMargin(10.0f, 2.0f);
			Arrow->SetStyle(Style);
			Arrow->AddChild(MakeText(FText::FromString(Glyph), 15.0f, AHMenuStyle::Amber, 0.0f));
			return Arrow;
		};
		UButton* Left = MakeArrow(TEXT("<"));
		Line->AddChildToHorizontalBox(Left);
		ValueOut = MakeText(FText::GetEmpty(), 15.0f, AHMenuStyle::Bone, 300.0f);
		UHorizontalBoxSlot* ValueSlot = Line->AddChildToHorizontalBox(ValueOut);
		ValueSlot->SetPadding(FMargin(14.0f, 0.0f));
		ValueSlot->SetHorizontalAlignment(HAlign_Center);
		UButton* Right = MakeArrow(TEXT(">"));
		Line->AddChildToHorizontalBox(Right);
		Bind(Left, false);
		Bind(Right, true);
		UVerticalBoxSlot* LineSlot = Page->AddChildToVerticalBox(Line);
		LineSlot->SetPadding(FMargin(2.0f, 7.0f, 40.0f, 7.0f));
	};

	AddOptionRow(TEXT("WINDOW MODE"), WindowModeValue, [this](UButton* Button, bool)
	{
		Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleCycleWindowMode);
	});
	AddOptionRow(TEXT("QUALITY"), QualityValue, [this](UButton* Button, bool)
	{
		Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleCycleQuality);
	});
	AddOptionRow(TEXT("RESOLUTION SCALE"), ResolutionValue, [this](UButton* Button, bool bRight)
	{
		if (bRight) { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleResolutionUp); }
		else { Button->OnClicked.AddDynamic(this, &UAHGameMenuWidget::HandleResolutionDown); }
	});

	MakeMenuButton(FText::FromString(TEXT("BACK")), TEXT("Back"), Page);
	RefreshOptionRows();
	return Page;
}

void UAHGameMenuWidget::RefreshOptionRows()
{
	UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
	if (!Settings)
	{
		return;
	}
	if (WindowModeValue)
	{
		const EWindowMode::Type ModeValue = Settings->GetFullscreenMode();
		WindowModeValue->SetText(FText::FromString(
			ModeValue == EWindowMode::Fullscreen ? TEXT("FULLSCREEN") :
			ModeValue == EWindowMode::WindowedFullscreen ? TEXT("BORDERLESS") : TEXT("WINDOWED")));
	}
	if (QualityValue)
	{
		static const TCHAR* Names[] = {TEXT("LOW"), TEXT("MEDIUM"), TEXT("HIGH"), TEXT("EPIC"), TEXT("CINEMATIC")};
		const int32 Level = Settings->GetOverallScalabilityLevel();
		QualityValue->SetText(FText::FromString(
			(Level >= 0 && Level <= 4) ? Names[Level] : TEXT("CUSTOM")));
	}
	if (ResolutionValue)
	{
		// Out params: normalized [0..1], value [%], min [%], max [%] — display the percent value.
		float Normalized = 0.0f, ScaleValue = 100.0f, MinScale = 0.0f, MaxScale = 0.0f;
		Settings->GetResolutionScaleInformationEx(Normalized, ScaleValue, MinScale, MaxScale);
		ResolutionValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(ScaleValue))));
	}
}

void UAHGameMenuWidget::ShowPage(EAHMenuPage Page)
{
	CurrentPage = Page;
	if (PageSwitcher)
	{
		PageSwitcher->SetActiveWidgetIndex(static_cast<int32>(Page));
	}
	if (Page == EAHMenuPage::Options)
	{
		RefreshOptionRows();
	}
}

AAHCombatPlayerController* UAHGameMenuWidget::GetOwningCombatController() const
{
	return Cast<AAHCombatPlayerController>(GetOwningPlayer());
}

bool UAHGameMenuWidget::ActivateAction(const FString& ActionName)
{
	if (ActionName.Equals(TEXT("Primary"), ESearchCase::IgnoreCase)) { HandlePrimary(); return true; }
	if (ActionName.Equals(TEXT("Continue"), ESearchCase::IgnoreCase)) { HandleContinue(); return true; }
	if (ActionName.Equals(TEXT("Restart"), ESearchCase::IgnoreCase)) { HandleRestartCheckpoint(); return true; }
	if (ActionName.Equals(TEXT("Controls"), ESearchCase::IgnoreCase)) { HandleControls(); return true; }
	if (ActionName.Equals(TEXT("Options"), ESearchCase::IgnoreCase)) { HandleOptions(); return true; }
	if (ActionName.Equals(TEXT("Back"), ESearchCase::IgnoreCase)) { HandleBack(); return true; }
	if (ActionName.Equals(TEXT("Exit"), ESearchCase::IgnoreCase)) { HandleExit(); return true; }
	return false;
}

FReply UAHGameMenuWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape)
	{
		if (CurrentPage != EAHMenuPage::Root)
		{
			HandleBack();
			return FReply::Handled();
		}
		if (Mode == EAHMenuMode::Pause)
		{
			HandleContinue();
			return FReply::Handled();
		}
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

namespace
{
	void PlayMenuClick(const UUserWidget* Widget)
	{
		if (UWorld* World = Widget ? Widget->GetWorld() : nullptr)
		{
			if (UAHAudioSubsystem* Audio = World->GetSubsystem<UAHAudioSubsystem>())
			{
				Audio->PlayUICue(EAHAudioCue::Objective, 0.55f, 1.15f);
			}
		}
	}
}

void UAHGameMenuWidget::HandlePrimary()
{
	PlayMenuClick(this);
	if (AAHCombatPlayerController* Controller = GetOwningCombatController())
	{
		Controller->MenuStartNewGame();
	}
}

void UAHGameMenuWidget::HandleContinue()
{
	PlayMenuClick(this);
	if (AAHCombatPlayerController* Controller = GetOwningCombatController())
	{
		Controller->CloseGameMenu();
	}
}

void UAHGameMenuWidget::HandleRestartCheckpoint()
{
	PlayMenuClick(this);
	if (AAHCombatPlayerController* Controller = GetOwningCombatController())
	{
		Controller->CloseGameMenu();
		Controller->ReloadCheckpoint();
	}
}

void UAHGameMenuWidget::HandleControls()
{
	PlayMenuClick(this);
	ShowPage(EAHMenuPage::Controls);
}

void UAHGameMenuWidget::HandleOptions()
{
	PlayMenuClick(this);
	ShowPage(EAHMenuPage::Options);
}

void UAHGameMenuWidget::HandleBack()
{
	PlayMenuClick(this);
	ShowPage(EAHMenuPage::Root);
}

void UAHGameMenuWidget::HandleExit()
{
	PlayMenuClick(this);
	if (AAHCombatPlayerController* Controller = GetOwningCombatController())
	{
		Controller->MenuExitGame();
	}
}

void UAHGameMenuWidget::HandleCycleWindowMode()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const EWindowMode::Type Current = Settings->GetFullscreenMode();
		const EWindowMode::Type Next =
			Current == EWindowMode::Fullscreen ? EWindowMode::WindowedFullscreen :
			Current == EWindowMode::WindowedFullscreen ? EWindowMode::Windowed : EWindowMode::Fullscreen;
		Settings->SetFullscreenMode(Next);
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	PlayMenuClick(this);
	RefreshOptionRows();
}

void UAHGameMenuWidget::HandleCycleQuality()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		const int32 Next = (FMath::Clamp(Settings->GetOverallScalabilityLevel(), 0, 3) + 1) % 4;
		Settings->SetOverallScalabilityLevel(Next);
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	PlayMenuClick(this);
	RefreshOptionRows();
}

void UAHGameMenuWidget::HandleResolutionDown()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		float Normalized = 0.0f, ScaleValue = 100.0f, MinScale = 0.0f, MaxScale = 0.0f;
		Settings->GetResolutionScaleInformationEx(Normalized, ScaleValue, MinScale, MaxScale);
		Settings->SetResolutionScaleValueEx(FMath::Clamp(ScaleValue - 10.0f, 50.0f, 100.0f));
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	PlayMenuClick(this);
	RefreshOptionRows();
}

void UAHGameMenuWidget::HandleResolutionUp()
{
	if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
	{
		float Normalized = 0.0f, ScaleValue = 100.0f, MinScale = 0.0f, MaxScale = 0.0f;
		Settings->GetResolutionScaleInformationEx(Normalized, ScaleValue, MinScale, MaxScale);
		Settings->SetResolutionScaleValueEx(FMath::Clamp(ScaleValue + 10.0f, 50.0f, 100.0f));
		Settings->ApplySettings(false);
		Settings->SaveSettings();
	}
	PlayMenuClick(this);
	RefreshOptionRows();
}
