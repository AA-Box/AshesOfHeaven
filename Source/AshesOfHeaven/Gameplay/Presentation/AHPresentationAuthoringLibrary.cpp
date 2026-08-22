#include "Gameplay/Presentation/AHPresentationAuthoringLibrary.h"

#if WITH_EDITOR

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SafeZone.h"
#include "Components/TextBlock.h"
#include "EditorAssetLibrary.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "WidgetBlueprintFactory.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Styling/SlateColor.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "UObject/UnrealType.h"
#include "Animation/WidgetAnimation.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieScene.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "NiagaraEmitter.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterFactoryNew.h"
#include "NiagaraSystemFactoryNew.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UWidgetBlueprint* CreateWidgetBlueprint(const FString& Name, const FString& Path, UClass* ParentClass)
	{
		const FString AssetPath = Path / Name;
		if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
		{
			UEditorAssetLibrary::DeleteAsset(AssetPath);
		}
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = ParentClass;
		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(FAssetToolsModule::GetModule().Get().CreateAsset(Name, Path, UWidgetBlueprint::StaticClass(), Factory));
		if (Blueprint)
		{
			// Existing Phase 4.1 assets were native AHHUDRootWidget wrappers. Set the
			// authored parent explicitly so regeneration cannot inherit that layout contract.
			Blueprint->ParentClass = ParentClass;
		}
		return Blueprint;
	}

	void SaveWidgetBlueprint(UWidgetBlueprint* Blueprint)
	{
		if (!Blueprint)
		{
			return;
		}
		// The factory creates a default root and seeds its variable GUID map. We replace
		// that root with an authored hierarchy. Leave the map empty so the UMG compiler's
		// own fix-up path creates deterministic GUID entries for both widgets and animations.
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		UEditorAssetLibrary::SaveAsset(Blueprint->GetPathName(), false);
	}

	void SetText(UTextBlock* Text, const FString& Value, int32 Size, const FLinearColor& Color, ETextJustify::Type Justification = ETextJustify::Left)
	{
		if (!Text)
		{
			return;
		}
		Text->SetText(FText::FromString(Value));
		Text->SetColorAndOpacity(FSlateColor(Color));
		FSlateFontInfo Font = Text->GetFont();
		Font.Size = Size;
		Text->SetFont(Font);
		Text->SetJustification(Justification);
	}

	UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FMargin& Offsets, const FAnchors& Anchors = FAnchors(0.f, 0.f, 0.f, 0.f), const FVector2D& Alignment = FVector2D::ZeroVector)
	{
		if (!Canvas || !Widget)
		{
			return nullptr;
		}
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(Alignment);
		return Slot;
	}

	UTextBlock* AddText(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name, const TCHAR* Value, int32 Size, const FLinearColor& Color, const FMargin& Offsets, ETextJustify::Type Justification = ETextJustify::Left)
	{
		UTextBlock* Text = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Name));
		SetText(Text, Value, Size, Color, Justification);
		Place(Canvas, Text, Offsets);
		return Text;
	}

	UProgressBar* AddProgress(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name, const FLinearColor& Color, const FMargin& Offsets)
	{
		UProgressBar* Bar = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), FName(Name));
		Bar->SetFillColorAndOpacity(Color);
		Bar->SetPercent(1.f);
		Place(Canvas, Bar, Offsets);
		return Bar;
	}

	UBorder* AddRule(UWidgetTree* Tree, UCanvasPanel* Canvas, const TCHAR* Name, const FLinearColor& Color, const FMargin& Offsets)
	{
		UBorder* Rule = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), FName(Name));
		Rule->SetBrushColor(Color);
		Place(Canvas, Rule, Offsets);
		return Rule;
	}

	UWidgetAnimation* AddOpacityAnimation(UWidgetBlueprint* Blueprint, const TCHAR* Name, const TCHAR* TargetWidget, float Duration, float StartValue, float PeakValue, float EndValue)
	{
		if (!Blueprint || !Blueprint->WidgetTree || !TargetWidget)
		{
			return nullptr;
		}
		UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(Blueprint, FName(Name), RF_Transactional);
		if (!Animation)
		{
			return nullptr;
		}
		Animation->Rename(Name);
		Animation->SetDisplayLabel(Name);
		Animation->MovieScene = NewObject<UMovieScene>(Animation, FName(Name), RF_Transactional);
		Animation->MovieScene->SetDisplayRate(FFrameRate(60, 1));
		const int32 EndFrame = FMath::Max(1, FMath::RoundToInt(Duration * 60.0f));
		Animation->MovieScene->SetPlaybackRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(EndFrame + 1)));
		Animation->MovieScene->GetEditorData().WorkStart = 0.0;
		Animation->MovieScene->GetEditorData().WorkEnd = Duration;

		const FGuid BindingGuid = Animation->MovieScene->AddPossessable(TargetWidget, UUserWidget::StaticClass());
		FWidgetAnimationBinding Binding;
		Binding.WidgetName = FName(TargetWidget);
		Binding.AnimationGuid = BindingGuid;
		Animation->AnimationBindings.Add(Binding);
		UMovieSceneFloatTrack* Track = Animation->MovieScene->AddTrack<UMovieSceneFloatTrack>(BindingGuid);
		if (!Track)
		{
			return Animation;
		}
		Track->SetPropertyNameAndPath(TEXT("RenderOpacity"), TEXT("RenderOpacity"));
		UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
		if (Section)
		{
			Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(EndFrame + 1)));
			Track->AddSection(*Section);
			FMovieSceneFloatChannel* Channel = Section->GetChannelProxy().GetChannel<FMovieSceneFloatChannel>(0);
			if (Channel)
			{
				Channel->GetData().AddKey(FFrameNumber(0), FMovieSceneFloatValue(StartValue));
				Channel->GetData().AddKey(FFrameNumber(FMath::Max(1, EndFrame / 2)), FMovieSceneFloatValue(PeakValue));
				Channel->GetData().AddKey(FFrameNumber(EndFrame), FMovieSceneFloatValue(EndValue));
			}
		}
		Blueprint->Animations.Add(Animation);
		return Animation;
	}

	void AuthorReticle(UWidgetBlueprint* Blueprint, const FLinearColor& Bone)
	{
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			return;
		}
		UCanvasPanel* Canvas = Cast<UCanvasPanel>(Blueprint->WidgetTree->RootWidget);
		if (!Canvas)
		{
			return;
		}
		const FLinearColor Transparent = FLinearColor(Bone.R, Bone.G, Bone.B, 0.96f);
		AddRule(Blueprint->WidgetTree, Canvas, TEXT("CrosshairCore"), Transparent, FMargin(-2.f, -2.f, 4.f, 4.f));
		AddRule(Blueprint->WidgetTree, Canvas, TEXT("CrosshairTop"), Transparent, FMargin(-1.f, -17.f, 2.f, 10.f));
		AddRule(Blueprint->WidgetTree, Canvas, TEXT("CrosshairBottom"), Transparent, FMargin(-1.f, 7.f, 2.f, 10.f));
		AddRule(Blueprint->WidgetTree, Canvas, TEXT("CrosshairLeft"), Transparent, FMargin(-17.f, -1.f, 10.f, 2.f));
		AddRule(Blueprint->WidgetTree, Canvas, TEXT("CrosshairRight"), Transparent, FMargin(7.f, -1.f, 10.f, 2.f));
		AddRule(Blueprint->WidgetTree, Canvas, TEXT("CrosshairHit"), FLinearColor(0.94f, 0.62f, 0.22f, 1.f), FMargin(-5.f, -5.f, 10.f, 10.f));
		UTextBlock* LegacyGlyph = AddText(Blueprint->WidgetTree, Canvas, TEXT("Crosshair"), TEXT(""), 1, FLinearColor::Transparent, FMargin(0.f, 0.f, 1.f, 1.f));
		LegacyGlyph->SetVisibility(ESlateVisibility::Collapsed);
	}

	UWidgetBlueprint* MakeSimpleWidget(const FString& Name, const FString& Path, const TArray<TTuple<FString, FString, int32, FLinearColor, FMargin, ETextJustify::Type>>& Texts, const FLinearColor& Accent)
	{
		UWidgetBlueprint* Blueprint = CreateWidgetBlueprint(Name, Path, UUserWidget::StaticClass());
		if (!Blueprint || !Blueprint->WidgetTree)
		{
			return nullptr;
		}
		UCanvasPanel* Canvas = Blueprint->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Layout"));
		Blueprint->WidgetTree->RootWidget = Canvas;
		if (Name == TEXT("WBP_Crosshair"))
		{
			AuthorReticle(Blueprint, Accent);
		}
		else
		{
			if (Name == TEXT("WBP_DamageIndicator"))
			{
				AddRule(Blueprint->WidgetTree, Canvas, TEXT("DamageRule"), FLinearColor(0.82f, 0.14f, 0.10f, 1.f), FMargin(0.f, 4.f, 3.f, 24.f));
			}
			for (const TTuple<FString, FString, int32, FLinearColor, FMargin, ETextJustify::Type>& Spec : Texts)
			{
				const FMargin TextOffsets = Name == TEXT("WBP_DamageIndicator")
					? FMargin(18.f, 0.f, 420.f, 32.f) : Spec.Get<4>();
				AddText(Blueprint->WidgetTree, Canvas, *Spec.Get<0>(), *Spec.Get<1>(), Spec.Get<2>(), Spec.Get<3>(), TextOffsets, Spec.Get<5>());
			}
		}
		if (Name == TEXT("WBP_ManticoreHUD"))
		{
			AddProgress(Blueprint->WidgetTree, Canvas, TEXT("ManticoreHealth"), FLinearColor(0.94f, 0.62f, 0.22f, 1.f), FMargin(0.f, 34.f, 238.f, 7.f));
		}
		SaveWidgetBlueprint(Blueprint);
		return Blueprint;
	}

	void AddRootChild(UWidgetBlueprint* Root, UCanvasPanel* Canvas, const TCHAR* Name, UClass* ChildClass, const FMargin& Offsets, const FAnchors& Anchors, const FVector2D& Alignment)
	{
		if (Root && Root->WidgetTree && Canvas && ChildClass)
		{
			UUserWidget* Child = Root->WidgetTree->ConstructWidget<UUserWidget>(ChildClass, FName(Name));
			// BindWidget properties are resolved from generated widget variables. Mark the
			// authored composition children as variables so the UMG compiler can bind the
			// native presentation contract without requiring a hand-edited designer pass.
			Child->bIsVariable = true;
			Place(Canvas, Child, Offsets, Anchors, Alignment);
		}
	}
}

