#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Components/StaticMeshComponent.h"

AAHChapterTerminal::AAHChapterTerminal()
{
	PrimaryActorTick.bCanEverTick = false;
	TerminalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerminalMesh"));
	RootComponent = TerminalMesh;
	TerminalMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	TerminalMesh->SetCollisionResponseToAllChannels(ECR_Block);
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
		return ConfirmationText;
	}
	return bInspected ? FText::FromString(TEXT("INTERACT  CONFIRM FAILSAFE")) : FText::FromString(TEXT("INTERACT  INSPECT TERMINAL"));
}
