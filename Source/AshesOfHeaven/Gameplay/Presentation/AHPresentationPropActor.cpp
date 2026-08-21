#include "Gameplay/Presentation/AHPresentationPropActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AAHPresentationPropActor::AAHPresentationPropActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	RootComponent = PropMesh;
	PropMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
	PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PropMesh->SetCollisionResponseToAllChannels(ECR_Block);
}
