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

AAHCombatHUD::AAHCombatHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AAHCombatHUD::DrawBar(const FVector2D& Position, const FVector2D& Size, float Percent, const FLinearColor& Color) const
{
	if (!Canvas || !GEngine || !GEngine->DefaultTexture)
	{
		return;
	}
	UTexture2D* WhiteTexture = GEngine->DefaultTexture.Get();
	FCanvasTileItem Background(Position, WhiteTexture->GetResource(), Size, FLinearColor(0.02f, 0.025f, 0.035f, 0.88f));
	Canvas->DrawItem(Background);
	FCanvasTileItem Fill(Position + FVector2D(2.0f, 2.0f), WhiteTexture->GetResource(), FVector2D(FMath::Max(0.0f, Size.X - 4.0f) * FMath::Clamp(Percent, 0.0f, 1.0f), FMath::Max(0.0f, Size.Y - 4.0f)), Color);
	Canvas->DrawItem(Fill);
}

void AAHCombatHUD::DrawTextAt(const FText& Text, const FVector2D& Position, const FLinearColor& Color, float Scale) const
{
	if (!Canvas || !GEngine)
	{
		return;
	}
	FCanvasTextItem Item(Position, Text, GEngine->GetMediumFont(), Color);
	Item.Scale = FVector2D(Scale, Scale);
	Item.EnableShadow(FLinearColor::Black, FVector2D(2.0f, 2.0f));
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
	UWorld* World = GetWorld();
	UAHChapterSubsystem* Chapter = World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr;
	UAHDialogueSubsystem* Dialogue = World ? World->GetSubsystem<UAHDialogueSubsystem>() : nullptr;
	AAHChapterOneDirector* ChapterDirector = World ? Cast<AAHChapterOneDirector>(UGameplayStatics::GetActorOfClass(World, AAHChapterOneDirector::StaticClass())) : nullptr;
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(GetOwningPawn());
	AAHManticoreVehicle* Vehicle = Cast<AAHManticoreVehicle>(GetOwningPawn());

	if (Player)
	{
		const float Health = Player->GetHealthComponent() ? Player->GetHealthComponent()->GetHealthPercent() : 0.0f;
		const float Armor = Player->GetArmorComponent() ? Player->GetArmorComponent()->GetArmorPercent() : 0.0f;
		DrawTextAt(FText::FromString(TEXT("ARMOR")), FVector2D(38.0f, ViewSize.Y - 112.0f), FLinearColor(0.42f, 0.72f, 0.95f, 1.0f), 0.72f);
		DrawBar(FVector2D(38.0f, ViewSize.Y - 88.0f), FVector2D(250.0f, 16.0f), Armor, FLinearColor(0.22f, 0.62f, 0.95f, 1.0f));
		DrawTextAt(FText::FromString(TEXT("HEALTH")), FVector2D(38.0f, ViewSize.Y - 62.0f), FLinearColor(0.91f, 0.47f, 0.38f, 1.0f), 0.72f);
		DrawBar(FVector2D(38.0f, ViewSize.Y - 38.0f), FVector2D(250.0f, 16.0f), Health, FLinearColor(0.80f, 0.17f, 0.16f, 1.0f));

		if (AAHWeaponBase* Weapon = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetCurrentWeapon() : nullptr)
		{
			const FAHAmmoState& Ammo = Weapon->GetAmmoState();
			DrawTextAt(FText::FromString(FString::Printf(TEXT("%02d / %03d"), Ammo.Magazine, Ammo.Reserve)), FVector2D(ViewSize.X - 250.0f, ViewSize.Y - 78.0f), FLinearColor::White, 1.25f);
			DrawTextAt(Weapon->DisplayName, FVector2D(ViewSize.X - 250.0f, ViewSize.Y - 42.0f), FLinearColor(0.68f, 0.71f, 0.76f, 1.0f), 0.72f);
		}

		const int32 Grenades = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetGrenades() : 0;
		DrawTextAt(FText::FromString(FString::Printf(TEXT("FRAG  %d"), Grenades)), FVector2D(ViewSize.X - 250.0f, ViewSize.Y - 116.0f), FLinearColor(0.92f, 0.73f, 0.32f, 1.0f), 0.72f);

		const FLinearColor CrosshairColor = (World->GetTimeSeconds() < HitMarkerUntil) ? (bHitWasHeadshot ? FLinearColor(1.0f, 0.55f, 0.1f, 1.0f) : FLinearColor::White) : FLinearColor(0.85f, 0.86f, 0.88f, 0.85f);
		FCanvasLineItem CrosshairLeft(Center + FVector2D(-13.0f, 0.0f), Center + FVector2D(-4.0f, 0.0f));
		CrosshairLeft.SetColor(CrosshairColor);
		Canvas->DrawItem(CrosshairLeft);
		FCanvasLineItem CrosshairRight(Center + FVector2D(4.0f, 0.0f), Center + FVector2D(13.0f, 0.0f));
		CrosshairRight.SetColor(CrosshairColor);
		Canvas->DrawItem(CrosshairRight);
		FCanvasLineItem CrosshairUp(Center + FVector2D(0.0f, -13.0f), Center + FVector2D(0.0f, -4.0f));
		CrosshairUp.SetColor(CrosshairColor);
		Canvas->DrawItem(CrosshairUp);
		FCanvasLineItem CrosshairDown(Center + FVector2D(0.0f, 4.0f), Center + FVector2D(0.0f, 13.0f));
		CrosshairDown.SetColor(CrosshairColor);
		Canvas->DrawItem(CrosshairDown);
		if (World->GetTimeSeconds() < HitMarkerUntil)
		{
			DrawTextAt(bHitWasHeadshot ? FText::FromString(TEXT("HEADSHOT")) : FText::FromString(TEXT("HIT")), Center + FVector2D(-45.0f, 30.0f), CrosshairColor, 0.62f);
		}

		if (Player->GetInteractionComponent() && !Player->GetInteractionComponent()->GetPrompt().IsEmpty())
		{
			DrawTextAt(Player->GetInteractionComponent()->GetPrompt(), Center + FVector2D(-95.0f, 90.0f), FLinearColor(0.92f, 0.73f, 0.32f, 1.0f), 0.72f);
		}
		if (World->GetTimeSeconds() < DamageFeedbackUntil)
		{
			DrawTextAt(bArmorBreak ? FText::FromString(TEXT("ARMOR BREAK")) : FText::FromString(TEXT("DAMAGE")), Center + FVector2D(-55.0f, -130.0f), bArmorBreak ? FLinearColor(1.0f, 0.68f, 0.2f, 1.0f) : FLinearColor(0.9f, 0.18f, 0.15f, 1.0f), 0.78f);
			const float AngleRadians = FMath::DegreesToRadians(DamageDirectionAngle);
			const FVector2D Direction(FMath::Sin(AngleRadians), -FMath::Cos(AngleRadians));
			FCanvasLineItem DamageDirection(Center + Direction * 105.0f, Center + Direction * 145.0f);
			DamageDirection.SetColor(FLinearColor(0.95f, 0.12f, 0.10f, 0.9f));
			DamageDirection.LineThickness = 4.0f;
			Canvas->DrawItem(DamageDirection);
		}
		if (Health < 0.25f)
		{
			FCanvasTileItem LowHealthOverlay(FVector2D::ZeroVector, GEngine->DefaultTexture->GetResource(), ViewSize, FLinearColor(0.6f, 0.0f, 0.0f, 0.10f));
			Canvas->DrawItem(LowHealthOverlay);
		}
	}
	else if (Vehicle)
	{
		DrawTextAt(FText::FromString(TEXT("MANTICORE")), FVector2D(38.0f, ViewSize.Y - 112.0f), FLinearColor(0.78f, 0.83f, 0.88f, 1.0f), 0.82f);
		DrawBar(FVector2D(38.0f, ViewSize.Y - 84.0f), FVector2D(250.0f, 16.0f), Vehicle->GetHealthPercent(), FLinearColor(0.75f, 0.42f, 0.16f, 1.0f));
		DrawTextAt(FText::FromString(FString::Printf(TEXT("SPEED  %03d"), FMath::RoundToInt(FMath::Abs(Vehicle->GetSpeed())))), FVector2D(38.0f, ViewSize.Y - 48.0f), FLinearColor(0.75f, 0.78f, 0.82f, 1.0f), 0.72f);
		FCanvasLineItem VehicleCrosshair(Center + FVector2D(-17.0f, 0.0f), Center + FVector2D(17.0f, 0.0f));
		VehicleCrosshair.SetColor(FLinearColor(1.0f, 0.72f, 0.25f, 0.9f));
		Canvas->DrawItem(VehicleCrosshair);
	}

	DrawTextAt(CurrentObjective, FVector2D(38.0f, 42.0f), FLinearColor(0.86f, 0.88f, 0.91f, 1.0f), 0.88f);
	DrawTextAt(FText::FromString(FString::Printf(TEXT("%02d / %02d"), FMath::Min(ObjectiveIndex + 1, ObjectiveCount), ObjectiveCount)), FVector2D(38.0f, 74.0f), FLinearColor(0.48f, 0.54f, 0.61f, 1.0f), 0.65f);

	if (Chapter && Chapter->IsCountdownActive())
	{
		const int32 Remaining = FMath::Max(0, FMath::CeilToInt(Chapter->GetCountdownSeconds()));
		DrawTextAt(FText::FromString(FString::Printf(TEXT("FAILSAFE  %02d:%02d"), Remaining / 60, Remaining % 60)), FVector2D(ViewSize.X - 290.0f, 42.0f), Remaining <= 10 ? FLinearColor(1.0f, 0.22f, 0.14f, 1.0f) : FLinearColor(0.92f, 0.73f, 0.32f, 1.0f), 0.86f);
	}

	if (Player && Player->GetInteractionComponent())
	{
		if (AAHChapterTerminal* Terminal = Cast<AAHChapterTerminal>(Player->GetInteractionComponent()->GetCurrentTarget()))
		{
			if (Terminal->IsInspected() && !Terminal->IsConfirmed())
			{
				DrawTextAt(Terminal->CasualtyText, Center + FVector2D(-230.0f, -150.0f), FLinearColor(0.92f, 0.92f, 0.96f, 1.0f), 0.82f);
			}
		}
	}

	if (ChapterDirector && ChapterDirector->GetCurrentStage() == EAHChapterStage::ErebusDestruction)
	{
		const float Fade = ChapterDirector->GetDestructionFadeAlpha();
		FCanvasTileItem DestructionOverlay(FVector2D::ZeroVector, GEngine->DefaultTexture->GetResource(), ViewSize, FLinearColor(1.0f, 0.94f, 0.82f, Fade));
		Canvas->DrawItem(DestructionOverlay);
	}
	if (ChapterDirector && ChapterDirector->IsOpeningBlack())
	{
		FCanvasTileItem OpeningOverlay(FVector2D::ZeroVector, GEngine->DefaultTexture->GetResource(), ViewSize, FLinearColor::Black);
		Canvas->DrawItem(OpeningOverlay);
	}
	if (Dialogue && Dialogue->HasActiveDialogue())
	{
		FCanvasTileItem DialoguePanel(FVector2D(ViewSize.X * 0.12f, ViewSize.Y - 172.0f), GEngine->DefaultTexture->GetResource(), FVector2D(ViewSize.X * 0.76f, 112.0f), FLinearColor(0.01f, 0.015f, 0.025f, 0.90f));
		Canvas->DrawItem(DialoguePanel);
		DrawTextAt(FText::FromName(Dialogue->GetCurrentSpeaker()), FVector2D(ViewSize.X * 0.15f, ViewSize.Y - 150.0f), FLinearColor(0.92f, 0.73f, 0.32f, 1.0f), 0.68f);
		DrawTextAt(Dialogue->GetCurrentSubtitle(), FVector2D(ViewSize.X * 0.15f, ViewSize.Y - 118.0f), FLinearColor::White, 0.92f);
	}
	if ((ChapterDirector && ChapterDirector->IsTitleReveal()) || (bMissionComplete && ChapterDirector))
	{
		FCanvasTileItem Scrim(FVector2D::ZeroVector, GEngine->DefaultTexture->GetResource(), ViewSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.84f));
		Canvas->DrawItem(Scrim);
		DrawTextAt(FText::FromString(TEXT("ASHES OF HEAVEN")), FVector2D(ViewSize.X * 0.5f - 245.0f, ViewSize.Y * 0.5f - 45.0f), FLinearColor(0.92f, 0.78f, 0.42f, 1.0f), 1.45f);
		DrawTextAt(FText::FromString(TEXT("CHAPTER ONE COMPLETE")), FVector2D(ViewSize.X * 0.5f - 150.0f, ViewSize.Y * 0.5f + 30.0f), FLinearColor::White, 0.78f);
	}
	else if (bMissionComplete)
	{
		FCanvasTileItem Scrim(FVector2D::ZeroVector, GEngine->DefaultTexture->GetResource(), ViewSize, FLinearColor(0.0f, 0.0f, 0.0f, 0.72f));
		Canvas->DrawItem(Scrim);
		DrawTextAt(FText::FromString(TEXT("PROVING GROUND COMPLETE")), FVector2D(ViewSize.X * 0.5f - 215.0f, ViewSize.Y * 0.5f - 24.0f), FLinearColor(0.92f, 0.78f, 0.42f, 1.0f), 1.3f);
		DrawTextAt(FText::FromString(TEXT("COMBAT PROVING GROUND — EREBUS")), FVector2D(ViewSize.X * 0.5f - 175.0f, ViewSize.Y * 0.5f + 26.0f), FLinearColor::White, 0.72f);
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
	CurrentObjective = NewObjective;
	ObjectiveIndex = NewIndex;
	ObjectiveCount = Count;
}

void AAHCombatHUD::ShowMissionComplete()
{
	bMissionComplete = true;
}
