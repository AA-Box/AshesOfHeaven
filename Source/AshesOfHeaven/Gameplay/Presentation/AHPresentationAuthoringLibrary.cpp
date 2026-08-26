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
#include "NiagaraScript.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitterHandle.h"
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
			AddText(BP->WidgetTree, Canvas, TEXT("ObjectiveIndex"), TEXT("OBJECTIVE UPDATED"), 11, Muted, FMargin(16.f, 0.f, 420.f, 18.f));
			AddText(BP->WidgetTree, Canvas, TEXT("ObjectiveText"), TEXT("AWAITING ORDERS"), 19, Bone, FMargin(16.f, 20.f, 520.f, 30.f));
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
			AddText(BP->WidgetTree, Canvas, TEXT("ArmorValue"), TEXT("ARMOR  100"), 11, Cyan, FMargin(0.f, 0.f, 172.f, 17.f));
			AddProgress(BP->WidgetTree, Canvas, TEXT("ArmorBar"), Cyan, FMargin(0.f, 19.f, 172.f, 5.f));
			AddText(BP->WidgetTree, Canvas, TEXT("HealthValue"), TEXT("VITALS  100"), 11, Amber, FMargin(0.f, 28.f, 172.f, 17.f));
			AddProgress(BP->WidgetTree, Canvas, TEXT("HealthBar"), Amber, FMargin(0.f, 47.f, 172.f, 5.f));
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
			AddText(BP->WidgetTree, Canvas, TEXT("WeaponName"), TEXT(""), 1, FLinearColor::Transparent, FMargin(0.f, 0.f, 1.f, 1.f), ETextJustify::Right);
			AddText(BP->WidgetTree, Canvas, TEXT("Ammo"), TEXT("36  /  180"), 24, Bone, FMargin(0.f, 12.f, 230.f, 32.f), ETextJustify::Right);
			AddText(BP->WidgetTree, Canvas, TEXT("Grenades"), TEXT("FRAG  02"), 11, Amber, FMargin(0.f, 46.f, 230.f, 18.f), ETextJustify::Right);
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
		UBorder* OpeningCurtain = Root->WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OpeningCurtain"));
		OpeningCurtain->bIsVariable = true;
		OpeningCurtain->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 1.f));
		Place(Canvas, OpeningCurtain, FMargin(0.f), FAnchors(0.f, 0.f, 1.f, 1.f), FVector2D::ZeroVector);
		const TArray<TTuple<const TCHAR*, const TCHAR*, FMargin, FAnchors, FVector2D>> Children = {
			MakeTuple(TEXT("ObjectiveWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Objective.WBP_Objective_C"), FMargin(36.f, 28.f, 540.f, 56.f), FAnchors(0.f, 0.f), FVector2D::ZeroVector),
			MakeTuple(TEXT("CountdownWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Countdown.WBP_Countdown_C"), FMargin(-220.f, 28.f, 200.f, 32.f), FAnchors(1.f, 0.f), FVector2D::ZeroVector),
			MakeTuple(TEXT("PlayerStatusWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_PlayerStatus.WBP_PlayerStatus_C"), FMargin(36.f, -58.f, 172.f, 52.f), FAnchors(0.f, 1.f), FVector2D(0.f, 1.f)),
			MakeTuple(TEXT("WeaponStatusWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_WeaponStatus.WBP_WeaponStatus_C"), FMargin(-250.f, -76.f, 230.f, 76.f), FAnchors(1.f, 1.f), FVector2D(0.f, 1.f)),
			MakeTuple(TEXT("CrosshairWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Crosshair.WBP_Crosshair_C"), FMargin(-20.f, -16.f, 40.f, 32.f), FAnchors(0.5f, 0.5f), FVector2D::ZeroVector),
			MakeTuple(TEXT("InteractionWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_InteractionPrompt.WBP_InteractionPrompt_C"), FMargin(-220.f, 64.f, 440.f, 28.f), FAnchors(0.5f, 0.5f), FVector2D::ZeroVector),
			MakeTuple(TEXT("DamageIndicatorWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_DamageIndicator.WBP_DamageIndicator_C"), FMargin(-190.f, 110.f, 380.f, 28.f), FAnchors(0.5f, 0.f), FVector2D::ZeroVector),
			MakeTuple(TEXT("DialogueWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_Dialogue.WBP_Dialogue_C"), FMargin(-360.f, -128.f, 720.f, 64.f), FAnchors(0.5f, 1.f), FVector2D(0.f, 1.f)),
			MakeTuple(TEXT("ChapterTitleWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_ChapterTitle.WBP_ChapterTitle_C"), FMargin(-320.f, -56.f, 640.f, 112.f), FAnchors(0.5f, 0.5f), FVector2D::ZeroVector),
			MakeTuple(TEXT("ManticoreWidget"), TEXT("/Game/Ashes/UI/HUD/WBP_ManticoreHUD.WBP_ManticoreHUD_C"), FMargin(36.f, -154.f, 300.f, 42.f), FAnchors(0.f, 1.f), FVector2D(0.f, 1.f)),
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

		// Update in place when the asset exists: DeleteAsset followed by CreateAsset at
		// the same path returns null in the same editor session (stale package), which
		// destroyed the whole family once. Load-or-create is the only safe idempotent path.
		UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *(EmitterAssetPath + TEXT(".NE_") + Effect));
		if (!Emitter)
		{
			UNiagaraEmitterFactoryNew* EmitterFactory = NewObject<UNiagaraEmitterFactoryNew>();
			EmitterFactory->bAddDefaultModulesAndRenderersToEmptyEmitter = true;
			Emitter = Cast<UNiagaraEmitter>(FAssetToolsModule::GetModule().Get().CreateAsset(
				TEXT("NE_") + Effect, EmitterPath, UNiagaraEmitter::StaticClass(), EmitterFactory));
		}
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
				// Combustion effects glow warm. MI_FireGlow_Orange retints the emissive
				// master; the cyan base is reserved for Veil technology per the art direction.
				EffectMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_FireGlow_Orange.MI_FireGlow_Orange"));
				if (!EffectMaterial)
				{
					EffectMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph"));
				}
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

			// The factory starter emitter is a generic fountain: every effect inherits its
			// spawn rate, sprite size and velocity unless the module defaults are re-authored.
			// Those module inputs live in the scripts' rapid-iteration parameter stores.
			// Discovery pass: log every parameter name/type once so the per-effect table
			// below can target real names on this engine version.
			const TArray<UNiagaraScript*> EmitterScripts = {
				EmitterData->SpawnScriptProps.Script,
				EmitterData->UpdateScriptProps.Script
			};
			if (Effect == TEXT("EmberDrift"))
			{
				for (UNiagaraScript* Script : EmitterScripts)
				{
					if (!Script)
					{
						continue;
					}
					for (const FNiagaraVariableWithOffset& Variable : Script->RapidIterationParameters.ReadParameterVariables())
					{
						UE_LOG(LogTemp, Display, TEXT("[Phase4.5][VFXParamDiscovery] %s | %s : %s"),
							*Effect, *Variable.GetName().ToString(), *Variable.GetType().GetName());
					}
				}
			}
		}
		UEditorAssetLibrary::SaveAsset(EmitterAssetPath, false);

		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *(SystemAssetPath + TEXT(".NS_") + Effect));
		if (!System)
		{
			UNiagaraSystemFactoryNew* SystemFactory = NewObject<UNiagaraSystemFactoryNew>();
			SystemFactory->EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(Emitter, Emitter->GetExposedVersion().VersionGuid));
			System = Cast<UNiagaraSystem>(FAssetToolsModule::GetModule().Get().CreateAsset(
				TEXT("NS_") + Effect, VFXPath, UNiagaraSystem::StaticClass(), SystemFactory));
		}
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