bool UAHPresentationAuthoringLibrary::AuthorPhase42Widgets()
{
	const FString HUDPath = TEXT("/Game/Ashes/UI/HUD");
	const FLinearColor Bone(0.84f, 0.85f, 0.81f, 1.f);
	const FLinearColor Muted(0.48f, 0.53f, 0.53f, 1.f);
	const FLinearColor Amber(0.94f, 0.62f, 0.22f, 1.f);
	const FLinearColor Cyan(0.42f, 0.68f, 0.71f, 1.f);
	const FLinearColor Red(0.82f, 0.14f, 0.10f, 1.f);
	bool bSuccess = true;

	{
		UWidgetBlueprint* BP = CreateWidgetBlueprint(TEXT("WBP_Objective"), HUDPath, UUserWidget::StaticClass());
		if (BP && BP->WidgetTree)
		{
			UCanvasPanel* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Layout"));
			BP->WidgetTree->RootWidget = Canvas;
			UBorder* Rule = BP->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ObjectiveRule"));
			Rule->SetBrushColor(Amber);
			Place(Canvas, Rule, FMargin(0.f, 0.f, 3.f, 64.f));
			AddText(BP->WidgetTree, Canvas, TEXT("ObjectiveIndex"), TEXT("OBJECTIVE"), 13, Muted, FMargin(18.f, 0.f, 520.f, 22.f));
			AddText(BP->WidgetTree, Canvas, TEXT("ObjectiveText"), TEXT("AWAITING ORDERS"), 22, Bone, FMargin(18.f, 24.f, 620.f, 36.f));
			SaveWidgetBlueprint(BP);
		}
		else { bSuccess = false; }
	}

	{
		UWidgetBlueprint* BP = CreateWidgetBlueprint(TEXT("WBP_PlayerStatus"), HUDPath, UUserWidget::StaticClass());
		if (BP && BP->WidgetTree)
		{
			UCanvasPanel* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Layout"));
			BP->WidgetTree->RootWidget = Canvas;
			AddText(BP->WidgetTree, Canvas, TEXT("ArmorValue"), TEXT("ARMOR  100"), 13, Cyan, FMargin(0.f, 0.f, 240.f, 20.f));
			AddProgress(BP->WidgetTree, Canvas, TEXT("ArmorBar"), Cyan, FMargin(0.f, 22.f, 238.f, 7.f));
			AddText(BP->WidgetTree, Canvas, TEXT("HealthValue"), TEXT("VITALS  100"), 13, Amber, FMargin(0.f, 34.f, 240.f, 20.f));
			AddProgress(BP->WidgetTree, Canvas, TEXT("HealthBar"), Amber, FMargin(0.f, 56.f, 238.f, 7.f));
			SaveWidgetBlueprint(BP);
		}
		else { bSuccess = false; }
	}

	{
		UWidgetBlueprint* BP = CreateWidgetBlueprint(TEXT("WBP_WeaponStatus"), HUDPath, UUserWidget::StaticClass());
		if (BP && BP->WidgetTree)
		{
			UCanvasPanel* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Layout"));
			BP->WidgetTree->RootWidget = Canvas;
			AddText(BP->WidgetTree, Canvas, TEXT("WeaponName"), TEXT("M91 // REVENANT"), 13, Muted, FMargin(0.f, 0.f, 318.f, 20.f), ETextJustify::Right);
			AddText(BP->WidgetTree, Canvas, TEXT("Ammo"), TEXT("30  /  120"), 30, Bone, FMargin(0.f, 20.f, 318.f, 40.f), ETextJustify::Right);
			AddText(BP->WidgetTree, Canvas, TEXT("Grenades"), TEXT("FRAG  03"), 13, Amber, FMargin(0.f, 62.f, 318.f, 22.f), ETextJustify::Right);
			SaveWidgetBlueprint(BP);
		}
		else { bSuccess = false; }
	}

	struct FSimpleSpec { const TCHAR* Name; TArray<TTuple<FString, FString, int32, FLinearColor, FMargin, ETextJustify::Type>> Texts; };
	const TArray<FSimpleSpec> SimpleSpecs = {
		{ TEXT("WBP_Crosshair"), { MakeTuple(FString(TEXT("Crosshair")), FString(), 1, FLinearColor::Transparent, FMargin(0.f, 0.f, 1.f, 1.f), ETextJustify::Center) } },
		{ TEXT("WBP_InteractionPrompt"), { MakeTuple(FString(TEXT("InteractionPrompt")), FString(), 16, Amber, FMargin(0.f, 0.f, 520.f, 32.f), ETextJustify::Center) } },
		{ TEXT("WBP_DamageIndicator"), { MakeTuple(FString(TEXT("DamageIndicator")), FString(), 16, Red, FMargin(0.f, 0.f, 420.f, 32.f), ETextJustify::Center) } },
		{ TEXT("WBP_Countdown"), { MakeTuple(FString(TEXT("Countdown")), FString(), 24, Amber, FMargin(0.f, 0.f, 220.f, 38.f), ETextJustify::Right) } },
		{ TEXT("WBP_Dialogue"), { MakeTuple(FString(TEXT("DialogueSpeaker")), FString(), 14, Amber, FMargin(0.f, 0.f, 800.f, 22.f), ETextJustify::Center), MakeTuple(FString(TEXT("DialogueSubtitle")), FString(), 20, Bone, FMargin(0.f, 24.f, 800.f, 52.f), ETextJustify::Center) } },
		{ TEXT("WBP_TerminalIntel"), { MakeTuple(FString(TEXT("TerminalIntel")), FString(TEXT("PLANETARY FAILSAFE // AUTHORIZATION REQUIRED")), 16, Bone, FMargin(20.f, 10.f, 600.f, 30.f), ETextJustify::Left), MakeTuple(FString(TEXT("TerminalStatus")), FString(TEXT("CONFIRM")), 24, Amber, FMargin(20.f, 48.f, 600.f, 40.f), ETextJustify::Left) } },
		{ TEXT("WBP_ChapterTitle"), { MakeTuple(FString(TEXT("MissionComplete")), FString(TEXT("ASHES OF HEAVEN\nCHAPTER ONE COMPLETE")), 30, Amber, FMargin(0.f, 0.f, 720.f, 120.f), ETextJustify::Center) } },
		{ TEXT("WBP_ManticoreHUD"), { MakeTuple(FString(TEXT("ManticoreHUD")), FString(), 17, Amber, FMargin(0.f, 0.f, 360.f, 30.f), ETextJustify::Left) } },
	};
	for (const FSimpleSpec& Spec : SimpleSpecs)
	{
		bSuccess &= MakeSimpleWidget(Spec.Name, HUDPath, Spec.Texts, Bone) != nullptr;
	}

	UWidgetBlueprint* Root = CreateWidgetBlueprint(TEXT("WBP_HUD_Root"), HUDPath, LoadClass<UUserWidget>(nullptr, TEXT("/Script/AshesOfHeaven.AHHUDRootWidget")));
	if (Root && Root->WidgetTree)
	{
		USafeZone* SafeZone = Root->WidgetTree->ConstructWidget<USafeZone>(USafeZone::StaticClass(), TEXT("HUDSafeZone"));
		UCanvasPanel* Canvas = Root->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("HUDLayout"));
		Root->WidgetTree->RootWidget = SafeZone;
		if (SafeZone)
		{
			SafeZone->AddChild(Canvas);
		}
		const TArray<TTuple<const TCHAR*, const TCHAR*, FMargin, FAnchors, FVector2D>> Children = {
			MakeTuple(TEXT("ObjectiveWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Objective.WBP_Objective_C"), FMargin(42.f, 34.f, 660.f, 72.f), FAnchors(0.f, 0.f), FVector2D::ZeroVector),
			MakeTuple(TEXT("CountdownWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Countdown.WBP_Countdown_C"), FMargin(-262.f, 34.f, 220.f, 38.f), FAnchors(1.f, 0.f), FVector2D::ZeroVector),
			MakeTuple(TEXT("PlayerStatusWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_PlayerStatus.WBP_PlayerStatus_C"), FMargin(42.f, -72.f, 238.f, 64.f), FAnchors(0.f, 1.f), FVector2D(0.f, 1.f)),
			MakeTuple(TEXT("WeaponStatusWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_WeaponStatus.WBP_WeaponStatus_C"), FMargin(-360.f, -88.f, 318.f, 88.f), FAnchors(1.f, 1.f), FVector2D(0.f, 1.f)),
			MakeTuple(TEXT("CrosshairWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Crosshair.WBP_Crosshair_C"), FMargin(-24.f, -20.f, 48.f, 40.f), FAnchors(0.5f, 0.5f), FVector2D::ZeroVector),
			MakeTuple(TEXT("InteractionWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_InteractionPrompt.WBP_InteractionPrompt_C"), FMargin(-260.f, 78.f, 520.f, 32.f), FAnchors(0.5f, 0.5f), FVector2D::ZeroVector),
			MakeTuple(TEXT("DamageIndicatorWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_DamageIndicator.WBP_DamageIndicator_C"), FMargin(-210.f, 126.f, 420.f, 32.f), FAnchors(0.5f, 0.f), FVector2D::ZeroVector),
			MakeTuple(TEXT("DialogueWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Dialogue.WBP_Dialogue_C"), FMargin(-400.f, -176.f, 800.f, 80.f), FAnchors(0.5f, 1.f), FVector2D(0.f, 1.f)),
			MakeTuple(TEXT("ChapterTitleWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_ChapterTitle.WBP_ChapterTitle_C"), FMargin(-360.f, -60.f, 720.f, 120.f), FAnchors(0.5f, 0.5f), FVector2D::ZeroVector),
			MakeTuple(TEXT("ManticoreWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_ManticoreHUD.WBP_ManticoreHUD_C"), FMargin(42.f, -184.f, 360.f, 48.f), FAnchors(0.f, 1.f), FVector2D(0.f, 1.f)),
		};
		for (const auto& Child : Children)
		{
			FString BlueprintPath = Child.Get<1>();
			BlueprintPath = BlueprintPath.LeftChop(BlueprintPath.Len() - BlueprintPath.Find(TEXT(".")));
			UClass* ChildClass = UEditorAssetLibrary::LoadBlueprintClass(BlueprintPath);
			if (!ChildClass)
			{
				UE_LOG(LogTemp, Warning, TEXT("[Phase4.3][UI] Could not load authored child class %s"), *BlueprintPath);
			}
			AddRootChild(Root, Canvas, Child.Get<0>(), ChildClass, Child.Get<2>(), Child.Get<3>(), Child.Get<4>());
		}
		AddOpacityAnimation(Root, TEXT("ObjectiveRevealAnimation"), TEXT("ObjectiveWidget"), 0.42f, 0.0f, 1.0f, 1.0f);
		AddOpacityAnimation(Root, TEXT("DamagePulseAnimation"), TEXT("DamageIndicatorWidget"), 0.22f, 0.0f, 1.0f, 0.0f);
		AddOpacityAnimation(Root, TEXT("CountdownUrgencyAnimation"), TEXT("CountdownWidget"), 0.34f, 0.62f, 1.0f, 0.62f);
		SaveWidgetBlueprint(Root);
	}
	else { bSuccess = false; }

	const FString TerminalPath = TEXT("/Game/Ashes/UI/Terminal");
	UWidgetBlueprint* Terminal = CreateWidgetBlueprint(TEXT("WBP_TerminalWorld"), TerminalPath, UUserWidget::StaticClass());
	if (Terminal && Terminal->WidgetTree)
	{
		UCanvasPanel* Canvas = Terminal->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("TerminalLayout"));
		Terminal->WidgetTree->RootWidget = Canvas;
		UBorder* Rule = Terminal->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TerminalRule"));
		Rule->SetBrushColor(Cyan);
		Place(Canvas, Rule, FMargin(0.f, 0.f, 3.f, 120.f));
		AddText(Terminal->WidgetTree, Canvas, TEXT("TerminalIntel"), TEXT("PLANETARY FAILSAFE // AUTHORIZATION REQUIRED"), 16, Bone, FMargin(20.f, 10.f, 600.f, 30.f));
		AddText(Terminal->WidgetTree, Canvas, TEXT("TerminalStatus"), TEXT("CONFIRM"), 24, Amber, FMargin(20.f, 48.f, 600.f, 40.f));
		SaveWidgetBlueprint(Terminal);
	}
	else { bSuccess = false; }

	return bSuccess;
}

