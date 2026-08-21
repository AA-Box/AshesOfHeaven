#include "Gameplay/UI/AHCombatHUD.h"

#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Gameplay/Level/AHChapterOneDirector.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FLinearColor Ink(0.005f, 0.007f, 0.009f, 0.92f);
	const FLinearColor Steel(0.24f, 0.29f, 0.31f, 0.92f);
	const FLinearColor Bone(0.84f, 0.85f, 0.81f, 1.0f);
	const FLinearColor Muted(0.48f, 0.53f, 0.53f, 1.0f);
	const FLinearColor Amber(0.94f, 0.62f, 0.22f, 1.0f);
	const FLinearColor Red(0.82f, 0.14f, 0.10f, 1.0f);
	const FLinearColor Cyan(0.42f, 0.68f, 0.71f, 1.0f);
}

AAHCombatHUD::AAHCombatHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAHCombatHUD::DrawPanel(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Color) const
{
	if (!Canvas || !GEngine || !GEngine->DefaultTexture)
	{
		return;
	}

	FCanvasTileItem Panel(Position, GEngine->DefaultTexture->GetResource(), Size, Color);
	Canvas->DrawItem(Panel);
}

void AAHCombatHUD::DrawLine(const FVector2D& Start, const FVector2D& End, const FLinearColor& Color, float Thickness) const
{
	if (!Canvas)
	{
		return;
	}

	FCanvasLineItem Line(Start, End);
	Line.SetColor(Color);
	Line.LineThickness = Thickness;
	Canvas->DrawItem(Line);
}

void AAHCombatHUD::DrawTacticalFrame(const FVector2D& Position, const FVector2D& Size, const FLinearColor& Accent, float FillAlpha) const
{
	DrawPanel(Position, Size, FLinearColor(Ink.R, Ink.G, Ink.B, FillAlpha));
	const float Corner = FMath::Min(16.0f, FMath::Min(Size.X, Size.Y) * 0.24f);
	const FLinearColor Hairline(Accent.R, Accent.G, Accent.B, 0.72f);
	DrawLine(Position, Position + FVector2D(Corner, 0.0f), Hairline, 1.2f);
	DrawLine(Position, Position + FVector2D(0.0f, Corner), Hairline, 1.2f);
	DrawLine(Position + FVector2D(Size.X, 0.0f), Position + FVector2D(Size.X - Corner, 0.0f), Hairline, 1.2f);
	DrawLine(Position + FVector2D(Size.X, 0.0f), Position + FVector2D(Size.X, Corner), Hairline, 1.2f);
	DrawLine(Position + FVector2D(0.0f, Size.Y), Position + FVector2D(Corner, Size.Y), Hairline, 1.2f);
	DrawLine(Position + FVector2D(0.0f, Size.Y), Position + FVector2D(0.0f, Size.Y - Corner), Hairline, 1.2f);
	DrawLine(Position + Size, Position + FVector2D(Size.X - Corner, Size.Y), Hairline, 1.2f);
	DrawLine(Position + Size, Position + FVector2D(Size.X, Size.Y - Corner), Hairline, 1.2f);
}

void AAHCombatHUD::DrawBar(const FVector2D& Position, const FVector2D& Size, float Percent, const FLinearColor& Color) const
{
	if (!Canvas || !GEngine || !GEngine->DefaultTexture)
	{
		return;
	}

	UTexture2D* WhiteTexture = GEngine->DefaultTexture.Get();
	FCanvasTileItem Background(Position, WhiteTexture->GetResource(), Size, FLinearColor(0.02f, 0.025f, 0.025f, 0.88f));
	Canvas->DrawItem(Background);
	const float FillWidth = FMath::Max(0.0f, Size.X - 4.0f) * FMath::Clamp(Percent, 0.0f, 1.0f);
	if (FillWidth > 0.0f)
	{
		FCanvasTileItem Fill(Position + FVector2D(2.0f, 2.0f), WhiteTexture->GetResource(), FVector2D(FillWidth, FMath::Max(1.0f, Size.Y - 4.0f)), Color);
		Canvas->DrawItem(Fill);
	}
	DrawLine(Position + FVector2D(0.0f, Size.Y + 1.0f), Position + FVector2D(Size.X, Size.Y + 1.0f), FLinearColor(Color.R, Color.G, Color.B, 0.38f), 1.0f);
}

