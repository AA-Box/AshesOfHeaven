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
	if (GetWorld()->LineTraceSingleByChannel(Hit, ViewLocation, TraceEnd, ECC_Visibility, Params))
	{
		if (Hit.GetActor() && Hit.GetActor()->GetClass()->ImplementsInterface(UAHInteractable::StaticClass()))
		{
			NewTarget = Hit.GetActor();
		}
	}

	if (NewTarget != CurrentTarget.Get())
	{
		CurrentTarget = NewTarget;
		CurrentPrompt = NewTarget ? IAHInteractable::Execute_GetInteractionPrompt(NewTarget) : FText::GetEmpty();
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