namespace
{
	// One near-camera effect recipe. Values target the packaged war gloom: small warm
	// licks for fire, pinprick embers, soft slow smoke. Rapid-iteration parameters on
	// the factory fountain modules are matched by name substring because the exact
	// "Constants.<Emitter>.<Module>.<Input>" prefixes vary per engine version.
	struct FAHNearVFXRecipe
	{
		const TCHAR* Name = nullptr;
		const TCHAR* MaterialPath = nullptr;
		bool bLocalSpace = true;
		float Bounds = 200.0f;
		float SpawnRate = 20.0f;
		float LifeMin = 0.5f;
		float LifeMax = 1.0f;
		float SizeMin = 10.0f;
		float SizeMax = 20.0f;
		float VelMin = 60.0f;
		float VelMax = 150.0f;
		float ConeAngle = 20.0f;
		float GravityZ = 0.0f;
		float Drag = 1.0f;
		float ShapeRadius = 30.0f;
		// Per-particle quad roll, degrees. Fountain exposes InitializeParticle's
		// "Sprite Rotation Angle Min/Max" as rapid-iteration parameters, so this
		// costs nothing but the two matcher branches below. Smoke wants it; fire
		// must NOT have it, because rolling a flame sprite rolls the direction its
		// noise pans and the licks stop pointing up.
		float RotationMin = 0.0f;
		float RotationMax = 0.0f;
		FLinearColor Color = FLinearColor::White;
	};

