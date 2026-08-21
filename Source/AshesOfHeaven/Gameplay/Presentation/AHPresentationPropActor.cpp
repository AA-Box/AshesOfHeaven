#include "Gameplay/Presentation/AHPresentationPropActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

namespace
{
	UStaticMesh* LoadBasicShape(const TCHAR* Shape)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Engine/BasicShapes/%s.%s"), Shape, Shape));
	}

	void AddDetailMesh(
		AAHPresentationPropActor* Owner,
		TArray<TObjectPtr<UStaticMeshComponent>>& Details,
		const TCHAR* Name,
		UStaticMesh* Mesh,
		const FVector& Location,
		const FRotator& Rotation,
		const FVector& Scale)
	{
		if (!Owner || !Mesh)
		{
			return;
		}
		UStaticMeshComponent* Component = NewObject<UStaticMeshComponent>(Owner, FName(Name));
		Component->SetupAttachment(Owner->GetRootComponent());
		Component->SetStaticMesh(Mesh);
		Component->SetRelativeLocation(Location);
		Component->SetRelativeRotation(Rotation);
		Component->SetRelativeScale3D(Scale);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetGenerateOverlapEvents(false);
		Component->RegisterComponent();
		Details.Add(Component);
	}
}

AAHPresentationPropActor::AAHPresentationPropActor()
{
	PrimaryActorTick.bCanEverTick = false;
	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	RootComponent = PropMesh;
	PropMesh->SetStaticMesh(LoadBasicShape(TEXT("Cube")));
	PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PropMesh->SetCollisionResponseToAllChannels(ECR_Block);
}

void AAHPresentationPropActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	for (UStaticMeshComponent* Detail : DetailMeshes)
	{
		if (Detail)
		{
			Detail->DestroyComponent();
		}
	}
	DetailMeshes.Reset();

	FString Style = PresentationStyle.IsNone() ? GetClass()->GetName() : PresentationStyle.ToString();
	Style.RemoveFromStart(TEXT("BP_"));
	UStaticMesh* Cube = LoadBasicShape(TEXT("Cube"));
	UStaticMesh* Cylinder = LoadBasicShape(TEXT("Cylinder"));
	UStaticMesh* Sphere = LoadBasicShape(TEXT("Sphere"));
	UStaticMesh* Cone = LoadBasicShape(TEXT("Cone"));
	UStaticMesh* Plane = LoadBasicShape(TEXT("Plane"));

	if (Style.Contains(TEXT("PipeCluster")))
	{
		PropMesh->SetStaticMesh(Cylinder);
		PropMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.8f));
		AddDetailMesh(this, DetailMeshes, TEXT("Pipe_01"), Cylinder, FVector(75.f, 35.f, 0.f), FRotator(0.f, 0.f, 8.f), FVector(0.35f, 0.35f, 1.6f));
		AddDetailMesh(this, DetailMeshes, TEXT("Pipe_02"), Cylinder, FVector(-70.f, -25.f, 20.f), FRotator(0.f, 0.f, -12.f), FVector(0.28f, 0.28f, 1.9f));
		AddDetailMesh(this, DetailMeshes, TEXT("PipeCap"), Sphere, FVector(0.f, 0.f, 220.f), FRotator::ZeroRotator, FVector(0.65f, 0.65f, 0.25f));
	}
	else if (Style.Contains(TEXT("Barricade")))
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeScale3D(FVector(2.4f, 0.35f, 0.55f));
		AddDetailMesh(this, DetailMeshes, TEXT("BraceLeft"), Cylinder, FVector(-145.f, 0.f, -30.f), FRotator(0.f, 0.f, 62.f), FVector(0.22f, 0.22f, 1.4f));
		AddDetailMesh(this, DetailMeshes, TEXT("BraceRight"), Cylinder, FVector(145.f, 0.f, -30.f), FRotator(0.f, 0.f, -62.f), FVector(0.22f, 0.22f, 1.4f));
		AddDetailMesh(this, DetailMeshes, TEXT("WarningPlate"), Plane, FVector(0.f, -38.f, 45.f), FRotator(90.f, 0.f, 0.f), FVector(1.5f, 0.35f, 1.f));
	}
	else if (Style.Contains(TEXT("Wreck")))
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeRotation(FRotator(0.f, 12.f, -8.f));
		PropMesh->SetRelativeScale3D(FVector(1.6f, 0.8f, 0.45f));
		AddDetailMesh(this, DetailMeshes, TEXT("WreckShell"), Cylinder, FVector(80.f, 0.f, 60.f), FRotator(0.f, 90.f, 0.f), FVector(0.55f, 0.55f, 1.2f));
		AddDetailMesh(this, DetailMeshes, TEXT("WreckWheel"), Cylinder, FVector(-70.f, 80.f, -25.f), FRotator(90.f, 0.f, 0.f), FVector(0.8f, 0.8f, 0.18f));
		AddDetailMesh(this, DetailMeshes, TEXT("WreckWheel2"), Cylinder, FVector(-70.f, -80.f, -25.f), FRotator(90.f, 0.f, 0.f), FVector(0.8f, 0.8f, 0.18f));
	}
	else if (Style.Contains(TEXT("Transit_Sign")))
	{
		PropMesh->SetStaticMesh(Cylinder);
		PropMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 2.4f));
		AddDetailMesh(this, DetailMeshes, TEXT("SignFace"), Cube, FVector(0.f, 0.f, 210.f), FRotator::ZeroRotator, FVector(1.45f, 0.18f, 0.55f));
		AddDetailMesh(this, DetailMeshes, TEXT("SignGlow"), Plane, FVector(0.f, -20.f, 210.f), FRotator(90.f, 0.f, 0.f), FVector(1.0f, 0.34f, 1.f));
	}
	else if (Style.Contains(TEXT("Transit_Bench")))
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeScale3D(FVector(1.5f, 0.48f, 0.14f));
		AddDetailMesh(this, DetailMeshes, TEXT("BenchLegLeft"), Cylinder, FVector(-100.f, 0.f, -65.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.8f));
		AddDetailMesh(this, DetailMeshes, TEXT("BenchLegRight"), Cylinder, FVector(100.f, 0.f, -65.f), FRotator::ZeroRotator, FVector(0.16f, 0.16f, 0.8f));
		AddDetailMesh(this, DetailMeshes, TEXT("BenchBack"), Cube, FVector(0.f, 35.f, 62.f), FRotator(0.f, 0.f, -8.f), FVector(1.5f, 0.12f, 0.55f));
	}
	else if (Style.Contains(TEXT("Cathedral_Fin")))
	{
		PropMesh->SetStaticMesh(Cone);
		PropMesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 2.8f));
		AddDetailMesh(this, DetailMeshes, TEXT("FinLeft"), Plane, FVector(0.f, 95.f, 70.f), FRotator(0.f, 20.f, 0.f), FVector(1.0f, 1.8f, 1.f));
		AddDetailMesh(this, DetailMeshes, TEXT("FinRight"), Plane, FVector(0.f, -95.f, 70.f), FRotator(0.f, -20.f, 0.f), FVector(1.0f, 1.8f, 1.f));
		AddDetailMesh(this, DetailMeshes, TEXT("FinCore"), Cylinder, FVector(0.f, 0.f, 150.f), FRotator::ZeroRotator, FVector(0.22f, 0.22f, 1.4f));
	}
	else if (Style.Contains(TEXT("GlyphPanel")))
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeScale3D(FVector(1.25f, 0.22f, 1.7f));
		AddDetailMesh(this, DetailMeshes, TEXT("GlyphInset"), Plane, FVector(0.f, -25.f, 0.f), FRotator(90.f, 0.f, 0.f), FVector(1.0f, 1.35f, 1.f));
		AddDetailMesh(this, DetailMeshes, TEXT("GlyphCrown"), Cone, FVector(0.f, 0.f, 195.f), FRotator::ZeroRotator, FVector(0.55f, 0.55f, 0.55f));
	}
	else if (Style.Contains(TEXT("ExpeditionLight")))
	{
		PropMesh->SetStaticMesh(Cylinder);
		PropMesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 1.2f));
		AddDetailMesh(this, DetailMeshes, TEXT("LightHousing"), Sphere, FVector(0.f, 0.f, 145.f), FRotator::ZeroRotator, FVector(0.85f, 0.85f, 0.6f));
		AddDetailMesh(this, DetailMeshes, TEXT("LightCrown"), Cone, FVector(0.f, 0.f, 215.f), FRotator::ZeroRotator, FVector(0.5f, 0.5f, 0.5f));
	}
	else if (Style.Contains(TEXT("BlastWall")))
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeScale3D(FVector(2.8f, 0.24f, 1.8f));
		AddDetailMesh(this, DetailMeshes, TEXT("BlastBeamLeft"), Cylinder, FVector(-210.f, 0.f, 0.f), FRotator(0.f, 0.f, 76.f), FVector(0.18f, 0.18f, 1.7f));
		AddDetailMesh(this, DetailMeshes, TEXT("BlastBeamRight"), Cylinder, FVector(210.f, 0.f, 0.f), FRotator(0.f, 0.f, -76.f), FVector(0.18f, 0.18f, 1.7f));
	}
	else
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeScale3D(FVector(1.f));
		AddDetailMesh(this, DetailMeshes, TEXT("DetailSphere"), Sphere, FVector(0.f, 0.f, 125.f), FRotator::ZeroRotator, FVector(0.35f));
	}
}
