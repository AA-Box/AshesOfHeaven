#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "AshesOfHeaven.h"
#include "Gameplay/WorldState/AHPersistentIdComponent.h"
#include "Gameplay/WorldState/AHWorldStateSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
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
	PersistentIdComponent = CreateDefaultSubobject<UAHPersistentIdComponent>(TEXT("PersistentId"));
}

void AAHChapterTerminal::BeginPlay()
{
	Super::BeginPlay();
	if (TerminalWidget)
	{
		if (UClass* WidgetClass = FSoftClassPath(TEXT("/Game/Ashes/UI/Terminal/WBP_TerminalWorld.WBP_TerminalWorld_C")).TryLoadClass<UUserWidget>())
		{
			TerminalWidget->SetWidgetClass(WidgetClass);
			// CasualtyText held the 11,407,231 figure the level is about and nothing ever read it,
			// so the screen showed the widget's authored placeholder instead.
			TerminalWidget->InitWidget();
			SetScreenText(FName(TEXT("TerminalIntel")), CasualtyText);
		}
	}
	if (UAHWorldStateSubsystem* WorldState = GetWorld()->GetSubsystem<UAHWorldStateSubsystem>())
	{
		WorldState->RegisterSavableActor(this);
	}
}

bool AAHChapterTerminal::SetScreenText(FName WidgetName, const FText& Value)
{
	UUserWidget* Screen = TerminalWidget ? TerminalWidget->GetUserWidgetObject() : nullptr;
	UTextBlock* Block = Screen ? Cast<UTextBlock>(Screen->GetWidgetFromName(WidgetName)) : nullptr;
	if (!Block)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase3.2][Terminal] missing screen text block=%s"), *WidgetName.ToString());
		return false;
	}
	Block->SetText(Value);
	return true;
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
		MarkWorldStateDirty();
		return;
	}

	bConfirmed = true;
	SetScreenText(FName(TEXT("TerminalStatus")), ConfirmationText);
	MarkWorldStateDirty();
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

void AAHChapterTerminal::SetPersistentId(const FGuid& PersistentId)
{
	PersistentIdComponent->SetPersistentId(PersistentId);
}

FGuid AAHChapterTerminal::GetPersistentId_Implementation() const
{
	return PersistentIdComponent ? PersistentIdComponent->GetPersistentId() : FGuid();
}

bool AAHChapterTerminal::CaptureWorldState_Implementation(TArray<uint8>& OutPayload) const
{
	const uint8 Flags = (bInspected ? 1u : 0u) | (bConfirmed ? 2u : 0u);
	OutPayload = { Flags };
	return true;
}

bool AAHChapterTerminal::RestoreWorldState_Implementation(const TArray<uint8>& Payload, int32 SavedStateVersion)
{
	if (Payload.Num() != 1 || SavedStateVersion < 0 || SavedStateVersion > GetWorldStateVersion_Implementation())
	{
		return false;
	}
	if (SavedStateVersion == 0)
	{
		// Version 0 stored only the confirmed bit. Confirmed implies the inspection step.
		bConfirmed = Payload[0] != 0;
		bInspected = bConfirmed;
	}
	else
	{
		bInspected = (Payload[0] & 1u) != 0;
		bConfirmed = (Payload[0] & 2u) != 0;
	}
	return true;
}

void AAHChapterTerminal::OnWorldStateRestored_Implementation()
{
}

void AAHChapterTerminal::MarkWorldStateDirty()
{
	if (UAHWorldStateSubsystem* WorldState = GetWorld()->GetSubsystem<UAHWorldStateSubsystem>())
	{
		WorldState->MarkActorDirty(this);
	}
}