	bool SetRapidIterationFloat(FNiagaraParameterStore& Store, const FNiagaraVariableBase& Var, float Value)
	{
		if (Var.GetType() != FNiagaraTypeDefinition::GetFloatDef())
		{
			return false;
		}
		FNiagaraVariable Param(Var.GetType(), Var.GetName());
		Store.SetParameterData(reinterpret_cast<const uint8*>(&Value), Param, false);
		return true;
	}

	bool SetRapidIterationVec2(FNiagaraParameterStore& Store, const FNiagaraVariableBase& Var, const FVector2f& Value)
	{
		if (Var.GetType() != FNiagaraTypeDefinition::GetVec2Def())
		{
			return false;
		}
		FNiagaraVariable Param(Var.GetType(), Var.GetName());
		Store.SetParameterData(reinterpret_cast<const uint8*>(&Value), Param, false);
		return true;
	}

	bool SetRapidIterationVec3(FNiagaraParameterStore& Store, const FNiagaraVariableBase& Var, const FVector3f& Value)
	{
		if (Var.GetType() != FNiagaraTypeDefinition::GetVec3Def())
		{
			return false;
		}
		FNiagaraVariable Param(Var.GetType(), Var.GetName());
		Store.SetParameterData(reinterpret_cast<const uint8*>(&Value), Param, false);
		return true;
	}

	bool SetRapidIterationColor(FNiagaraParameterStore& Store, const FNiagaraVariableBase& Var, const FLinearColor& Value)
	{
		if (Var.GetType() != FNiagaraTypeDefinition::GetColorDef())
		{
			return false;
		}
		FNiagaraVariable Param(Var.GetType(), Var.GetName());
		Store.SetParameterData(reinterpret_cast<const uint8*>(&Value), Param, false);
		return true;
	}

