#include "Gameplay/UI/AHCombatHUD.h"

#include "Gameplay/UI/AHHUDRootWidget.h"
#include "UObject/SoftObjectPath.h"

AAHCombatHUD::AAHCombatHUD()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AAHCombatHUD::BeginPlay()
{
	Super::BeginPlay();
	const FSoftClassPath RootClassPath(TEXT("/Game/Ashes/UI/HUD/WBP_HUD_Root.WBP_HUD_Root_C"));
	UClass* RootClass = RootClassPath.TryLoadClass<UAHHUDRootWidget>();
	if (!RootClass)
	{
		UE_LOG(LogTemp, Error, TEXT("[Phase4.2][UI] WBP_HUD_Root missing; production HUD was not created"));
		return;
	}
	RootWidget = CreateWidget<UAHHUDRootWidget>(GetOwningPlayerController(), RootClass);
	if (RootWidget)
	{
		RootWidget->AddToViewport(100);
		RootWidget->SetObjective(CurrentObjective, ObjectiveIndex, ObjectiveCount);
		if (bMissionComplete)
		{
			RootWidget->ShowMissionComplete();
		}
		if (GetOwningPawn())
		{
			RootWidget->SetPossessedPawn(GetOwningPawn());
		}
	}
}

void AAHCombatHUD::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (RootWidget)
	{
		RootWidget->RemoveFromParent();
		RootWidget = nullptr;
	}
	Super::EndPlay(EndPlayReason);
}

void AAHCombatHUD::DrawHUD()
{
	// Production presentation is UMG. Keeping this override empty prevents Canvas from
	// becoming an accidental second HUD and preserves the existing HUD class contract.
	Super::DrawHUD();
}

void AAHCombatHUD::SetPossessedPawn(APawn* NewPawn)
{
	if (RootWidget)
	{
		RootWidget->SetPossessedPawn(NewPawn);
	}
}

void AAHCombatHUD::ShowHitMarker(bool bHeadshot)
{
	if (RootWidget)
	{
		RootWidget->ShowHitMarker(bHeadshot);
	}
}

void AAHCombatHUD::ShowDamageFeedback(bool bArmorBreakIn, float DirectionAngle)
{
	if (RootWidget)
	{
		RootWidget->ShowDamageFeedback(bArmorBreakIn, DirectionAngle);
	}
}

void AAHCombatHUD::SetObjective(const FText& NewObjective, int32 NewIndex, int32 Count)
{
	CurrentObjective = NewObjective;
	ObjectiveIndex = NewIndex;
	ObjectiveCount = Count;
	if (RootWidget)
	{
		RootWidget->SetObjective(NewObjective, NewIndex, Count);
	}
}

void AAHCombatHUD::ShowMissionComplete()
{
	bMissionComplete = true;
	if (RootWidget)
	{
		RootWidget->ShowMissionComplete();
	}
}

void AAHCombatHUD::ShowMissionFailed(const FText& Headline)
{
	if (RootWidget)
	{
		RootWidget->ShowMissionFailed(Headline);
	}
}

void AAHCombatHUD::HideMissionComplete()
{
	bMissionComplete = false;
	if (RootWidget)
	{
		RootWidget->HideMissionComplete();
	}
}
