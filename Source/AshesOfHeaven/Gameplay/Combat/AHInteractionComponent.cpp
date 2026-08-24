#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

UAHInteractionComponent::UAHInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAHInteractionComponent::BeginPlay()
{
	Super::BeginPlay();
	SetComponentTickInterval(0.08f);
}

void UAHInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APawn* Pawn = Cast<APawn>(GetOwner());
	APlayerController* Controller = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr;
	if (!Controller || !Controller->PlayerCameraManager)
	{
		return;
	}

	FVector ViewLocation;
	FRotator ViewRotation;
	Controller->GetPlayerViewPoint(ViewLocation, ViewRotation);
	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * InteractionDistance);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHInteraction), true, GetOwner());
	FHitResult Hit;
	AActor* NewTarget = nullptr;
	FText NewPrompt;
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		if (Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UAHInteractable::StaticClass()))
		{
			// An empty prompt is how an interactable says it has nothing to offer right now - a
			// combatant is only lootable once it is a corpse. Treat that as no target at all,
			// rather than putting a blank prompt on the HUD every time one crosses the crosshair.
			NewPrompt = IAHInteractable::Execute_GetInteractionPrompt(Hit.GetActor());
			if (!NewPrompt.IsEmpty())
			{
				NewTarget = Hit.GetActor();
			}
		}
	}

	// The prompt is re-read every tick, so a target whose state changed under the crosshair
	// (a body stripped of its rifle) updates without having to look away and back.
	if (NewTarget != CurrentTarget.Get() || !NewPrompt.EqualTo(CurrentPrompt))
	{
		CurrentTarget = NewTarget;
		CurrentPrompt = NewPrompt;
		OnTargetChanged.Broadcast(NewTarget);
	}
}

void UAHInteractionComponent::Interact()
{
	if (AActor* Target = CurrentTarget.Get())
	{
		IAHInteractable::Execute_Interact(Target, GetOwner());
	}
}
