#include "Gameplay/Presentation/AHPresentationPropActor.h"

#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"

namespace
{
	UStaticMesh* LoadPresentationShape(const TCHAR* Shape)
	{
		FString AssetName;
		if (FCString::Stricmp(Shape, TEXT("Cube")) == 0) AssetName = TEXT("SM_AH_Cube");
		else if (FCString::Stricmp(Shape, TEXT("Cylinder")) == 0) AssetName = TEXT("SM_AH_Cylinder");
		else if (FCString::Stricmp(Shape, TEXT("Sphere")) == 0) AssetName = TEXT("SM_AH_Sphere");
		else if (FCString::Stricmp(Shape, TEXT("Cone")) == 0) AssetName = TEXT("SM_AH_Cone");
		else if (FCString::Stricmp(Shape, TEXT("Plane")) == 0) AssetName = TEXT("SM_AH_Plane");
		if (AssetName.IsEmpty())
		{
			return nullptr;
		}
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Ashes/Presentation/Meshes/%s.%s"), *AssetName, *AssetName));
	}

	// Authored Erebus kit modules carry their material instances on the asset; a missing
	// module returns null so each style can fall back to its legacy primitive composition.
	UStaticMesh* LoadKitMesh(const TCHAR* Name)
	{
		return LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Ashes/Environment/Erebus/Meshes/%s.%s"), Name, Name));
	}

	bool IsPrimitiveShape(const UStaticMeshComponent* Component)
	{
		const UStaticMesh* Mesh = Component ? Component->GetStaticMesh() : nullptr;
		return Mesh && Mesh->GetPathName().StartsWith(TEXT("/Game/Ashes/Presentation/Meshes/"));
	}