	// Applies the recipe to every rapid-iteration parameter whose name matches a known
	// module input. Returns the number of parameters written; logs every unmatched
	// parameter once so the recipe table can grow against real engine names.
	int32 ApplyRecipeToScript(UNiagaraScript* Script, const FAHNearVFXRecipe& Recipe)
	{
		if (!Script)
		{
			return 0;
		}
		int32 Applied = 0;
		FNiagaraParameterStore& Store = Script->RapidIterationParameters;
		// Copy the variable list up front: SetParameterData can reallocate the store.
		TArray<FNiagaraVariableBase> Variables;
		for (const FNiagaraVariableWithOffset& Var : Store.ReadParameterVariables())
		{
			Variables.Add(FNiagaraVariableBase(Var.GetType(), Var.GetName()));
		}
		for (const FNiagaraVariableBase& Var : Variables)
		{
			// Match on the module INPUT name only (last dot segment): full paths like
			// "Constants.Emitter.SpawnRate.Spawn Probability" contain the module name
			// "SpawnRate" and would otherwise hijack unrelated inputs.
			FString Input = Var.GetName().ToString();
			int32 LastDot = INDEX_NONE;
			if (Input.FindLastChar(TEXT('.'), LastDot))
			{
				Input = Input.Mid(LastDot + 1);
			}
			Input = Input.ToLower().Replace(TEXT(" "), TEXT("")).Replace(TEXT("/"), TEXT(""));

			bool bSet = false;
			if (Input == TEXT("spawnrate"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.SpawnRate);
			}
			else if (Input == TEXT("lifetimemin"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.LifeMin);
			}
			else if (Input == TEXT("lifetimemax"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.LifeMax);
			}
			else if (Input == TEXT("spritesizemin") || Input == TEXT("uniformspritesizemin"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.SizeMin)
					|| SetRapidIterationVec2(Store, Var, FVector2f(Recipe.SizeMin, Recipe.SizeMin));
			}
			else if (Input == TEXT("spritesizemax") || Input == TEXT("uniformspritesizemax"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.SizeMax)
					|| SetRapidIterationVec2(Store, Var, FVector2f(Recipe.SizeMax, Recipe.SizeMax));
			}
			else if (Input == TEXT("color"))
			{
				bSet = SetRapidIterationColor(Store, Var, Recipe.Color);
			}
			else if (Input == TEXT("velocity"))
			{
				bSet = SetRapidIterationVec3(Store, Var, FVector3f(0.0f, 0.0f, 0.5f * (Recipe.VelMin + Recipe.VelMax)))
					|| SetRapidIterationFloat(Store, Var, 0.5f * (Recipe.VelMin + Recipe.VelMax));
			}
			else if (Input == TEXT("velocityspeedscale") || Input == TEXT("speedscale"))
			{
				bSet = SetRapidIterationFloat(Store, Var, 1.0f);
			}
			else if (Input == TEXT("velocitymin") || Input == TEXT("speedmin"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.VelMin);
			}
			else if (Input == TEXT("velocitymax") || Input == TEXT("speedmax"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.VelMax);
			}
			else if (Input == TEXT("coneangle"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.ConeAngle);
			}
			else if (Input == TEXT("innerconeangle"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.ConeAngle * 0.4f);
			}
			else if (Input == TEXT("gravity"))
			{
				bSet = SetRapidIterationVec3(Store, Var, FVector3f(0.0f, 0.0f, Recipe.GravityZ))
					|| SetRapidIterationFloat(Store, Var, Recipe.GravityZ);
			}
			else if (Input == TEXT("drag"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.Drag);
			}
			else if (Input == TEXT("sphereradius") || Input == TEXT("radius"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.ShapeRadius);
			}
			else if (Input == TEXT("spriterotationanglemin"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.RotationMin);
			}
			else if (Input == TEXT("spriterotationanglemax"))
			{
				bSet = SetRapidIterationFloat(Store, Var, Recipe.RotationMax);
			}
			if (bSet)
			{
				++Applied;
				UE_LOG(LogTemp, Display, TEXT("[Phase4.6][NearVFX] %s set %s"), Recipe.Name, *Var.GetName().ToString());
			}
			else
			{
				UE_LOG(LogTemp, Verbose, TEXT("[Phase4.6][NearVFX] untouched param %s : %s (%s)"),
					Recipe.Name, *Var.GetName().ToString(), *Var.GetType().GetName());
			}
		}
		return Applied;
	}
}

