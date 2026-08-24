#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "UObject/SoftObjectPath.h"

AAHChapterTerminal::AAHChapterTerminal()
{
	PrimaryActorTick.bCanEverTick = false;
	TerminalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalMesh"));
	RootComponent = TerminalMesh;
	TerminalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerminalMesh->SetCollisionResponseToAllChannels(ECR_Block);
	TerminalWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("TerminalWidget"));
	TerminalWidget->SetupAttachment(TerminalMesh);
	TerminalWidget->SetWidgetSpace(EWidgetSpace::World);
	TerminalWidget->SetDrawSize(FVector2D(420.0f, 240.0f));
	TerminalWidget->SetRelativeLocation(FVector(0.0f, 0.0f, 135.0f));
	TerminalWidget->SetRelativeRotation(FRotator(0.0f, 180.0f, 90.0f));
}

void AAHChapterTerminal::BeginPlay()
{
	Super::BeginPlay();
	if (TerminalWidget)
	{
		if (UClass* WidgetClass = FSoftClassPath(TEXT("/Game/Ashes/UI/Terminal/WBP_TerminalWorld.WBP_TerminalWorld_C")).TryLoadClass<UUserWidget>())
		{
			TerminalWidget->SetWidgetClass(WidgetClass);
		}
	}
}

void AAHChapterTerminal::Interact_Implementation(AActor* Interactor)
{
	if (bConfirmed)
	{
		return;
	}
	if (!bInspected)
	{
		bInspected = true;
		return;
	}

	bConfirmed = true;
	OnConfirmed.Broadcast();
}

FText AAHChapterTerminal::GetInteractionPrompt_Implementation() const
{
	if (bConfirmed)
	{
		return FText::GetEmpty();
	}
	return bInspected
		? NSLOCTEXT("AshesOfHeaven", "ConfirmFailsafePrompt", "E — CONFIRM FAILSAFE")
		: NSLOCTEXT("AshesOfHeaven", "InspectTerminalPrompt", "E — INSPECT TERMINAL");
}

float AAHChapterTerminal::GetInteractionPriority_Implementation() const
{
	return bConfirmed ? 0.0f : 0.60f;
}

float AAHChapterTerminal::GetObjectiveInteractionPriority_Implementation() const
{
	return bConfirmed ? 0.0f : 1.0f;
}