	UMaterialInterface* LoadPresentationMaterial(const TCHAR* MaterialName)
	{
		return LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("/Game/Ashes/Materials/%s.%s"), MaterialName, MaterialName));
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
	PropMesh->SetStaticMesh(LoadPresentationShape(TEXT("Cube")));
	// Presentation-only by contract: gameplay collision is the hidden Phase 3 layer.
	// Level-placed instances must never add collision the greybox does not have.
	PropMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PropMesh->SetGenerateOverlapEvents(false);
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
	UStaticMesh* Cube = LoadPresentationShape(TEXT("Cube"));
	UStaticMesh* Cylinder = LoadPresentationShape(TEXT("Cylinder"));
	UStaticMesh* Sphere = LoadPresentationShape(TEXT("Sphere"));
	UStaticMesh* Cone = LoadPresentationShape(TEXT("Cone"));
	UStaticMesh* Plane = LoadPresentationShape(TEXT("Plane"));

	if (Style.Contains(TEXT("PipeCluster")))
	{
		if (UStaticMesh* PipeLarge = LoadKitMesh(TEXT("SM_Erebus_Pipe_Large_A")))
		{
			PropMesh->SetStaticMesh(PipeLarge);
			PropMesh->SetRelativeScale3D(FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Pipe_Stacked"), PipeLarge, FVector(20.f, 62.f, 84.f), FRotator(0.f, 2.f, 0.f), FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Pipe_Elbow"), LoadKitMesh(TEXT("SM_Erebus_Pipe_Elbow_A")), FVector(320.f, -30.f, 0.f), FRotator(0.f, 12.f, 0.f), FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Pipe_Support_A"), LoadKitMesh(TEXT("SM_Erebus_PipeSupport_A")), FVector(-190.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Pipe_Support_B"), LoadKitMesh(TEXT("SM_Erebus_PipeSupport_A")), FVector(190.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(1.0f));
		}
		else
		{
			PropMesh->SetStaticMesh(Cylinder);
			PropMesh->SetRelativeScale3D(FVector(0.55f, 0.55f, 1.8f));
			AddDetailMesh(this, DetailMeshes, TEXT("Pipe_01"), Cylinder, FVector(75.f, 35.f, 0.f), FRotator(0.f, 0.f, 8.f), FVector(0.35f, 0.35f, 1.6f));
			AddDetailMesh(this, DetailMeshes, TEXT("Pipe_02"), Cylinder, FVector(-70.f, -25.f, 20.f), FRotator(0.f, 0.f, -12.f), FVector(0.28f, 0.28f, 1.9f));
			AddDetailMesh(this, DetailMeshes, TEXT("PipeCap"), Sphere, FVector(0.f, 0.f, 220.f), FRotator::ZeroRotator, FVector(0.65f, 0.65f, 0.25f));
		}
	}
	else if (Style.Contains(TEXT("Barricade")))
	{
		if (UStaticMesh* Barrier = LoadKitMesh(TEXT("SM_Erebus_Barricade_A")))
		{
			PropMesh->SetStaticMesh(Barrier);
			PropMesh->SetRelativeScale3D(FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Barricade_Second"), Barrier, FVector(30.f, 340.f, 0.f), FRotator(0.f, -8.f, 0.f), FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Barricade_Bags"), LoadKitMesh(TEXT("SM_Erebus_SandbagRow_A")), FVector(-90.f, -60.f, 0.f), FRotator(0.f, 86.f, 0.f), FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Barricade_Rubble"), LoadKitMesh(TEXT("SM_Erebus_RubbleMedium_A")), FVector(60.f, -260.f, 0.f), FRotator(0.f, 40.f, 0.f), FVector(1.0f));
		}
		else
		{
			PropMesh->SetStaticMesh(Cube);
			PropMesh->SetRelativeScale3D(FVector(2.4f, 0.35f, 0.55f));
			AddDetailMesh(this, DetailMeshes, TEXT("BraceLeft"), Cylinder, FVector(-145.f, 0.f, -30.f), FRotator(0.f, 0.f, 62.f), FVector(0.22f, 0.22f, 1.4f));
			AddDetailMesh(this, DetailMeshes, TEXT("BraceRight"), Cylinder, FVector(145.f, 0.f, -30.f), FRotator(0.f, 0.f, -62.f), FVector(0.22f, 0.22f, 1.4f));
			AddDetailMesh(this, DetailMeshes, TEXT("WarningPlate"), Plane, FVector(0.f, -38.f, 45.f), FRotator(90.f, 0.f, 0.f), FVector(1.5f, 0.35f, 1.f));
		}
	}
	else if (Style.Contains(TEXT("WreckCluster")))
	{
		PropMesh->SetStaticMesh(LoadKitMesh(TEXT("SM_Erebus_Wreckage_A")));
		PropMesh->SetRelativeScale3D(FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Cluster_Flatbed"), LoadKitMesh(TEXT("SM_Erebus_Wreckage_B")), FVector(240.f, 260.f, 0.f), FRotator(0.f, 140.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Cluster_RubbleL"), LoadKitMesh(TEXT("SM_Erebus_RubbleLarge_A")), FVector(-190.f, 150.f, 0.f), FRotator(0.f, 60.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Cluster_RubbleM"), LoadKitMesh(TEXT("SM_Erebus_RubbleMedium_A")), FVector(120.f, -180.f, 0.f), FRotator(0.f, 200.f, 0.f), FVector(1.2f));
		AddDetailMesh(this, DetailMeshes, TEXT("Cluster_Barrel"), LoadKitMesh(TEXT("SM_Erebus_Barrel_A")), FVector(-140.f, -150.f, 0.f), FRotator(4.f, 0.f, 12.f), FVector(1.0f));
	}
	else if (Style.Contains(TEXT("Wreck")))
	{
		if (UStaticMesh* Hull = LoadKitMesh(TEXT("SM_Erebus_Wreckage_A")))
		{
			PropMesh->SetStaticMesh(Hull);
			PropMesh->SetRelativeScale3D(FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Wreck_Rubble"), LoadKitMesh(TEXT("SM_Erebus_RubbleMedium_A")), FVector(-150.f, 120.f, 0.f), FRotator(0.f, 80.f, 0.f), FVector(1.0f));
		}
		else
		{
			PropMesh->SetStaticMesh(Cube);
			PropMesh->SetRelativeRotation(FRotator(0.f, 12.f, -8.f));
			PropMesh->SetRelativeScale3D(FVector(1.6f, 0.8f, 0.45f));
			AddDetailMesh(this, DetailMeshes, TEXT("WreckShell"), Cylinder, FVector(80.f, 0.f, 60.f), FRotator(0.f, 90.f, 0.f), FVector(0.55f, 0.55f, 1.2f));
			AddDetailMesh(this, DetailMeshes, TEXT("WreckWheel"), Cylinder, FVector(-70.f, 80.f, -25.f), FRotator(90.f, 0.f, 0.f), FVector(0.8f, 0.8f, 0.18f));
			AddDetailMesh(this, DetailMeshes, TEXT("WreckWheel2"), Cylinder, FVector(-70.f, -80.f, -25.f), FRotator(90.f, 0.f, 0.f), FVector(0.8f, 0.8f, 0.18f));
		}
	}
	else if (Style.Contains(TEXT("Bunker")))
	{
		PropMesh->SetStaticMesh(LoadKitMesh(TEXT("SM_Erebus_BunkerWall_A")));
		PropMesh->SetRelativeScale3D(FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Bunker_Back"), LoadKitMesh(TEXT("SM_Erebus_BunkerWall_A")), FVector(0.f, 420.f, 0.f), FRotator(0.f, 180.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Bunker_CornerL"), LoadKitMesh(TEXT("SM_Erebus_BunkerCorner_A")), FVector(-320.f, 100.f, 0.f), FRotator(0.f, 0.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Bunker_CornerR"), LoadKitMesh(TEXT("SM_Erebus_BunkerCorner_A")), FVector(320.f, 320.f, 0.f), FRotator(0.f, 180.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Bunker_Roof"), LoadKitMesh(TEXT("SM_Erebus_BunkerRoof_A")), FVector(0.f, 210.f, 348.f), FRotator::ZeroRotator, FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Bunker_Bags"), LoadKitMesh(TEXT("SM_Erebus_SandbagRow_A")), FVector(0.f, -120.f, 0.f), FRotator(0.f, 4.f, 0.f), FVector(1.1f));
	}
	else if (Style.Contains(TEXT("DefensivePosition")))
	{
		PropMesh->SetStaticMesh(LoadKitMesh(TEXT("SM_Erebus_ArmorBarrier_A")));
		PropMesh->SetRelativeScale3D(FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Def_BarricadeL"), LoadKitMesh(TEXT("SM_Erebus_Barricade_A")), FVector(-60.f, -360.f, 0.f), FRotator(0.f, 14.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Def_BarricadeR"), LoadKitMesh(TEXT("SM_Erebus_Barricade_A")), FVector(-60.f, 360.f, 0.f), FRotator(0.f, -14.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Def_BagsL"), LoadKitMesh(TEXT("SM_Erebus_SandbagRow_A")), FVector(60.f, -220.f, 0.f), FRotator(0.f, 92.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Def_BagsR"), LoadKitMesh(TEXT("SM_Erebus_SandbagRow_A")), FVector(60.f, 220.f, 0.f), FRotator(0.f, 88.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Def_Crate"), LoadKitMesh(TEXT("SM_Erebus_Crate_A")), FVector(150.f, 60.f, 0.f), FRotator(0.f, 28.f, 0.f), FVector(1.0f));
	}
	else if (Style.Contains(TEXT("RuinedBlock")))
	{
		PropMesh->SetStaticMesh(LoadKitMesh(TEXT("SM_Erebus_RuinedFacade_A")));
		PropMesh->SetRelativeScale3D(FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Ruin_Side"), LoadKitMesh(TEXT("SM_Erebus_RuinedFacade_B")), FVector(440.f, 330.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Ruin_Floor"), LoadKitMesh(TEXT("SM_Erebus_BrokenFloor_A")), FVector(180.f, 220.f, 0.f), FRotator(0.f, 12.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Ruin_RubbleA"), LoadKitMesh(TEXT("SM_Erebus_RubbleLarge_A")), FVector(-120.f, 160.f, 0.f), FRotator(0.f, 220.f, 0.f), FVector(1.1f));
		AddDetailMesh(this, DetailMeshes, TEXT("Ruin_RubbleB"), LoadKitMesh(TEXT("SM_Erebus_RubbleMedium_A")), FVector(240.f, -120.f, 0.f), FRotator(0.f, 90.f, 0.f), FVector(1.0f));
	}
	else if (Style.Contains(TEXT("PropCluster")))
	{
		PropMesh->SetStaticMesh(LoadKitMesh(TEXT("SM_Erebus_Crate_A")));
		PropMesh->SetRelativeScale3D(FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Props_CrateB"), LoadKitMesh(TEXT("SM_Erebus_Crate_A")), FVector(90.f, 110.f, 0.f), FRotator(0.f, 34.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Props_CrateOpen"), LoadKitMesh(TEXT("SM_Erebus_CrateOpen_A")), FVector(-110.f, 60.f, 0.f), FRotator(0.f, -18.f, 0.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Props_Barrel"), LoadKitMesh(TEXT("SM_Erebus_Barrel_A")), FVector(40.f, -130.f, 0.f), FRotator(0.f, 0.f, 6.f), FVector(1.0f));
		AddDetailMesh(this, DetailMeshes, TEXT("Props_Beam"), LoadKitMesh(TEXT("SM_Erebus_IndustrialSupport_A")), FVector(-60.f, -60.f, 40.f), FRotator(0.f, 70.f, -12.f), FVector(1.0f));
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
		if (UStaticMesh* WorkLight = LoadKitMesh(TEXT("SM_Erebus_WorkLight_A")))
		{
			// Authored kit work-light: the primitive-composed version tripped the
			// corridor debug-primitive audit (visual gate #31).
			PropMesh->SetStaticMesh(WorkLight);
			PropMesh->SetRelativeScale3D(FVector(1.0f));
		}
		else
		{
			// Legacy primitive fallback only.
			PropMesh->SetStaticMesh(Cylinder);
			PropMesh->SetRelativeScale3D(FVector(0.22f, 0.22f, 1.4f));
			AddDetailMesh(this, DetailMeshes, TEXT("LightHousing"), Cube, FVector(0.f, 0.f, 138.f), FRotator(0.f, 0.f, 0.f), FVector(0.42f, 0.30f, 0.22f));
			AddDetailMesh(this, DetailMeshes, TEXT("LightShade"), Cone, FVector(0.f, 0.f, 128.f), FRotator(180.f, 0.f, 0.f), FVector(0.34f, 0.34f, 0.24f));
			AddDetailMesh(this, DetailMeshes, TEXT("LightBase"), Cube, FVector(0.f, 0.f, -58.f), FRotator::ZeroRotator, FVector(0.55f, 0.55f, 0.08f));
		}
	}
	else if (Style.Contains(TEXT("BlastWall")))
	{
		if (UStaticMesh* Wall = LoadKitMesh(TEXT("SM_Erebus_BlastWall_A")))
		{
			PropMesh->SetStaticMesh(Wall);
			PropMesh->SetRelativeScale3D(FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Blast_Damaged"), LoadKitMesh(TEXT("SM_Erebus_BlastWall_B")), FVector(452.f, 14.f, 0.f), FRotator(0.f, 5.f, 0.f), FVector(1.0f));
			AddDetailMesh(this, DetailMeshes, TEXT("Blast_Rubble"), LoadKitMesh(TEXT("SM_Erebus_RubbleMedium_A")), FVector(240.f, -90.f, 0.f), FRotator(0.f, 150.f, 0.f), FVector(1.0f));
		}
		else
		{
			PropMesh->SetStaticMesh(Cube);
			PropMesh->SetRelativeScale3D(FVector(2.8f, 0.24f, 1.8f));
			AddDetailMesh(this, DetailMeshes, TEXT("BlastBeamLeft"), Cylinder, FVector(-210.f, 0.f, 0.f), FRotator(0.f, 0.f, 76.f), FVector(0.18f, 0.18f, 1.7f));
			AddDetailMesh(this, DetailMeshes, TEXT("BlastBeamRight"), Cylinder, FVector(210.f, 0.f, 0.f), FRotator(0.f, 0.f, -76.f), FVector(0.18f, 0.18f, 1.7f));
		}
	}
	else
	{
		PropMesh->SetStaticMesh(Cube);
		PropMesh->SetRelativeScale3D(FVector(1.f));
		AddDetailMesh(this, DetailMeshes, TEXT("DetailSphere"), Sphere, FVector(0.f, 0.f, 125.f), FRotator::ZeroRotator, FVector(0.35f));
	}

	// Legacy primitive shapes need a material assignment; authored kit modules carry their
	// own material instances on the asset and must not be stomped.
	const bool bCathedral = Style.Contains(TEXT("Cathedral")) || Style.Contains(TEXT("Glyph"));
	const bool bTransit = Style.Contains(TEXT("Transit"));
	UMaterialInterface* PropMaterial = LoadPresentationMaterial(
		bCathedral ? TEXT("M_CathedralMatter") : bTransit ? TEXT("M_HumanPaintedMetal") : TEXT("M_HumanMetal"));
	if (PropMaterial && IsPrimitiveShape(PropMesh))
	{
		PropMesh->SetMaterial(0, PropMaterial);
	}
	for (UStaticMeshComponent* Detail : DetailMeshes)
	{
		if (Detail && IsPrimitiveShape(Detail))
		{
			const bool bGlyphDetail = Style.Contains(TEXT("Glyph")) && Detail->GetFName() == TEXT("GlyphInset");
			Detail->SetMaterial(0, bGlyphDetail ? LoadPresentationMaterial(TEXT("M_EmissiveGlyph")) : PropMaterial);
		}
	}
}