bool UAHPresentationAuthoringLibrary::AuthorPhase42Niagara()
{
	const FString VFXPath = TEXT("/Game/Ashes/VFX");
	const FString EmitterPath = VFXPath / TEXT("Emitters");
	const TArray<FString> Effects = {
		TEXT("AshField"), TEXT("EmberDrift"), TEXT("ImpactSparks"), TEXT("FireSmall"),
		TEXT("FireLarge"), TEXT("SmokeColumn"), TEXT("DustSheet"), TEXT("CathedralMotes")
	};
	bool bSuccess = true;
	const TArray<FString> LegacyEffects = { TEXT("NS_Ash"), TEXT("NS_Embers"), TEXT("NS_Sparks"), TEXT("NS_Dust"), TEXT("NS_CathedralParticles") };
	for (const FString& LegacyEffect : LegacyEffects)
	{
		const FString LegacyPath = VFXPath / LegacyEffect;
		if (UEditorAssetLibrary::DoesAssetExist(LegacyPath))
		{
			UEditorAssetLibrary::DeleteAsset(LegacyPath);
		}
	}

	for (const FString& Effect : Effects)
	{
		const FString EmitterAssetPath = EmitterPath / (TEXT("NE_") + Effect);
		const FString SystemAssetPath = VFXPath / (TEXT("NS_") + Effect);
		// Remove prior template duplicates so regeneration always leaves only authored
		// emitters and systems in the project namespace.
		if (UEditorAssetLibrary::DoesAssetExist(SystemAssetPath))
		{
			UEditorAssetLibrary::DeleteAsset(SystemAssetPath);
		}
		if (UEditorAssetLibrary::DoesAssetExist(EmitterAssetPath))
		{
			UEditorAssetLibrary::DeleteAsset(EmitterAssetPath);
		}

		UNiagaraEmitterFactoryNew* EmitterFactory = NewObject<UNiagaraEmitterFactoryNew>();
		EmitterFactory->bAddDefaultModulesAndRenderersToEmptyEmitter = true;
		UNiagaraEmitter* Emitter = Cast<UNiagaraEmitter>(FAssetToolsModule::GetModule().Get().CreateAsset(
			TEXT("NE_") + Effect, EmitterPath, UNiagaraEmitter::StaticClass(), EmitterFactory));
		if (!Emitter)
		{
			UE_LOG(LogTemp, Error, TEXT("[Phase4.3][VFX] failed to author emitter %s"), *Effect);
			bSuccess = false;
			continue;
		}
		Emitter->SetUniqueEmitterName(TEXT("AH_") + Effect);
		Emitter->Category = FText::FromString(TEXT("Ashes of Heaven / Phase 4.3"));
		Emitter->TemplateAssetDescription = FText::FromString(TEXT("Authored presentation emitter; no engine template dependency."));
		if (FVersionedNiagaraEmitterData* EmitterData = Emitter->GetLatestEmitterData())
		{
			EmitterData->bLocalSpace = Effect == TEXT("EmberDrift") || Effect == TEXT("ImpactSparks") || Effect == TEXT("FireSmall") || Effect == TEXT("FireLarge");
			EmitterData->bDeterminism = true;
			EmitterData->RandomSeed = 417 + Effects.IndexOfByKey(Effect);
			EmitterData->bRequiresPersistentIDs = Effect.Contains(TEXT("Smoke")) || Effect.Contains(TEXT("Ash"));
			EmitterData->Importance = Effect.Contains(TEXT("Impact")) || Effect.Contains(TEXT("Fire"))
				? ENiagaraEmitterImportance::Critical : ENiagaraEmitterImportance::Normal;
			EmitterData->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
			const float Bound = Effect.Contains(TEXT("Cathedral")) ? 900.0f : Effect.Contains(TEXT("Smoke")) ? 650.0f : 360.0f;
			EmitterData->FixedBounds = FBox(FVector(-Bound, -Bound, -Bound), FVector(Bound, Bound, Bound));
			EmitterData->AllocationMode = EParticleAllocationMode::ManualEstimate;
			EmitterData->PreAllocationCount = Effect.Contains(TEXT("Smoke")) || Effect.Contains(TEXT("Ash")) ? 256 : 64;

			// Keep the starter emitter editable, but give each effect a deliberate renderer identity
			// instead of leaving every factory-created emitter on the same default presentation.
			UMaterialInterface* EffectMaterial = nullptr;
			if (Effect.Contains(TEXT("Cathedral")))
			{
				EffectMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph"));
			}
			else if (Effect.Contains(TEXT("Fire")) || Effect.Contains(TEXT("Ember")) || Effect.Contains(TEXT("Impact")))
			{
				EffectMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph"));
			}
			else
			{
				EffectMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_Grime.M_Grime"));
			}
			for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
			{
				if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
				{
					Sprite->Material = EffectMaterial;
					Sprite->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
					Sprite->Alignment = Effect.Contains(TEXT("Impact")) ? ENiagaraSpriteAlignment::VelocityAligned : ENiagaraSpriteAlignment::Unaligned;
					Sprite->SortMode = Effect.Contains(TEXT("Smoke")) || Effect.Contains(TEXT("Ash")) ? ENiagaraSortMode::ViewDepth : ENiagaraSortMode::None;
				}
			}
		}
		UEditorAssetLibrary::SaveAsset(EmitterAssetPath, false);

		UNiagaraSystemFactoryNew* SystemFactory = NewObject<UNiagaraSystemFactoryNew>();
		SystemFactory->EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(Emitter, Emitter->GetExposedVersion().VersionGuid));
		UNiagaraSystem* System = Cast<UNiagaraSystem>(FAssetToolsModule::GetModule().Get().CreateAsset(
			TEXT("NS_") + Effect, VFXPath, UNiagaraSystem::StaticClass(), SystemFactory));
		if (!System)
		{
			UE_LOG(LogTemp, Error, TEXT("[Phase4.3][VFX] failed to author system %s"), *Effect);
			bSuccess = false;
			continue;
		}
		System->Category = FText::FromString(TEXT("Ashes of Heaven / Phase 4.3"));
		if (FBoolProperty* Determinism = FindFProperty<FBoolProperty>(System->GetClass(), TEXT("bDeterminism")))
		{
			Determinism->SetPropertyValue_InContainer(System, true);
		}
		if (FIntProperty* RandomSeed = FindFProperty<FIntProperty>(System->GetClass(), TEXT("RandomSeed")))
		{
			RandomSeed->SetPropertyValue_InContainer(System, 417 + Effects.IndexOfByKey(Effect));
		}
		if (FBoolProperty* FixedTick = FindFProperty<FBoolProperty>(System->GetClass(), TEXT("bFixedTickDelta")))
		{
			FixedTick->SetPropertyValue_InContainer(System, true);
		}
		if (FFloatProperty* FixedTickDelta = FindFProperty<FFloatProperty>(System->GetClass(), TEXT("FixedTickDeltaTime")))
		{
			FixedTickDelta->SetPropertyValue_InContainer(System, 1.0f / 60.0f);
		}
		System->SetFixedBounds(FBox(FVector(-900.0f), FVector(900.0f)));
		UEditorAssetLibrary::SaveAsset(SystemAssetPath, false);
	}

	return bSuccess;
}

#else

bool UAHPresentationAuthoringLibrary::AuthorPhase42Widgets()
{
	return false;
}

bool UAHPresentationAuthoringLibrary::AuthorPhase42Niagara()
{
	return false;
}

#endif