void AAHCombatHUD::DrawTextAt(const FText& Text, const FVector2D& Position, const FLinearColor& Color, float Scale) const
{
	if (!Canvas || !GEngine)
	{
		return;
	}

	FCanvasTextItem Item(Position, Text, GEngine->GetMediumFont(), Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor(0.0f, 0.0f, 0.0f, 0.82f), FVector2D(1.0f, 1.0f));
	Canvas->DrawItem(Item);
}

void AAHCombatHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine || !GEngine->DefaultTexture)
	{
		return;
	}

	const FVector2D ViewSize(Canvas->SizeX, Canvas->SizeY);
	const FVector2D Center = ViewSize * 0.5f;
	const float UIScale = FMath::Clamp(FMath::Min(ViewSize.X / 1280.0f, ViewSize.Y / 720.0f), 0.85f, 1.35f);
	const float Margin = 34.0f * UIScale;
	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	UWorld* World = GetWorld();
	UAHChapterSubsystem* Chapter = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr;
	UAHDialogueSubsystem* Dialogue = World ? World->GetSubsystem<UAHDialogueSubsystem>() : nullptr;
	AAHChapterOneDirector* ChapterDirector = World ? Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(World, AAHChapterOneDirector::StaticClass())) : nullptr;
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetOwningPawn());
	AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(GetOwningPawn());

	const float ObjectiveWidth = FMath::Min(570.0f * UIScale, ViewSize.X - Margin * 2.0f - 300.0f * UIScale);
	const FVector2D ObjectivePosition(Margin, 30.0f * UIScale);
	const FVector2D ObjectiveSize(FMath::Max(360.0f * UIScale, ObjectiveWidth), 78.0f * UIScale);
	const float ObjectivePulse = ObjectivePulseUntil > Now ? FMath::Clamp((ObjectivePulseUntil - Now) / 2.2f, 0.0f, 1.0f) : 0.0f;
	DrawTacticalFrame(ObjectivePosition, ObjectiveSize, ObjectivePulse > 0.0f ? Amber : Steel, 0.62f + ObjectivePulse * 0.12f);
	DrawTextAt(FText::FromString(FString::Printf(TEXT("MISSION // OBJECTIVE %02d OF %02d"), FMath::Min(ObjectiveIndex + 1, ObjectiveCount), ObjectiveCount)), ObjectivePosition + FVector2D(15.0f * UIScale, 10.0f * UIScale), Muted, 0.68f * UIScale);
	DrawLine(ObjectivePosition + FVector2D(15.0f * UIScale, 31.0f * UIScale), ObjectivePosition + FVector2D(148.0f * UIScale, 31.0f * UIScale), Amber, 2.0f);
	DrawTextAt(CurrentObjective.IsEmpty() ? FText::FromString(TEXT("AWAITING ORDERS")) : CurrentObjective, ObjectivePosition + FVector2D(15.0f * UIScale, 41.0f * UIScale), Bone, 0.93f * UIScale);

	if (Chapter && Chapter->IsCountdownActive())
	{
		const int32 Remaining = FMath::Max(0, FMath::CeilToInt(Chapter->GetCountdownSeconds()));
		const FVector2D CountdownSize(242.0f * UIScale, 64.0f * UIScale);
		const FVector2D CountdownPosition(ViewSize.X - Margin - CountdownSize.X, 30.0f * UIScale);
		const FLinearColor CountdownColor = Remaining <= 10 ? Red : Amber;
		DrawTacticalFrame(CountdownPosition, CountdownSize, CountdownColor, Remaining <= 10 ? 0.72f : 0.58f);
		DrawTextAt(FText::FromString(TEXT("FAILSAFE // EVACUATION")), CountdownPosition + FVector2D(14.0f * UIScale, 9.0f * UIScale), Muted, 0.62f * UIScale);
		DrawTextAt(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Remaining / 60, Remaining % 60)), CountdownPosition + FVector2D(14.0f * UIScale, 28.0f * UIScale), CountdownColor, 1.05f * UIScale);
		DrawLine(CountdownPosition + FVector2D(110.0f * UIScale, 46.0f * UIScale), CountdownPosition + FVector2D(CountdownSize.X - 14.0f * UIScale, 46.0f * UIScale), CountdownColor, 2.0f);
	}

	if (Player)
	{
		const float Health = Player->GetHealthComponent() ? Player->GetHealthComponent()->GetHealthPercent() : 0.0f;
		const float Armor = Player->GetArmorComponent() ? Player->GetArmorComponent()->GetArmorPercent() : 0.0f;
		const FVector2D StatusSize(342.0f * UIScale, 132.0f * UIScale);
		const FVector2D StatusPosition(Margin, ViewSize.Y - Margin - StatusSize.Y);
		DrawTacticalFrame(StatusPosition, StatusSize, Armor > 0.0f ? Cyan : Red, 0.64f);
		DrawTextAt(FText::FromString(TEXT("LUCIAN VARR // UNS VIGIL")), StatusPosition + FVector2D(15.0f * UIScale, 11.0f * UIScale), Bone, 0.72f * UIScale);
		DrawTextAt(FText::FromString(TEXT("ARMOR")), StatusPosition + FVector2D(15.0f * UIScale, 39.0f * UIScale), Cyan, 0.62f * UIScale);
		DrawTextAt(FText::FromString(FString::Printf(TEXT("%03d"), FMath::RoundToInt(Armor * 100.0f))), StatusPosition + FVector2D(295.0f * UIScale, 37.0f * UIScale), Cyan, 0.65f * UIScale);
		DrawBar(StatusPosition + FVector2D(15.0f * UIScale, 56.0f * UIScale), FVector2D(312.0f * UIScale, 8.0f * UIScale), Armor, Cyan);
		DrawTextAt(FText::FromString(TEXT("VITALS")), StatusPosition + FVector2D(15.0f * UIScale, 78.0f * UIScale), Health < 0.25f ? Red : Amber, 0.62f * UIScale);
		DrawTextAt(FText::FromString(FString::Printf(TEXT("%03d"), FMath::RoundToInt(Health * 100.0f))), StatusPosition + FVector2D(295.0f * UIScale, 76.0f * UIScale), Health < 0.25f ? Red : Amber, 0.65f * UIScale);
		DrawBar(StatusPosition + FVector2D(15.0f * UIScale, 95.0f * UIScale), FVector2D(312.0f * UIScale, 8.0f * UIScale), Health, Health < 0.25f ? Red : Amber);

		if (AAHWeaponBase* Weapon = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetCurrentWeapon() : nullptr)
		{
			const FAHAmmoState& Ammo = Weapon->GetAmmoState();
			const FVector2D WeaponSize(338.0f * UIScale, 132.0f * UIScale);
			const FVector2D WeaponPosition(ViewSize.X - Margin - WeaponSize.X, ViewSize.Y - Margin - WeaponSize.Y);
			DrawTacticalFrame(WeaponPosition, WeaponSize, Ammo.Magazine == 0 ? Red : Amber, 0.64f);
			DrawLine(WeaponPosition + FVector2D(16.0f * UIScale, 22.0f * UIScale), WeaponPosition + FVector2D(16.0f * UIScale, 109.0f * UIScale), Amber, 2.0f);
			DrawTextAt(Weapon->DisplayName, WeaponPosition + FVector2D(29.0f * UIScale, 10.0f * UIScale), Muted, 0.68f * UIScale);
			DrawTextAt(FText::FromString(FString::Printf(TEXT("%02d"), Ammo.Magazine)), WeaponPosition + FVector2D(29.0f * UIScale, 31.0f * UIScale), Bone, 1.72f * UIScale);
			DrawTextAt(FText::FromString(FString::Printf(TEXT("/ %03d"), Ammo.Reserve)), WeaponPosition + FVector2D(159.0f * UIScale, 50.0f * UIScale), Muted, 0.78f * UIScale);
			DrawTextAt(FText::FromString(TEXT("MAG // RESERVE")), WeaponPosition + FVector2D(29.0f * UIScale, 92.0f * UIScale), Muted, 0.58f * UIScale);
			const int32 Grenades = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetGrenades() : 0;
			DrawTextAt(FText::FromString(FString::Printf(TEXT("FRAG  %02d"), Grenades)), WeaponPosition + FVector2D(228.0f * UIScale, 91.0f * UIScale), Amber, 0.66f * UIScale);
		}

		const FLinearColor CrosshairColor = Now < HitMarkerUntil ? (bHitWasHeadshot ? Amber : Bone) : FLinearColor(Bone.R, Bone.G, Bone.B, 0.86f);
		const float Gap = 6.0f * UIScale;
		const float Arm = 10.0f * UIScale;
		DrawLine(Center + FVector2D(-Gap - Arm, 0.0f), Center + FVector2D(-Gap, 0.0f), CrosshairColor, 1.2f);
		DrawLine(Center + FVector2D(Gap, 0.0f), Center + FVector2D(Gap + Arm, 0.0f), CrosshairColor, 1.2f);
		DrawLine(Center + FVector2D(0.0f, -Gap - Arm), Center + FVector2D(0.0f, -Gap), CrosshairColor, 1.2f);
		DrawLine(Center + FVector2D(0.0f, Gap), Center + FVector2D(0.0f, Gap + Arm), CrosshairColor, 1.2f);
		DrawPanel(Center - FVector2D(1.5f * UIScale, 1.5f * UIScale), FVector2D(3.0f * UIScale, 3.0f * UIScale), CrosshairColor);

		if (Now < HitMarkerUntil)
		{
			DrawTextAt(bHitWasHeadshot ? FText::FromString(TEXT("HEADSHOT")) : FText::FromString(TEXT("HIT")), Center + FVector2D(-34.0f * UIScale, 26.0f * UIScale), CrosshairColor, 0.62f * UIScale);
		}
		if (Player->GetInteractionComponent() && !Player->GetInteractionComponent()->GetPrompt().IsEmpty())
		{
			const FVector2D PromptPosition(Center + FVector2D(-145.0f * UIScale, 78.0f * UIScale));
			DrawTacticalFrame(PromptPosition, FVector2D(290.0f * UIScale, 34.0f * UIScale), Amber, 0.48f);
			DrawTextAt(Player->GetInteractionComponent()->GetPrompt(), PromptPosition + FVector2D(14.0f * UIScale, 9.0f * UIScale), Bone, 0.70f * UIScale);
		}
		if (Now < DamageFeedbackUntil)
		{
			const FLinearColor DamageColor = bArmorBreak ? Amber : Red;
			DrawTextAt(bArmorBreak ? FText::FromString(TEXT("ARMOR BREACH")) : FText::FromString(TEXT("DAMAGE")), Center + FVector2D(-52.0f * UIScale, -128.0f * UIScale), DamageColor, 0.70f * UIScale);
			const float AngleRadians = FMath::DegreesToRadians(DamageDirectionAngle);
			const FVector2D Direction(FMath::Sin(AngleRadians), -FMath::Cos(AngleRadians));
			DrawLine(Center + Direction * (92.0f * UIScale), Center + Direction * (132.0f * UIScale), DamageColor, 3.0f * UIScale);
		}
		if (Health < 0.25f)
		{
			const FLinearColor Warning(Red.R, Red.G, Red.B, 0.60f);
			DrawLine(FVector2D::ZeroVector, FVector2D(ViewSize.X, 0.0f), Warning, 3.0f);
			DrawLine(FVector2D(0.0f, ViewSize.Y), ViewSize, Warning, 3.0f);
			DrawLine(FVector2D::ZeroVector, FVector2D(0.0f, ViewSize.Y), Warning, 3.0f);
			DrawLine(FVector2D(ViewSize.X, 0.0f), ViewSize, Warning, 3.0f);
		}
	}
	else if (Vehicle)
	{
		const FVector2D VehicleSize(342.0f * UIScale, 112.0f * UIScale);
		const FVector2D VehiclePosition(Margin, ViewSize.Y - Margin - VehicleSize.Y);
		DrawTacticalFrame(VehiclePosition, VehicleSize, Amber, 0.66f);
		DrawTextAt(FText::FromString(TEXT("MANTICORE // HEAVY FRAME")), VehiclePosition + FVector2D(15.0f * UIScale, 11.0f * UIScale), Bone, 0.72f * UIScale);
		DrawTextAt(FText::FromString(FString::Printf(TEXT("HULL %03d"), FMath::RoundToInt(Vehicle->GetHealthPercent() * 100.0f))), VehiclePosition + FVector2D(15.0f * UIScale, 40.0f * UIScale), Amber, 0.64f * UIScale);
		DrawBar(VehiclePosition + FVector2D(15.0f * UIScale, 58.0f * UIScale), FVector2D(312.0f * UIScale, 8.0f * UIScale), Vehicle->GetHealthPercent(), Amber);
		DrawTextAt(FText::FromString(FString::Printf(TEXT("SPEED  %03d"), FMath::RoundToInt(FMath::Abs(Vehicle->GetSpeed())))), VehiclePosition + FVector2D(15.0f * UIScale, 78.0f * UIScale), Muted, 0.66f * UIScale);
		DrawLine(Center + FVector2D(-18.0f * UIScale, 0.0f), Center + FVector2D(18.0f * UIScale, 0.0f), Amber, 1.2f);
		DrawLine(Center + FVector2D(0.0f, -18.0f * UIScale), Center + FVector2D(0.0f, 18.0f * UIScale), Amber, 1.2f);
	}

	if (Player && Player->GetInteractionComponent())
	{
		if (AAHChapterTerminal* Terminal = Cast<AAHChapterTerminal>(Player->GetInteractionComponent()->GetCurrentTarget()))
		{
			if (Terminal->IsInspected() && !Terminal->IsConfirmed())
			{
				const FVector2D IntelPosition(Center + FVector2D(-260.0f * UIScale, -178.0f * UIScale));
				DrawTacticalFrame(IntelPosition, FVector2D(520.0f * UIScale, 42.0f * UIScale), Red, 0.54f);
				DrawTextAt(Terminal->CasualtyText, IntelPosition + FVector2D(14.0f * UIScale, 11.0f * UIScale), Bone, 0.70f * UIScale);
			}
		}
	}

	if (ChapterDirector && ChapterDirector->GetCurrentStage() == EAHChapterStage::ErebusDestruction)
	{
		const float Fade = ChapterDirector->GetDestructionFadeAlpha();
		DrawPanel(FVector2D::ZeroVector, ViewSize, FLinearColor(1.0f, 0.94f, 0.82f, Fade));
	}
	if (ChapterDirector && ChapterDirector->IsOpeningBlack())
	{
		DrawPanel(FVector2D::ZeroVector, ViewSize, FLinearColor::Black);
	}

	if (Dialogue && Dialogue->HasActiveDialogue())
	{
		const float DialogueHeight = 146.0f * UIScale;
		const FVector2D DialoguePosition(0.0f, ViewSize.Y - DialogueHeight - 20.0f * UIScale);
		DrawPanel(DialoguePosition, FVector2D(ViewSize.X, DialogueHeight + 20.0f * UIScale), FLinearColor(0.004f, 0.006f, 0.008f, 0.80f));
		DrawLine(FVector2D(70.0f * UIScale, DialoguePosition.Y), FVector2D(ViewSize.X - 70.0f * UIScale, DialoguePosition.Y), Amber, 1.5f);
		DrawTextAt(FText::FromString(FString::Printf(TEXT("// %s"), *Dialogue->GetCurrentSpeaker().ToString())), FVector2D(70.0f * UIScale, DialoguePosition.Y + 26.0f * UIScale), Amber, 0.72f * UIScale);
		DrawTextAt(Dialogue->GetCurrentSubtitle(), FVector2D(70.0f * UIScale, DialoguePosition.Y + 64.0f * UIScale), Bone, 1.02f * UIScale);
	}

	if ((ChapterDirector && ChapterDirector->IsTitleReveal()) || (bMissionComplete && ChapterDirector))
	{
		DrawPanel(FVector2D::ZeroVector, ViewSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.86f));
		const FVector2D TitleCenter(ViewSize.X * 0.5f, ViewSize.Y * 0.5f);
		DrawLine(TitleCenter + FVector2D(-260.0f * UIScale, -58.0f * UIScale), TitleCenter + FVector2D(260.0f * UIScale, -58.0f * UIScale), Amber, 1.5f);
		DrawTextAt(FText::FromString(TEXT("ASHES OF HEAVEN")), TitleCenter + FVector2D(-178.0f * UIScale, -38.0f * UIScale), Amber, 1.42f * UIScale);
		DrawTextAt(FText::FromString(TEXT("CHAPTER ONE // COMPLETE")), TitleCenter + FVector2D(-128.0f * UIScale, 22.0f * UIScale), Bone, 0.78f * UIScale);
		DrawLine(TitleCenter + FVector2D(-260.0f * UIScale, 58.0f * UIScale), TitleCenter + FVector2D(260.0f * UIScale, 58.0f * UIScale), Amber, 1.5f);
	}
	else if (bMissionComplete)
	{
		DrawPanel(FVector2D::ZeroVector, ViewSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.74f));
		DrawTextAt(FText::FromString(TEXT("PROVING GROUND // COMPLETE")), Center + FVector2D(-180.0f * UIScale, -24.0f * UIScale), Amber, 1.14f * UIScale);
		DrawTextAt(FText::FromString(TEXT("EREBUS COMBAT SLICE")), Center + FVector2D(-95.0f * UIScale, 26.0f * UIScale), Bone, 0.72f * UIScale);
	}
}

void AAHCombatHUD::ShowHitMarker(bool bHeadshot)
{
	bHitWasHeadshot = bHeadshot;
	HitMarkerUntil = GetWorld()->GetTimeSeconds() + 0.18f;
}

void AAHCombatHUD::ShowDamageFeedback(bool bArmorBreakIn, float DirectionAngle)
{
	bArmorBreak = bArmorBreakIn;
	DamageDirectionAngle = DirectionAngle;
	DamageFeedbackUntil = GetWorld()->GetTimeSeconds() + 0.65f;
}

void AAHCombatHUD::SetObjective(const FText& NewObjective, int32 NewIndex, int32 Count)
{
	if (!CurrentObjective.EqualTo(NewObjective) || ObjectiveIndex != NewIndex)
	{
		ObjectivePulseUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.2f : 2.2f;
	}
	CurrentObjective = NewObjective;
	ObjectiveIndex = NewIndex;
	ObjectiveCount = Count;
}

void AAHCombatHUD::ShowMissionComplete()
{
	bMissionComplete = true;
}
