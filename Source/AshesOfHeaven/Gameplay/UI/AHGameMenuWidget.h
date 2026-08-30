#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AHGameMenuWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;

UENUM()
enum class EAHMenuMode : uint8
{
	FrontEnd,
	Pause
};

UENUM()
enum class EAHMenuPage : uint8
{
	Root,
	Controls,
	Options
};

/**
 * The full-screen game menu: the boot front end (START / CONTINUE / CONTROLS /
 * OPTIONS / EXIT) and the in-game ESC pause menu (RESUME / RESTART CHECKPOINT /
 * CONTROLS / OPTIONS / EXIT). Built entirely from C++ in the HUD target's design
 * language (ink ground, bone type, amber accents, letterspaced small caps).
 * Owned and driven by AAHCombatPlayerController.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHGameMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void InitializeMenu(EAHMenuMode InMode);
	void ShowPage(EAHMenuPage Page);
	EAHMenuMode GetMode() const { return Mode; }
	EAHMenuPage GetPage() const { return CurrentPage; }

	/** Simulates activating a root action by name; the test/console path into the same handlers. */
	bool ActivateAction(const FString& ActionName);

	virtual bool NativeSupportsKeyboardFocus() const override { return true; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	void BuildTree();
	UWidget* BuildRootPage();
	UWidget* BuildControlsPage();
	UWidget* BuildOptionsPage();
	UButton* MakeMenuButton(const FText& Label, FName Action, UVerticalBox* Container);
	UTextBlock* MakeText(const FText& Value, float Size, const FLinearColor& Color, float LetterSpacing = 300.0f);
	void RefreshOptionRows();

	UFUNCTION() void HandlePrimary();
	UFUNCTION() void HandleContinue();
	UFUNCTION() void HandleRestartCheckpoint();
	UFUNCTION() void HandleControls();
	UFUNCTION() void HandleOptions();
	UFUNCTION() void HandleExit();
	UFUNCTION() void HandleBack();
	UFUNCTION() void HandleCycleWindowMode();
	UFUNCTION() void HandleCycleQuality();
	UFUNCTION() void HandleResolutionDown();
	UFUNCTION() void HandleResolutionUp();
	UFUNCTION() void HandleToggleInvertLook();

	class AAHCombatPlayerController* GetOwningCombatController() const;

	EAHMenuMode Mode = EAHMenuMode::FrontEnd;
	EAHMenuPage CurrentPage = EAHMenuPage::Root;

	UPROPERTY(Transient) TObjectPtr<UWidgetSwitcher> PageSwitcher;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> WindowModeValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> QualityValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> ResolutionValue;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> InvertLookValue;
	UPROPERTY(Transient) TObjectPtr<UButton> ContinueButton;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> PrimaryLabel;
};