bool UAHPresentationAuthoringLibrary::AuthorErebusNearVFX()
{
	const FString VFXPath = TEXT("/Game/Ashes/VFX");
	const FString EmitterPath = VFXPath / TEXT("Emitters");

	TArray<FAHNearVFXRecipe> Recipes;
	{
		FAHNearVFXRecipe Fire;
		Fire.Name = TEXT("Erebus_FireSmall");
		Fire.MaterialPath = TEXT("/Game/Ashes/Materials/M_AH_FireSprite.M_AH_FireSprite");
		Fire.bLocalSpace = true;
		Fire.Bounds = 280.0f;
		// A flame is a CONTINUOUS body. 70/s at half-second lives left ~35 particles
		// spread over 1.5m, which is why the last pass read as orange popcorn with
		// gaps between every puff. Density is the fix that no material can fake.
		Fire.SpawnRate = 260.0f;
		Fire.LifeMin = 0.32f; Fire.LifeMax = 0.72f;
		// Real fire licks: the 6-14uu sprites of an earlier pass were matchheads,
		// invisible past 3m (visual gate 4.8 - fires must read as fires).
		Fire.SizeMin = 20.0f; Fire.SizeMax = 46.0f;
		Fire.VelMin = 50.0f; Fire.VelMax = 105.0f;
		// A 35-degree cone over a 40uu pool fanned the flame out sideways and read as
		// a pile of burning leaves. Real flame is a column: narrow cone, tight base.
		Fire.ConeAngle = 22.0f;
		Fire.GravityZ = 0.0f;
		Fire.Drag = 1.6f;
		Fire.ShapeRadius = 26.0f;
		// Near-white multiplier: M_AH_FireSprite owns the hot->orange->dying-red
		// ramp now, so a saturated ParticleColor here would tint the ramp twice.
		Fire.Color = FLinearColor(3.4f, 2.8f, 2.2f, 1.0f);
		Recipes.Add(Fire);

		FAHNearVFXRecipe Wreck;
		Wreck.Name = TEXT("Erebus_FireWreck");
		Wreck.MaterialPath = TEXT("/Game/Ashes/Materials/M_AH_FireSprite.M_AH_FireSprite");
		Wreck.bLocalSpace = true;
		Wreck.Bounds = 520.0f;
		Wreck.SpawnRate = 320.0f;
		Wreck.LifeMin = 0.42f; Wreck.LifeMax = 0.95f;
		Wreck.SizeMin = 40.0f; Wreck.SizeMax = 110.0f;
		Wreck.VelMin = 100.0f; Wreck.VelMax = 230.0f;
		Wreck.ConeAngle = 18.0f;
		Wreck.Drag = 1.3f;
		Wreck.ShapeRadius = 55.0f;
		Wreck.Color = FLinearColor(3.5f, 2.9f, 2.3f, 1.0f);
		Recipes.Add(Wreck);

		FAHNearVFXRecipe Embers;
		Embers.Name = TEXT("Erebus_EmbersNear");
		Embers.MaterialPath = TEXT("/Game/Ashes/Materials/M_AH_FireSprite.M_AH_FireSprite");
		Embers.bLocalSpace = false;
		Embers.Bounds = 520.0f;
		Embers.SpawnRate = 14.0f;
		Embers.LifeMin = 0.8f; Embers.LifeMax = 1.8f;
		Embers.SizeMin = 1.4f; Embers.SizeMax = 3.4f;
		// Slower, wider drift: the old 200uu/s jets stacked embers into a sourceless
		// vertical string over every fire in the comparison frame.
		Embers.VelMin = 50.0f; Embers.VelMax = 120.0f;
		Embers.ConeAngle = 45.0f;
		Embers.GravityZ = -20.0f;
		Embers.Drag = 0.6f;
		Embers.ShapeRadius = 60.0f;
		Embers.Color = FLinearColor(2.2f, 1.7f, 1.3f, 1.0f);
		Recipes.Add(Embers);

		FAHNearVFXRecipe Smoke;
		Smoke.Name = TEXT("Erebus_SmokeLocal");
		Smoke.MaterialPath = TEXT("/Game/Ashes/Materials/M_AH_SmokeSoft.M_AH_SmokeSoft");
		Smoke.bLocalSpace = false;
		Smoke.Bounds = 700.0f;
		Smoke.SpawnRate = 34.0f;
		Smoke.LifeMin = 3.5f; Smoke.LifeMax = 7.5f;
		// Sizes are the FINAL size: M_AH_SmokeSoft grows the visible mass inside the
		// quad from 15% to 50% of it over life, so a puff billows out instead of
		// popping in as a full-size disc.
		Smoke.SizeMin = 170.0f; Smoke.SizeMax = 420.0f;
		Smoke.VelMin = 60.0f; Smoke.VelMax = 130.0f;
		Smoke.ConeAngle = 22.0f;
		Smoke.Drag = 0.8f;
		Smoke.ShapeRadius = 45.0f;
		// The smoke master is LIT now, so ParticleColor is an albedo tint, not the
		// final colour - a near-black tint here would kill all light response.
		Smoke.RotationMin = 0.0f; Smoke.RotationMax = 360.0f;
		Smoke.Color = FLinearColor(1.0f, 1.0f, 1.0f, 0.62f);
		Recipes.Add(Smoke);

		FAHNearVFXRecipe Column;
		Column.Name = TEXT("Erebus_SmokeColumn");
		Column.MaterialPath = TEXT("/Game/Ashes/Materials/M_AH_SmokeSoft.M_AH_SmokeSoft");
		Column.bLocalSpace = false;
		// The reference's sky is carried by massive dark plumes; the previous thin
		// wisps disappeared entirely at 60m+ in every packaged capture.
		Column.Bounds = 9000.0f;
		// Rate is deliberately LOW for the size: overlapping translucent 2000uu quads
		// accumulate alpha, and a few hundred of them on one column piled up into a
		// solid egg brighter than the sky. A plume needs depth, not opacity.
		Column.SpawnRate = 18.0f;
		Column.LifeMin = 10.0f; Column.LifeMax = 16.0f;
		Column.SizeMin = 900.0f; Column.SizeMax = 2000.0f;
		Column.VelMin = 240.0f; Column.VelMax = 420.0f;
		Column.ConeAngle = 11.0f;
		Column.Drag = 0.4f;
		Column.ShapeRadius = 300.0f;
		Column.RotationMin = 0.0f; Column.RotationMax = 360.0f;
		Column.Color = FLinearColor(1.0f, 1.0f, 1.0f, 0.62f);
		Recipes.Add(Column);

		FAHNearVFXRecipe Ash;
		Ash.Name = TEXT("Erebus_AshAmbient");
		Ash.MaterialPath = TEXT("/Game/Ashes/Materials/M_AH_SmokeSoft.M_AH_SmokeSoft");
		Ash.bLocalSpace = false;
		Ash.Bounds = 1400.0f;
		Ash.SpawnRate = 26.0f;
		Ash.LifeMin = 4.0f; Ash.LifeMax = 8.0f;
		Ash.SizeMin = 2.2f; Ash.SizeMax = 5.0f;
		Ash.VelMin = 20.0f; Ash.VelMax = 50.0f;
		Ash.ConeAngle = 70.0f;
		Ash.GravityZ = -28.0f;
		Ash.Drag = 0.5f;
		Ash.ShapeRadius = 900.0f;
		Ash.RotationMin = 0.0f; Ash.RotationMax = 360.0f;
		Ash.Color = FLinearColor(0.30f, 0.29f, 0.27f, 0.50f);
		Recipes.Add(Ash);
	}

	bool bSuccess = true;
	for (const FAHNearVFXRecipe& Recipe : Recipes)
	{
		const FString EmitterName = FString(TEXT("NE_")) + Recipe.Name;
		const FString SystemName = FString(TEXT("NS_")) + Recipe.Name;
		const FString EmitterAssetPath = EmitterPath / EmitterName;
		const FString SystemAssetPath = VFXPath / SystemName;

		// Load-or-duplicate: a factory-fresh emitter exposes almost no rapid-iteration
		// parameters (module inputs stay at graph defaults until edited), so it cannot
		// be re-authored programmatically. Epic's UI-built Fountain template exposes
		// the full input set (SpawnRate, Lifetime, Sprite Size, Color, Velocity,
		// Gravity, Drag, ShapeLocation) as rapid-iteration parameters — duplicate it
		// and overwrite those. Delete+recreate at the same path corrupts the family in
		// unattended sessions, so existing assets are updated in place instead.
		UNiagaraEmitter* Emitter = LoadObject<UNiagaraEmitter>(nullptr, *(EmitterAssetPath + TEXT(".") + EmitterName));
		const bool bEmitterExisted = Emitter != nullptr;
		if (!Emitter)
		{
			// LoadObject, not the asset-registry duplicate: unattended commandlets never
			// scan engine-plugin content, so registry-based lookups cannot see /Niagara.
			if (UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr, TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain")))
			{
				Emitter = Cast<UNiagaraEmitter>(FAssetToolsModule::GetModule().Get().DuplicateAsset(EmitterName, EmitterPath, Template));
			}
		}
		if (!Emitter)
		{
			UE_LOG(LogTemp, Error, TEXT("[Phase4.6][NearVFX] failed to author emitter %s"), Recipe.Name);
			bSuccess = false;
			continue;
		}

		Emitter->SetUniqueEmitterName(FString(TEXT("AH_")) + Recipe.Name);
		Emitter->Category = FText::FromString(TEXT("Ashes of Heaven / Phase 4.6"));

		// Author one emitter-data view (space, bounds, renderer material, module params).
		auto AuthorEmitterData = [&Recipe](FVersionedNiagaraEmitterData* EmitterData) -> int32
		{
			if (!EmitterData)
			{
				return 0;
			}
			EmitterData->bLocalSpace = Recipe.bLocalSpace;
			EmitterData->bDeterminism = true;
			EmitterData->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
			EmitterData->FixedBounds = FBox(FVector(-Recipe.Bounds), FVector(Recipe.Bounds));
			EmitterData->AllocationMode = EParticleAllocationMode::ManualEstimate;
			// Fire now runs 260-320 spawns/s against ~0.7s lives; a 128 estimate
			// would force a realloc every frame on the densest emitters.
			EmitterData->PreAllocationCount = 384;

			UMaterialInterface* EffectMaterial = LoadObject<UMaterialInterface>(nullptr, Recipe.MaterialPath);
			for (UNiagaraRendererProperties* Renderer : EmitterData->GetRenderers())
			{
				if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
				{
					if (EffectMaterial)
					{
						Sprite->Material = EffectMaterial;
					}
					Sprite->FacingMode = ENiagaraSpriteFacingMode::FaceCamera;
					Sprite->SortMode = FString(Recipe.Name).Contains(TEXT("Smoke"))
						? ENiagaraSortMode::ViewDepth : ENiagaraSortMode::None;
				}
			}

			int32 Applied = 0;
			Applied += ApplyRecipeToScript(EmitterData->SpawnScriptProps.Script, Recipe);
			Applied += ApplyRecipeToScript(EmitterData->UpdateScriptProps.Script, Recipe);
			// SpawnRate lives in the EMITTER update script, not the particle scripts.
			Applied += ApplyRecipeToScript(EmitterData->EmitterSpawnScriptProps.Script, Recipe);
			Applied += ApplyRecipeToScript(EmitterData->EmitterUpdateScriptProps.Script, Recipe);
			return Applied;
		};

		int32 AppliedParams = AuthorEmitterData(Emitter->GetLatestEmitterData());
		UE_LOG(LogTemp, Display, TEXT("[Phase4.6][NearVFX] %s authored: %d module parameters applied"), Recipe.Name, AppliedParams);
		if (AppliedParams == 0)
		{
			// A recipe that matched nothing would ship factory-default columns again.
			UE_LOG(LogTemp, Error, TEXT("[Phase4.6][NearVFX] %s matched no module parameters; refusing factory defaults"), Recipe.Name);
			bSuccess = false;
		}
		UEditorAssetLibrary::SaveAsset(EmitterAssetPath, false);

		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *(SystemAssetPath + TEXT(".") + SystemName));
		if (!System)
		{
			UNiagaraSystemFactoryNew* SystemFactory = NewObject<UNiagaraSystemFactoryNew>();
			SystemFactory->EmittersToAddToNewSystem.Add(FVersionedNiagaraEmitter(Emitter, Emitter->GetExposedVersion().VersionGuid));
			System = Cast<UNiagaraSystem>(FAssetToolsModule::GetModule().Get().CreateAsset(
				SystemName, VFXPath, UNiagaraSystem::StaticClass(), SystemFactory));
		}
		if (!System)
		{
			UE_LOG(LogTemp, Error, TEXT("[Phase4.6][NearVFX] failed to author system %s"), Recipe.Name);
			bSuccess = false;
			continue;
		}

		// The system holds its own INHERITED COPY of the emitter, baked at creation:
		// editing the standalone NE_* asset afterwards never reaches the packaged
		// runtime (this is why earlier passes still rendered factory columns).
		// Author every emitter handle inside the system directly.
		int32 HandleParams = 0;
		for (int32 HandleIndex = 0; HandleIndex < System->GetNumEmitters(); ++HandleIndex)
		{
			FNiagaraEmitterHandle& Handle = System->GetEmitterHandle(HandleIndex);
			HandleParams += AuthorEmitterData(Handle.GetInstance().GetEmitterData());
		}
		UE_LOG(LogTemp, Display, TEXT("[Phase4.6][NearVFX] %s system handles authored: %d module parameters applied"), Recipe.Name, HandleParams);
		if (HandleParams == 0)
		{
			UE_LOG(LogTemp, Error, TEXT("[Phase4.6][NearVFX] %s system emitter copy matched nothing; factory defaults would ship"), Recipe.Name);
			bSuccess = false;
		}

		System->SetFixedBounds(FBox(FVector(-Recipe.Bounds), FVector(Recipe.Bounds)));
		System->RequestCompile(false);
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

bool UAHPresentationAuthoringLibrary::AuthorErebusNearVFX()
{
	return false;
}

#endif
