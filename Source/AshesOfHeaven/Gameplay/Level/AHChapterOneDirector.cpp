#include "Gameplay/Level/AHChapterOneDirector.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHHumanSoldierCharacter.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Characters/AHVeilWardenCharacter.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTerminal.h"
#include "Gameplay/Chapter/AHChapterTrigger.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "Gameplay/Checkpoints/AHCheckpointActor.h"
#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Encounters/AHCombatEncounter.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Gameplay/Weapons/AHWeaponPickup.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Presentation/AHPresentationData.h"
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/TextRenderComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PointLight.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkyLight.h"
#include "Animation/SkeletalMeshActor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Parse.h"
#include "NavigationSystem.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	const FName OpeningObjective(TEXT("Ch01_ReachDefensiveLine"));
	const FName OpeningBattleObjective(TEXT("Ch01_SurviveOpeningBattle"));
	const FName TransitObjective(TEXT("Ch01_ReachTransitStation"));
	const FName RevelationObjective(TEXT("Ch01_SurviveVeilRevelation"));
	const FName BattlefieldObjective(TEXT("Ch01_CrossBattlefield"));
	const FName ManticoreObjective(TEXT("Ch01_EnterManticore"));
	const FName ApproachObjective(TEXT("Ch01_ReachCathedralApproach"));
	const FName FailsafeObjective(TEXT("Ch01_ActivatePlanetaryFailsafe"));
	const FName TerminalObjective(TEXT("Ch01_ReachTerminal"));
	const FName ConfirmObjective(TEXT("Ch01_ConfirmFailsafe"));
	const FName EscapeObjective(TEXT("Ch01_EscapeCathedral"));
	const FName DestructionObjective(TEXT("Ch01_SurviveDestruction"));
	const FName MayaObjective(TEXT("Ch01_MeetMaya"));
	const FName NysaObjective(TEXT("Ch01_ReceiveNysa"));
	const FName FleetObjective(TEXT("Ch01_FleetDeparture"));
	const FName StarsObjective(TEXT("Ch01_SeeDisappearingStars"));
	const FName TitleObjective(TEXT("Ch01_TitleReveal"));

	FAHDialogueLine Line(const TCHAR* Speaker, const TCHAR* Text, float Duration = 2.8f)
	{
		FAHDialogueLine Result;
		Result.Speaker = FName(Speaker);
		Result.Subtitle = FText::FromString(Text);
		Result.Duration = Duration;
		return Result;
	}

	TArray<FAHDialogueLine> OpeningLines()
	{
		return {Line(TEXT("CHILD"), TEXT("Did we win?"), 2.4f), Line(TEXT("LUCIAN"), TEXT("For a while."), 3.2f)};
	}

	TArray<FAHDialogueLine> RevelationLines()
	{
		return {Line(TEXT("VEIL"), TEXT("You came back."), 2.8f), Line(TEXT("KELL"), TEXT("Commander?"), 2.2f), Line(TEXT("VEIL"), TEXT("You always do."), 3.0f)};
	}

	TArray<FAHDialogueLine> OrderLines()
	{
		return {
			Line(TEXT("ADMIRAL"), TEXT("They're inside the core."), 2.4f),
			Line(TEXT("LUCIAN"), TEXT("Then reinforce us."), 2.3f),
			Line(TEXT("LUCIAN"), TEXT("How long?"), 1.8f),
			Line(TEXT("ADMIRAL"), TEXT("Nine minutes."), 2.0f),
			Line(TEXT("LUCIAN"), TEXT("There are eleven million people here."), 2.8f),
			Line(TEXT("ADMIRAL"), TEXT("I know."), 1.8f),
			Line(TEXT("LUCIAN"), TEXT("Say the order."), 2.2f),
			Line(TEXT("LUCIAN"), TEXT("Say it."), 1.8f),
			Line(TEXT("ADMIRAL"), TEXT("Destroy Erebus."), 3.0f)
		};
	}

	TArray<FAHDialogueLine> KellLines()
	{
		return {Line(TEXT("KELL"), TEXT("How many?"), 2.0f), Line(TEXT("KELL"), TEXT("How many people?"), 2.4f), Line(TEXT("LUCIAN"), TEXT("Everyone."), 2.8f)};
	}

	TArray<FAHDialogueLine> SaelLines()
	{
		return {
			Line(TEXT("SAEL"), TEXT("Lucian."), 2.0f),
			Line(TEXT("LUCIAN"), TEXT("Who are you?"), 2.2f),
			Line(TEXT("SAEL"), TEXT("You have asked me that before."), 3.0f),
			Line(TEXT("LUCIAN"), TEXT("I've never seen you."), 2.5f),
			Line(TEXT("SAEL"), TEXT("You never remember."), 3.0f),
			Line(TEXT("SAEL"), TEXT("Do not activate it."), 2.5f),
			Line(TEXT("LUCIAN"), TEXT("You invaded my world."), 2.6f),
			Line(TEXT("SAEL"), TEXT("Because the alternative is worse."), 3.0f)
		};
	}

	TArray<FAHDialogueLine> MayaLines()
	{
		return {
			Line(TEXT("MAYA"), TEXT("We need you."), 2.2f),
			Line(TEXT("LUCIAN"), TEXT("I'm retired."), 2.2f),
			Line(TEXT("MAYA"), TEXT("Eleven million people would disagree."), 3.0f),
			Line(TEXT("MAYA"), TEXT("That message came from something three billion years old."), 3.2f),
			Line(TEXT("LUCIAN"), TEXT("When?"), 1.6f),
			Line(TEXT("MAYA"), TEXT("Six hours ago."), 2.0f),
			Line(TEXT("LUCIAN"), TEXT("Then we're already late."), 2.8f)
		};
	}

	TArray<FAHDialogueLine> NysaLines()
	{
		return {Line(TEXT("NYSA TRANSMISSION"), TEXT("LUCIAN VARR."), 2.0f), Line(TEXT("NYSA TRANSMISSION"), TEXT("Modern military identification confirmed."), 2.8f)};
	}

	TArray<FAHDialogueLine> OtherLucianLines()
	{
		return {Line(TEXT("OTHER LUCIAN"), TEXT("No more time."), 2.6f)};
	}

	FAutoConsoleCommandWithWorldAndArgs AHSpatialAuditCommand(
		TEXT("AH.Debug.SpatialAudit"),
		TEXT("Audit the current Chapter One stage and every canonical stage spatial definition."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (!World)
			{
				return;
			}
			TActorIterator<AAHChapterOneDirector> It(World);
			if (It)
			{
				It->DebugSpatialAudit();
			}
		}));
}

AAHChapterOneDirector::AAHChapterOneDirector()
{
	PrimaryActorTick.bCanEverTick = true;
	BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube"));
	// Gameplay collision still uses the engine cube, but no normal-runtime visual may
	// resolve to the old checker/grid material.
	BlockMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_Concrete_Wet.MI_Concrete_Wet"));
	CathedralMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_CathedralMatter_Dark.MI_CathedralMatter_Dark"));
	HumanMetalMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_HumanMetal_Dark.MI_HumanMetal_Dark"));
	ConcreteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_Concrete_Wet.MI_Concrete_Wet"));
	VeilObsidianMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_VeilObsidian_Black.MI_VeilObsidian_Black"));
	EmissiveTechnologyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_EmissiveGlyph_Cyan.MI_EmissiveGlyph_Cyan"));
	if (!CathedralMaterial) CathedralMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_CathedralMatter.M_CathedralMatter"));
	if (!HumanMetalMaterial) HumanMetalMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"));
	if (!ConcreteMaterial) ConcreteMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_Concrete.M_Concrete"));
	if (!VeilObsidianMaterial) VeilObsidianMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"));
	if (!EmissiveTechnologyMaterial) EmissiveTechnologyMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph"));
}

void AAHChapterOneDirector::BeginPlay()
{
	Super::BeginPlay();
	if (!GetGameInstance())
	{
		return;
	}

	Chapter = GetGameInstance()->GetSubsystem<UAHChapterSubsystem>();
	Dialogue = GetWorld()->GetSubsystem<UAHDialogueSubsystem>();
	Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>();
	BuildMissionGraph();
	BuildStageAnchors();
	BuildGreybox();
	BuildVisualArtTargets();
	BuildMissionActors();
	ConfigureObjectives();

	if (Objectives)
	{
		Objectives->OnObjectiveCompleted.AddDynamic(this, &AAHChapterOneDirector::HandleObjectiveCompleted);
		Objectives->OnMissionComplete.AddDynamic(this, &AAHChapterOneDirector::HandleMissionComplete);
	}
	if (Dialogue)
	{
		Dialogue->OnSequenceComplete.AddDynamic(this, &AAHChapterOneDirector::HandleDialogueComplete);
	}

	StartStage(Chapter ? Chapter->GetStage() : EAHChapterStage::OpeningBlack);
	LogPresentationState(GetCurrentStage());

#if !UE_BUILD_SHIPPING
	FString ArtTarget;
	if (FParse::Value(FCommandLine::Get(), TEXT("ArtTarget="), ArtTarget))
	{
		FTimerDelegate ArtTargetDelegate;
		ArtTargetDelegate.BindUObject(this, &AAHChapterOneDirector::ActivateArtTargetView, ArtTarget);
		GetWorldTimerManager().SetTimerForNextTick(ArtTargetDelegate);
	}
#endif
}

void AAHChapterOneDirector::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Chapter)
	{
		return;
	}

	StageElapsed += DeltaSeconds;
	Chapter->TickCountdown(DeltaSeconds);

	if (GetCurrentStage() == EAHChapterStage::OpeningBlack && !bOpeningSequenceStarted && StageElapsed > 0.5f)
	{
		StartStage(EAHChapterStage::OpeningBlack);
	}

	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	if (Player && GetWorld()->GetTimeSeconds() - LastSpatialRecoveryTime > 1.0f)
	{
		const FAHStageSpatialDefinition& CurrentDefinition = AHChapterSpatial::GetStageDefinition(GetCurrentStage());
		if (PlayerLocation.ContainsNaN() || PlayerLocation.Z < CurrentDefinition.ExpectedBoundsMin.Z - 300.0f)
		{
			LastSpatialRecoveryTime = GetWorld()->GetTimeSeconds();
			EnsureStageSpatialValidity(GetCurrentStage(), TEXT("FallRecovery"));
		}
	}
	if (GetCurrentStage() == EAHChapterStage::CathedralApproach && (PlayerLocation.X > 13700.0f || (Manticore && Manticore->GetActorLocation().X > 13700.0f)))
	{
		CompleteCurrentObjective();
	}
	if (GetCurrentStage() == EAHChapterStage::ErebusDestruction)
	{
		DestructionFadeAlpha = FMath::Clamp(StageElapsed / 6.0f, 0.0f, 1.0f);
	}
	if (GetCurrentStage() == EAHChapterStage::FleetDeparture && StageElapsed > 5.0f)
	{
		CompleteCurrentObjective();
	}
	if (GetCurrentStage() == EAHChapterStage::StarsDisappearing && StageElapsed > 5.0f)
	{
		CompleteCurrentObjective();
	}
	if (GetCurrentStage() == EAHChapterStage::ChapterComplete && StageElapsed > 4.0f && Objectives && !Objectives->IsMissionComplete())
	{
		CompleteCurrentObjective();
	}
}

EAHChapterStage AAHChapterOneDirector::GetCurrentStage() const
{
	return Chapter ? Chapter->GetStage() : EAHChapterStage::OpeningBlack;
}

bool AAHChapterOneDirector::IsOpeningBlack() const
{
	return GetCurrentStage() == EAHChapterStage::OpeningBlack && StageElapsed < 4.5f;
}

bool AAHChapterOneDirector::IsOpeningPresentationActive() const
{
	return GetCurrentStage() == EAHChapterStage::OpeningBlack
		|| (Dialogue && Dialogue->HasActiveDialogue() && Dialogue->GetCurrentSequence() == FName(TEXT("Ch01_Opening")));
}

bool AAHChapterOneDirector::IsTitleReveal() const
{
	return GetCurrentStage() == EAHChapterStage::ChapterComplete;
}

void AAHChapterOneDirector::BuildMissionGraph()
{
	MissionStages = {
		{EAHChapterStage::OpeningBlack, NAME_None, FText::FromString(TEXT("OPENING")), true},
		{EAHChapterStage::ErebusOpening, OpeningObjective, FText::FromString(TEXT("REACH THE DEFENSIVE LINE")), true},
		{EAHChapterStage::OpeningBattle, OpeningBattleObjective, FText::FromString(TEXT("HOLD THE EREBUS LINE")), true},
		{EAHChapterStage::TransitStation, TransitObjective, FText::FromString(TEXT("ENTER THE TRANSIT STATION")), true},
		{EAHChapterStage::VeilRevelation, RevelationObjective, FText::FromString(TEXT("SURVIVE THE REVELATION")), true},
		{EAHChapterStage::OpenBattlefield, BattlefieldObjective, FText::FromString(TEXT("CROSS THE OPEN BATTLEFIELD")), true},
		{EAHChapterStage::ManticoreSection, ManticoreObjective, FText::FromString(TEXT("ENTER THE MANTICORE")), true},
		{EAHChapterStage::CathedralApproach, ApproachObjective, FText::FromString(TEXT("REACH THE CATHEDRAL APPROACH")), true},
		{EAHChapterStage::FailsafeOrder, FailsafeObjective, FText::FromString(TEXT("ACTIVATE PLANETARY FAILSAFE")), true},
		{EAHChapterStage::CathedralInterior, TerminalObjective, FText::FromString(TEXT("REACH THE FAILSAFE TERMINAL")), true},
		{EAHChapterStage::SaelTransmission, TerminalObjective, FText::FromString(TEXT("HEAR THE SAEL TRANSMISSION")), false},
		{EAHChapterStage::FailsafeTerminal, ConfirmObjective, FText::FromString(TEXT("CONFIRM PLANETARY FAILSAFE")), true},
		{EAHChapterStage::Escape, EscapeObjective, FText::FromString(TEXT("ESCAPE THE CATHEDRAL")), true},
		{EAHChapterStage::OtherLucian, EscapeObjective, FText::FromString(TEXT("THE OTHER LUCIAN")), false},
		{EAHChapterStage::ErebusDestruction, DestructionObjective, FText::FromString(TEXT("SURVIVE THE DESTRUCTION")), true},
		{EAHChapterStage::TenYearsLater, MayaObjective, FText::FromString(TEXT("MEET CAPTAIN MAYA SOL")), true},
		{EAHChapterStage::MayaScene, NysaObjective, FText::FromString(TEXT("RECEIVE THE NYSA TRANSMISSION")), true},
		{EAHChapterStage::NysaTransmission, FleetObjective, FText::FromString(TEXT("PREPARE FOR FLEET DEPARTURE")), true},
		{EAHChapterStage::FleetDeparture, StarsObjective, FText::FromString(TEXT("WATCH THE STARS")), true},
		{EAHChapterStage::StarsDisappearing, TitleObjective, FText::FromString(TEXT("THE SIGNAL CONTINUES")), true},
		{EAHChapterStage::ChapterComplete, NAME_None, FText::FromString(TEXT("CHAPTER ONE COMPLETE")), true}
	};
}

void AAHChapterOneDirector::ConfigureObjectives()
{
	if (!Objectives || !Chapter)
	{
		return;
	}

	const TArray<FAHObjectiveDefinition> Definitions = {
		{OpeningObjective, FText::FromString(TEXT("REACH THE DEFENSIVE LINE")), FText::FromString(TEXT("Join the last human line at Erebus."))},
		{OpeningBattleObjective, FText::FromString(TEXT("HOLD THE EREBUS LINE")), FText::FromString(TEXT("Repel the first Veil assault."))},
		{TransitObjective, FText::FromString(TEXT("ENTER THE TRANSIT STATION")), FText::FromString(TEXT("Find a route beneath the battlefield."))},
		{RevelationObjective, FText::FromString(TEXT("SURVIVE THE REVELATION")), FText::FromString(TEXT("The enemy knows Lucian."))},
		{BattlefieldObjective, FText::FromString(TEXT("CROSS THE OPEN BATTLEFIELD")), FText::FromString(TEXT("Reach the Manticore route."))},
		{ManticoreObjective, FText::FromString(TEXT("ENTER THE MANTICORE")), FText::FromString(TEXT("Take the assault vehicle."))},
		{ApproachObjective, FText::FromString(TEXT("REACH THE CATHEDRAL APPROACH")), FText::FromString(TEXT("Break through to the Cathedral."))},
		{FailsafeObjective, FText::FromString(TEXT("ACTIVATE PLANETARY FAILSAFE")), FText::FromString(TEXT("The order has been given."))},
		{TerminalObjective, FText::FromString(TEXT("REACH THE FAILSAFE TERMINAL")), FText::FromString(TEXT("Enter the impossible structure."))},
		{ConfirmObjective, FText::FromString(TEXT("CONFIRM PLANETARY FAILSAFE")), FText::FromString(TEXT("Authorize the destruction of Erebus."))},
		{EscapeObjective, FText::FromString(TEXT("ESCAPE THE CATHEDRAL")), FText::FromString(TEXT("Run before the world ends."))},
		{DestructionObjective, FText::FromString(TEXT("SURVIVE THE DESTRUCTION")), FText::FromString(TEXT("Reach protected shelter."))},
		{MayaObjective, FText::FromString(TEXT("MEET CAPTAIN MAYA SOL")), FText::FromString(TEXT("Ten years later."))},
		{NysaObjective, FText::FromString(TEXT("RECEIVE THE NYSA TRANSMISSION")), FText::FromString(TEXT("A message from deep time."))},
		{FleetObjective, FText::FromString(TEXT("PREPARE FOR FLEET DEPARTURE")), FText::FromString(TEXT("The fleet is moving."))},
		{StarsObjective, FText::FromString(TEXT("WATCH THE STARS")), FText::FromString(TEXT("Something is approaching."))},
		{TitleObjective, FText::FromString(TEXT("ASHES OF HEAVEN")), FText::FromString(TEXT("Chapter One."))}
	};
	Objectives->ConfigureObjectives(Definitions, Chapter->GetState().ObjectiveIndex);
	Chapter->SetObjectiveIndex(Objectives->GetCurrentObjectiveIndex());
}

bool AAHChapterOneDirector::ValidateStageSpatialDefinition(const FAHStageSpatialDefinition& Definition, bool bLogDetails) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const bool bFinite = !Definition.SafePlayerLocation.ContainsNaN()
		&& !Definition.StageAnchor.ContainsNaN()
		&& !Definition.ObjectiveTargetLocation.ContainsNaN()
		&& !Definition.SafePlayerRotation.ContainsNaN()
		&& Definition.ExpectedBoundsMin.X <= Definition.ExpectedBoundsMax.X
		&& Definition.ExpectedBoundsMin.Y <= Definition.ExpectedBoundsMax.Y
		&& Definition.ExpectedBoundsMin.Z <= Definition.ExpectedBoundsMax.Z;
	const bool bInBounds = Definition.SafePlayerLocation.X >= Definition.ExpectedBoundsMin.X
		&& Definition.SafePlayerLocation.X <= Definition.ExpectedBoundsMax.X
		&& Definition.SafePlayerLocation.Y >= Definition.ExpectedBoundsMin.Y
		&& Definition.SafePlayerLocation.Y <= Definition.ExpectedBoundsMax.Y
		&& Definition.SafePlayerLocation.Z >= Definition.ExpectedBoundsMin.Z
		&& Definition.SafePlayerLocation.Z <= Definition.ExpectedBoundsMax.Z;
	const bool bNearAnchor = FVector::Dist2D(Definition.SafePlayerLocation, Definition.StageAnchor) <= Definition.MaxDistanceFromAnchor;
	const bool bTargetInBounds = Definition.ObjectiveTargetId == NAME_None
		|| (Definition.ObjectiveTargetLocation.X >= Definition.ExpectedBoundsMin.X
			&& Definition.ObjectiveTargetLocation.X <= Definition.ExpectedBoundsMax.X
			&& Definition.ObjectiveTargetLocation.Y >= Definition.ExpectedBoundsMin.Y
			&& Definition.ObjectiveTargetLocation.Y <= Definition.ExpectedBoundsMax.Y
			&& Definition.ObjectiveTargetLocation.Z >= Definition.ExpectedBoundsMin.Z
			&& Definition.ObjectiveTargetLocation.Z <= Definition.ExpectedBoundsMax.Z);
	bool bAnchorActorValid = false;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("AH.StageAnchor")))
			&& It->ActorHasTag(FName(*FString::Printf(TEXT("AH.Stage.%s"), *UEnum::GetValueAsString(Definition.Stage))))
			&& FVector::Dist2D(It->GetActorLocation(), Definition.StageAnchor) <= 10.0f)
		{
			bAnchorActorValid = true;
			break;
		}
	}
	FHitResult Ground;
	const bool bGroundHit = !Definition.bRequiresGround || World->LineTraceSingleByChannel(
		Ground,
		Definition.SafePlayerLocation + FVector(0.0f, 0.0f, 300.0f),
		Definition.SafePlayerLocation - FVector(0.0f, 0.0f, 1500.0f),
		ECC_Visibility);
	const bool bGroundAtExpectedHeight = !Definition.bRequiresGround || (bGroundHit && FMath::Abs(Ground.ImpactPoint.Z - Definition.GameplayFloorZ) <= 180.0f);
	AAHCombatPlayerCharacter* ExistingPlayer = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(World, 0));
	FCollisionQueryParams DefinitionParams(SCENE_QUERY_STAT(AHStageSpatialDefinition), false, ExistingPlayer);
	const bool bNoCapsulePenetration = !World->OverlapBlockingTestByChannel(
		Definition.SafePlayerLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeCapsule(34.0f, 88.0f),
		DefinitionParams);
	int32 NearbyPresentation = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("Phase4Presentation")))
			&& FVector::Dist2D(It->GetActorLocation(), Definition.SafePlayerLocation) <= 2400.0f)
		{
			++NearbyPresentation;
		}
	}
	const bool bPresentationNearby = NearbyPresentation >= 3;
	const bool bPass = bFinite && bInBounds && bNearAnchor && bTargetInBounds && bAnchorActorValid && bGroundHit && bGroundAtExpectedHeight && bNoCapsulePenetration && bPresentationNearby;
	if (bLogDetails)
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[SpatialAudit] %s %s zone=%s safe=%s anchor=%s anchorActor=%s targetBounds=%s ground=%s groundZ=%.1f expectedZ=%.1f capsule=%s presentation=%d"),
			*UEnum::GetValueAsString(Definition.Stage),
			bPass ? TEXT("PASS") : TEXT("FAIL"),
			*Definition.ZoneId.ToString(),
			*Definition.SafePlayerLocation.ToCompactString(),
			*Definition.StageAnchor.ToCompactString(),
			bAnchorActorValid ? TEXT("true") : TEXT("false"),
			bTargetInBounds ? TEXT("true") : TEXT("false"),
			bGroundHit ? TEXT("true") : TEXT("false"),
			bGroundHit ? Ground.ImpactPoint.Z : -9999.0f,
			Definition.GameplayFloorZ,
			bNoCapsulePenetration ? TEXT("clear") : TEXT("blocked"),
			NearbyPresentation);
	}
	return bPass;
}

bool AAHChapterOneDirector::ValidateStageSpatialState(EAHChapterStage Stage, bool bLogDetails) const
{
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(Stage);
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	if (!Player)
	{
		return false;
	}
	const FVector Location = Player->GetActorLocation();
	const bool bInBounds = Location.X >= Definition.ExpectedBoundsMin.X && Location.X <= Definition.ExpectedBoundsMax.X
		&& Location.Y >= Definition.ExpectedBoundsMin.Y && Location.Y <= Definition.ExpectedBoundsMax.Y
		&& Location.Z >= Definition.ExpectedBoundsMin.Z && Location.Z <= Definition.ExpectedBoundsMax.Z;
	const bool bNearAnchor = FVector::Dist2D(Location, Definition.StageAnchor) <= Definition.MaxDistanceFromAnchor;
	FHitResult Ground;
	const bool bGroundHit = GetWorld()->LineTraceSingleByChannel(Ground, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 1500.0f), ECC_Visibility);
	const bool bGroundAtExpectedHeight = bGroundHit && FMath::Abs(Ground.ImpactPoint.Z - Definition.GameplayFloorZ) <= 180.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHStageSpatialState), false, Player);
	const bool bNoCapsulePenetration = !GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.0f, 88.0f), Params);
	int32 NearbyPresentation = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(FName(TEXT("Phase4Presentation"))) && FVector::Dist2D(It->GetActorLocation(), Location) <= 2400.0f)
		{
			++NearbyPresentation;
		}
	}
	const bool bPass = bInBounds && bNearAnchor && bGroundHit && bGroundAtExpectedHeight && bNoCapsulePenetration && NearbyPresentation >= 3;
	if (bLogDetails)
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[SpatialAudit] current stage=%s %s player=%s anchor=%s distance=%.1f ground=%s groundZ=%.1f presentation=%d"),
			*UEnum::GetValueAsString(Stage), bPass ? TEXT("PASS") : TEXT("FAIL"), *Location.ToCompactString(), *Definition.StageAnchor.ToCompactString(), FVector::Dist2D(Location, Definition.StageAnchor), bGroundHit ? TEXT("true") : TEXT("false"), bGroundHit ? Ground.ImpactPoint.Z : -9999.0f, NearbyPresentation);
	}
	return bPass;
}

void AAHChapterOneDirector::EnsureStageSpatialValidity(EAHChapterStage Stage, const TCHAR* Reason)
{
	// No pawn yet means there is nothing spatial to validate — possession can lag the
	// first StartStage on a slow boot (and test worlds may run stage logic pawn-less).
	// The delayed settled-state validation re-runs once the world has settled.
	if (!UGameplayStatics::GetPlayerCharacter(GetWorld(), 0))
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Spatial] stage=%s validation deferred: no player pawn yet (%s)"), *UEnum::GetValueAsString(Stage), Reason ? Reason : TEXT("unknown"));
		return;
	}
	if (ValidateStageSpatialState(Stage, false))
	{
		return;
	}
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(Stage);
	// A recovery teleport is only for a player who is genuinely in void space. Stage
	// transitions legitimately fire while the player still stands at the previous
	// stage's exit trigger (outside the new stage's expected envelope), and yanking a
	// walking player across the map is worse than any telemetry mismatch.
	ACharacter* Player = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	FHitResult Ground;
	const bool bPlayerGrounded = Player && GetWorld()->LineTraceSingleByChannel(Ground, Player->GetActorLocation() + FVector(0.0f, 0.0f, 300.0f), Player->GetActorLocation() - FVector(0.0f, 0.0f, 1500.0f), ECC_Visibility);
	if (bPlayerGrounded)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Spatial] stage=%s reason=%s player outside expected envelope but grounded at %s; no recovery needed"), *UEnum::GetValueAsString(Stage), Reason ? Reason : TEXT("unknown"), *Player->GetActorLocation().ToCompactString());
		return;
	}
	UE_LOG(LogAshesOfHeaven, Error, TEXT("[Spatial][ERROR] invalid stage transition stage=%s reason=%s; recovering to safe spawn=%s zone=%s"), *UEnum::GetValueAsString(Stage), Reason ? Reason : TEXT("unknown"), *Definition.SafePlayerLocation.ToCompactString(), *Definition.ZoneId.ToString());
	TeleportPlayer(Definition.SafePlayerLocation, Definition.SafePlayerRotation);
	if (!ValidateStageSpatialState(Stage, true))
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Spatial][ERROR] recovery failed stage=%s"), *UEnum::GetValueAsString(Stage));
	}
}

void AAHChapterOneDirector::RunDelayedStageSpatialValidation(EAHChapterStage Stage)
{
	if (!GetWorld())
	{
		return;
	}
	GetWorldTimerManager().SetTimer(StageSpatialValidationTimer, FTimerDelegate::CreateWeakLambda(this, [this, Stage]()
	{
		if (!Chapter || Chapter->GetStage() != Stage)
		{
			return;
		}
		if (!ValidateStageSpatialState(Stage, false))
		{
			EnsureStageSpatialValidity(Stage, TEXT("WorldSettled"));
		}
		ValidateStageObjectiveConsistency(Stage);
	}), 0.25f, false);
}

void AAHChapterOneDirector::ValidateStageObjectiveConsistency(EAHChapterStage Stage) const
{
	if (!Chapter)
	{
		return;
	}
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(Stage);
	const int32 ExpectedIndex = UAHChapterSubsystem::ObjectiveIndexForStage(Stage);
	const bool bIndexMatches = ExpectedIndex == INDEX_NONE || Chapter->GetState().ObjectiveIndex == ExpectedIndex;
	// OnObjectiveCompleted broadcasts before UAHObjectiveSubsystem advances its cursor. The
	// Chapter state already contains the next canonical index at that point, so accept only
	// that one-step delegate window and let the delayed settled-state check verify the cursor.
	const bool bObjectiveMatches = Definition.ObjectiveId == NAME_None
		|| !Objectives
		|| Objectives->IsCurrentObjective(Definition.ObjectiveId)
		|| (bIndexMatches && ExpectedIndex > 0 && Objectives->GetCurrentObjectiveIndex() == ExpectedIndex - 1);
	if (!bIndexMatches || !bObjectiveMatches)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Spatial][ERROR] stage/objective mismatch stage=%s expectedObjective=%s expectedIndex=%d actualIndex=%d"), *UEnum::GetValueAsString(Stage), *Definition.ObjectiveId.ToString(), ExpectedIndex, Chapter->GetState().ObjectiveIndex);
	}
}

void AAHChapterOneDirector::StartStage(EAHChapterStage Stage)
{
	if (!Chapter)
	{
		return;
	}
	StageElapsed = 0.0f;
	DestructionFadeAlpha = 0.0f;
	#if !UE_BUILD_SHIPPING
	const AActor* LoggedPlayer = UGameplayStatics::GetPlayerCharacter(GetWorld(), 0);
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] start_stage=%s objective=%d player=%s"), *UEnum::GetValueAsString(Stage), Objectives ? Objectives->GetCurrentObjectiveIndex() : INDEX_NONE, LoggedPlayer ? *LoggedPlayer->GetActorLocation().ToCompactString() : TEXT("none"));
	#endif
	Chapter->SetStage(Stage);
	EnsureStageSpatialValidity(Stage, TEXT("StageTransition"));
	ValidateStageObjectiveConsistency(Stage);
	LogPresentationState(GetCurrentStage());

	switch (Stage)
	{
	case EAHChapterStage::OpeningBlack:
		if (!bOpeningSequenceStarted && !Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_Opening"))))
		{
			bOpeningSequenceStarted = true;
			StartDialogueSequence(FName(TEXT("Ch01_Opening")), OpeningLines());
		}
		else if (Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_Opening"))))
		{
			StartStage(EAHChapterStage::ErebusOpening);
		}
		break;
	case EAHChapterStage::OpeningBattle:
		SpawnOpeningBattle();
		break;
	case EAHChapterStage::VeilRevelation:
		if (!Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_VeilRevelation"))))
		{
			GetWorld()->SpawnActor<AAHVeilPilgrimCharacter>(AAHVeilPilgrimCharacter::StaticClass(), FVector(4700.0f, 0.0f, 110.0f), FRotator::ZeroRotator);
			StartDialogueSequence(FName(TEXT("Ch01_VeilRevelation")), RevelationLines());
		}
		break;
	case EAHChapterStage::OpenBattlefield:
		SpawnOpenBattlefieldEncounter();
		break;
	case EAHChapterStage::ManticoreSection:
		SpawnManticore();
		break;
	case EAHChapterStage::FailsafeOrder:
		Chapter->StartCountdown(522.0f);
		if (!bOrderSequenceStarted && !Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_Order"))))
		{
			bOrderSequenceStarted = true;
			StartDialogueSequence(FName(TEXT("Ch01_Order")), OrderLines());
		}
		break;
	case EAHChapterStage::CathedralInterior:
		if (!bSaelSequenceStarted && !Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_Sael"))))
		{
			bSaelSequenceStarted = true;
			StartDialogueSequence(FName(TEXT("Ch01_Sael")), SaelLines());
		}
		break;
	case EAHChapterStage::FailsafeTerminal:
		SpawnCathedralTerminal();
		break;
	case EAHChapterStage::Escape:
		SpawnEscapeEncounter();
		break;
	case EAHChapterStage::OtherLucian:
		SpawnFriendly(AHChapterSpatial::GetStageDefinition(EAHChapterStage::OtherLucian).SafePlayerLocation + FVector(0.0f, -260.0f, 0.0f), FName(TEXT("OtherLucian")));
		if (!bOtherLucianSequenceStarted && !Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_OtherLucian"))))
		{
			bOtherLucianSequenceStarted = true;
			StartDialogueSequence(FName(TEXT("Ch01_OtherLucian")), OtherLucianLines());
		}
		break;
	case EAHChapterStage::ErebusDestruction:
		Chapter->StopCountdown();
		GetWorld()->GetTimerManager().SetTimer(StageTimer, this, &AAHChapterOneDirector::FinishDestructionSequence, 7.0f, false);
		break;
	case EAHChapterStage::TenYearsLater:
		SpawnPresentDayScene();
		if (!bMayaSceneStarted && !Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_Maya"))))
		{
			bMayaSceneStarted = true;
			StartDialogueSequence(FName(TEXT("Ch01_Maya")), MayaLines());
		}
		break;
	case EAHChapterStage::MayaScene:
		if (!bNysaSequenceStarted && !Chapter->HasCompletedNarrativeEvent(FName(TEXT("Ch01_Nysa"))))
		{
			bNysaSequenceStarted = true;
			StartDialogueSequence(FName(TEXT("Ch01_Nysa")), NysaLines());
		}
		break;
	case EAHChapterStage::NysaTransmission:
		break;
	case EAHChapterStage::FleetDeparture:
		break;
	case EAHChapterStage::StarsDisappearing:
		break;
	case EAHChapterStage::ChapterComplete:
		break;
	default:
		break;
	}
	RunDelayedStageSpatialValidation(Stage);
}

void AAHChapterOneDirector::StartDialogueSequence(FName SequenceId, const TArray<FAHDialogueLine>& Lines)
{
	if (Dialogue)
	{
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] dialogue_start=%s"), *SequenceId.ToString());
		#endif
		Dialogue->StartSequence(SequenceId, Lines, true);
	}
}

void AAHChapterOneDirector::CompleteCurrentObjective()
{
	if (Objectives && !Objectives->IsMissionComplete())
	{
		Objectives->CompleteObjective(Objectives->GetCurrentObjective().Id);
	}
}

void AAHChapterOneDirector::HandleTrigger(FName TriggerId)
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] trigger=%s stage=%s"), *TriggerId.ToString(), *UEnum::GetValueAsString(GetCurrentStage()));
	#endif
	if (TriggerId == FName(TEXT("ReachDefensiveLine")) || TriggerId == FName(TEXT("ReachTransitStation")) || TriggerId == FName(TEXT("CrossBattlefield")) || TriggerId == FName(TEXT("EnterCathedral")) || TriggerId == FName(TEXT("ReachTerminal")) || TriggerId == FName(TEXT("EscapeCathedral")))
	{
		CompleteCurrentObjective();
	}
	else if (TriggerId == FName(TEXT("OtherLucian")) && GetCurrentStage() == EAHChapterStage::Escape && !bOtherLucianShown)
	{
		bOtherLucianShown = true;
		StartStage(EAHChapterStage::OtherLucian);
	}
}

void AAHChapterOneDirector::HandleObjectiveCompleted(FName ObjectiveId)
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] objective_complete=%s next_index=%d"), *ObjectiveId.ToString(), Objectives ? Objectives->GetCurrentObjectiveIndex() : INDEX_NONE);
	#endif
	if (Chapter)
	{
		// The objective delegate fires before UAHObjectiveSubsystem advances its cursor.
		// Persist the next index so stage/objective/checkpoint state cannot diverge.
		Chapter->SetObjectiveIndex(Objectives
			? FMath::Min(Objectives->GetCurrentObjectiveIndex() + 1, Objectives->GetObjectiveCount())
			: Chapter->GetState().ObjectiveIndex + 1);
	}

	if (ObjectiveId == OpeningObjective) StartStage(EAHChapterStage::OpeningBattle);
	else if (ObjectiveId == OpeningBattleObjective) StartStage(EAHChapterStage::TransitStation);
	else if (ObjectiveId == TransitObjective) StartStage(EAHChapterStage::VeilRevelation);
	else if (ObjectiveId == RevelationObjective) StartStage(EAHChapterStage::OpenBattlefield);
	else if (ObjectiveId == BattlefieldObjective) StartStage(EAHChapterStage::ManticoreSection);
	else if (ObjectiveId == ManticoreObjective) StartStage(EAHChapterStage::CathedralApproach);
	else if (ObjectiveId == ApproachObjective) StartStage(EAHChapterStage::FailsafeOrder);
	else if (ObjectiveId == FailsafeObjective) StartStage(EAHChapterStage::CathedralInterior);
	else if (ObjectiveId == TerminalObjective) StartStage(EAHChapterStage::FailsafeTerminal);
	else if (ObjectiveId == ConfirmObjective) StartStage(EAHChapterStage::Escape);
	else if (ObjectiveId == EscapeObjective) StartStage(EAHChapterStage::ErebusDestruction);
	else if (ObjectiveId == DestructionObjective) StartStage(EAHChapterStage::TenYearsLater);
	else if (ObjectiveId == MayaObjective) StartStage(EAHChapterStage::MayaScene);
	else if (ObjectiveId == NysaObjective) StartStage(EAHChapterStage::NysaTransmission);
	else if (ObjectiveId == FleetObjective) StartStage(EAHChapterStage::StarsDisappearing);
	else if (ObjectiveId == StarsObjective) StartStage(EAHChapterStage::StarsDisappearing);
	else if (ObjectiveId == TitleObjective) StartStage(EAHChapterStage::ChapterComplete);
}

void AAHChapterOneDirector::HandleDialogueComplete(FName SequenceId)
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] dialogue_complete=%s"), *SequenceId.ToString());
	#endif
	if (SequenceId == FName(TEXT("Ch01_Opening")))
	{
		StartStage(EAHChapterStage::ErebusOpening);
	}
	else if (SequenceId == FName(TEXT("Ch01_VeilRevelation")))
	{
		for (TActorIterator<AAHVeilPilgrimCharacter> It(GetWorld()); It; ++It)
		{
			if (FVector::DistSquared(It->GetActorLocation(), FVector(4700.0f, 0.0f, 110.0f)) < FMath::Square(900.0f) && !It->IsCombatantDead())
			{
				UGameplayStatics::ApplyDamage(*It, 99999.0f, nullptr, this, nullptr);
			}
		}
		CompleteCurrentObjective();
	}
	else if (SequenceId == FName(TEXT("Ch01_Sael")))
	{
		StartStage(EAHChapterStage::SaelTransmission);
	}
	else if (SequenceId == FName(TEXT("Ch01_Maya")))
	{
		CompleteCurrentObjective();
	}
	else if (SequenceId == FName(TEXT("Ch01_Nysa")))
	{
		CompleteCurrentObjective();
	}
	else if (SequenceId == FName(TEXT("Ch01_OtherLucian")))
	{
		StartStage(EAHChapterStage::Escape);
	}
}

void AAHChapterOneDirector::HandleTerminalConfirmed()
{
	if (Chapter)
	{
		Chapter->SetFailsafeConfirmed(true);
		Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_FailsafeConfirmed")));
		Chapter->StopCountdown();
	}
	CompleteCurrentObjective();
}

void AAHChapterOneDirector::HandleVehicleEntered(APawn* Driver)
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] manticore_enter driver=%s stage=%s"), *GetNameSafe(Driver), *UEnum::GetValueAsString(GetCurrentStage()));
	#endif
	if (GetCurrentStage() == EAHChapterStage::ManticoreSection)
	{
		CompleteCurrentObjective();
	}
}

void AAHChapterOneDirector::HandleVehicleExited(APawn* Driver)
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] manticore_exit driver=%s"), *GetNameSafe(Driver));
	#endif
	if (Manticore && Chapter)
	{
		Chapter->SetVehicleState(Manticore->GetVehicleState());
	}
}

void AAHChapterOneDirector::HandleVehicleDestroyed()
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase3.2][ChapterDirector] manticore_destroyed"));
	#endif
	if (Chapter)
	{
		FAHVehicleState VehicleState = Manticore ? Manticore->GetVehicleState() : FAHVehicleState();
		VehicleState.bDestroyed = true;
		Chapter->SetVehicleState(VehicleState);
	}
}

void AAHChapterOneDirector::HandleMissionComplete()
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] chapter_complete_delegate"));
	#endif
	if (Chapter)
	{
		if (Objectives)
		{
			Chapter->SetObjectiveIndex(Objectives->GetObjectiveCount());
		}
		Chapter->SetStage(EAHChapterStage::ChapterComplete);
		Chapter->MarkNarrativeEvent(FName(TEXT("Ch01_TitleReveal")));
	}
}

void AAHChapterOneDirector::FinishDestructionSequence()
{
	if (GetCurrentStage() == EAHChapterStage::ErebusDestruction)
	{
		CompleteCurrentObjective();
	}
}

void AAHChapterOneDirector::BuildGreybox()
{
	SpawnGreyboxLighting();
	// Floor has to cover every ground-level actor in the chapter, from the player start at
	// X=-1400 through the present day scene at X=30200. At scale 190 it only spanned
	// X 5000..24000, so the player spawned over open space and fell out of the level.
	SpawnBlock(FVector(14500.0f, 0.0f, -100.0f), FVector(330.0f, 40.0f, 1.0f));
	// Rear boundary behind the player spawn (X=-1400): the collision floor ends at X=-2000,
	// so without this wall the player can simply walk backwards off the world.
	SpawnBlock(FVector(-2150.0f, 0.0f, 150.0f), FVector(0.6f, 40.0f, 4.0f));
	for (int32 Index = 0; Index < 14; ++Index)
	{
		const float X = -650.0f + Index * 1150.0f;
		SpawnBlock(FVector(X, -1050.0f, 220.0f), FVector(4.0f, 0.38f, 2.2f));
		SpawnBlock(FVector(X + 480.0f, 980.0f, 260.0f), FVector(3.0f, 0.35f, 2.7f));
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float X = 2500.0f + Index * 1150.0f;
		SpawnBlock(FVector(X, -360.0f, 130.0f), FVector(2.0f, 0.55f, 1.3f));
		SpawnBlock(FVector(X + 300.0f, 420.0f, 160.0f), FVector(1.4f, 0.42f, 1.6f));
	}
	SpawnBlock(FVector(8700.0f, 0.0f, 1350.0f), FVector(2.0f, 10.0f, 13.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(11000.0f, 0.0f, 900.0f), FVector(1.4f, 8.0f, 9.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(13000.0f, 0.0f, 1900.0f), FVector(1.0f, 13.0f, 18.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(15100.0f, -1300.0f, 1000.0f), FVector(0.8f, 4.0f, 10.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(15100.0f, 1300.0f, 1000.0f), FVector(0.8f, 4.0f, 10.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(18000.0f, 0.0f, 1800.0f), FVector(0.7f, 17.0f, 16.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(21500.0f, 0.0f, 600.0f), FVector(0.8f, 13.0f, 6.0f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnBlock(FVector(26000.0f, 0.0f, 300.0f), FVector(0.5f, 14.0f, 3.0f));
	BuildCathedralSpatialRoute();
	SpawnLabel(FVector(300.0f, -1180.0f, 300.0f), TEXT("EREBUS\nTEN YEARS EARLIER"), FColor(220, 220, 220));
	SpawnLabel(FVector(3600.0f, -1180.0f, 300.0f), TEXT("TRANSIT STATION"), FColor(120, 180, 220));
	SpawnLabel(FVector(7000.0f, -1180.0f, 300.0f), TEXT("OPEN BATTLEFIELD"), FColor(220, 140, 80));
	SpawnLabel(FVector(13000.0f, -1600.0f, 2600.0f), TEXT("CATHEDRAL"), FColor(160, 120, 255));
	SpawnLabel(FVector(30000.0f, -1000.0f, 400.0f), TEXT("TEN YEARS LATER"), FColor(120, 220, 190));
	SpawnBattlefieldSimulation();
}

void AAHChapterOneDirector::BuildStageAnchors()
{
	if (!GetWorld() || !StageAnchors.IsEmpty())
	{
		return;
	}
	for (const FAHStageSpatialDefinition& Definition : AHChapterSpatial::GetStageDefinitions())
	{
		if (StageAnchors.ContainsByPredicate([&Definition](const TObjectPtr<AActor>& Existing)
		{
			return Existing && Existing->ActorHasTag(FName(*FString::Printf(TEXT("AH.Stage.%s"), *UEnum::GetValueAsString(Definition.Stage))));
		}))
		{
			continue;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* Anchor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params))
		{
			// A plain AActor has no root component, so the transform passed to SpawnActor
			// is discarded. Give the runtime anchor a real scene root before placing it.
			USceneComponent* AnchorRoot = NewObject<USceneComponent>(Anchor, TEXT("StageAnchorRoot"));
			AnchorRoot->SetMobility(EComponentMobility::Movable);
			Anchor->SetRootComponent(AnchorRoot);
			AnchorRoot->RegisterComponent();
			Anchor->SetActorLocation(Definition.StageAnchor);
			#if WITH_EDITOR
			Anchor->SetActorLabel(FString::Printf(TEXT("AHStageAnchor_%s"), *Definition.ZoneId.ToString()));
			#endif
			Anchor->Tags.Add(FName(TEXT("AH.StageAnchor")));
			Anchor->Tags.Add(FName(*FString::Printf(TEXT("AH.Stage.%s"), *UEnum::GetValueAsString(Definition.Stage))));
			Anchor->Tags.Add(FName(*FString::Printf(TEXT("AH.Zone.%s"), *Definition.ZoneId.ToString())));
			StageAnchors.Add(Anchor);
		}
	}
}

void AAHChapterOneDirector::BuildCathedralSpatialRoute()
{
	// A raised, continuous route connects the Manticore approach to the terminal and
	// escape. Every surface has one deliberate top at Z=790; the lower approach is a
	// set of real steps instead of a vertical teleport between unrelated bands.
	const FVector CathedralOrigin = AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralApproach).StageAnchor;
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const float X = CathedralOrigin.X - 500.0f + Index * 700.0f;
		SpawnBlock(FVector(X, CathedralOrigin.Y, CathedralOrigin.Z - 50.0f), FVector(7.0f, 3.0f, 1.0f), FRotator::ZeroRotator, HumanMetalMaterial);
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float X = CathedralOrigin.X - 2900.0f + Index * 240.0f;
		const float TopZ = CathedralOrigin.Z - 756.0f + Index * 84.0f;
		SpawnBlock(FVector(X, CathedralOrigin.Y, TopZ - 25.0f), FVector(1.2f, 3.0f, 0.50f), FRotator::ZeroRotator, HumanMetalMaterial);
	}
}

void AAHChapterOneDirector::BuildVisualArtTargets()
{
	if (bVisualArtTargetsBuilt)
	{
		return;
	}
	bVisualArtTargetsBuilt = true;

	// Runtime-tinted variants of the authored masters. One palette, created once; every
	// zone builder pulls from these so the whole chapter shares one material response.
	MudMaterial = MakeTintedMaterial(ConcreteMaterial, FLinearColor(0.045f, 0.042f, 0.037f), 0.62f, 0.30f, 0.65f);
	// Vertical surfaces must stay dry: wet roughness on walls mirrors the pale horizon sky
	// and reads as glowing teal panels in the war gloom.
	WallConcreteMaterial = MakeTintedMaterial(ConcreteMaterial, FLinearColor(0.052f, 0.050f, 0.046f), 0.82f, 0.0f, 0.70f);
	PuddleMaterial = MakeTintedMaterial(LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_Glass.M_Glass")), FLinearColor(0.010f, 0.012f, 0.016f), 0.04f, 1.0f);
	RuinSilhouetteMaterial = MakeTintedMaterial(VeilObsidianMaterial, FLinearColor(0.014f, 0.015f, 0.017f), 0.85f);
	BannerClothMaterial = MakeTintedMaterial(VeilObsidianMaterial, FLinearColor(0.030f, 0.030f, 0.032f), 0.95f);
	BannerEmblemMaterial = MakeTintedMaterial(HumanMetalMaterial, FLinearColor(0.42f, 0.43f, 0.40f), 0.85f, -1.0f, 0.35f, 0.08f);
	DarkStructureMaterial = MakeTintedMaterial(HumanMetalMaterial, FLinearColor(0.055f, 0.058f, 0.060f), 0.68f, 0.0f, 0.50f);

	// These are non-colliding presentation layers over the proven Phase 3 layout. The
	// gameplay blocks, triggers, checkpoints and nav data remain authoritative underneath.
	// Erebus prefers the authored streamed level (Phase 4.5 art recovery); the legacy
	// primitive construction below stays as the packaged-safety fallback only.
	bErebusAuthoredZoneActive = TryLoadAuthoredErebusZone();
	// Machine-checkable presentation mode line: visual acceptance requires Authored.
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Presentation] ErebusMode=%s"),
		bErebusAuthoredZoneActive ? TEXT("Authored") : TEXT("LegacyFallback"));
	if (!bErebusAuthoredZoneActive)
	{
		BuildErebusArtTarget();
		BuildErebusSkyline();
	}
	BuildTransitStationArtTarget();
	BuildCathedralArtTarget();
	BuildPresentDayArtTarget();
}

bool AAHChapterOneDirector::TryLoadAuthoredErebusZone()
{
	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	// The authored level is placed in anchor-local space: local origin = the canonical
	// ErebusOpening stage anchor. The spatial definition stays the coordinate authority.
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(EAHChapterStage::ErebusOpening);
	bool bRequestValid = false;
	ULevelStreamingDynamic* Streaming = ULevelStreamingDynamic::LoadLevelInstance(
		World,
		TEXT("/Game/Ashes/Environment/Erebus/L_ErebusOpening_Presentation"),
		Definition.StageAnchor,
		FRotator::ZeroRotator,
		bRequestValid);
	if (!bRequestValid || !Streaming)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.5][Presentation] authored Erebus zone request failed; using legacy primitive fallback"));
		return false;
	}

	// The rest of BeginPlay (and the spatial validation pass) expects presentation actors
	// to exist immediately, so complete the stream synchronously.
	Streaming->SetShouldBeVisible(true);
	World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

	ULevel* Loaded = Streaming->GetLoadedLevel();
	const int32 ActorCount = Loaded ? Loaded->Actors.Num() : 0;
	if (ActorCount < 20)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.5][Presentation] authored Erebus zone loaded with %d actors; using legacy primitive fallback"), ActorCount);
		return false;
	}

	// Editor-placed Niagara actors do not reliably self-activate when streamed into a
	// running game world; the legacy path activated its systems by spawning them live.
	int32 ActivatedVFX = 0;
	for (AActor* Actor : Loaded->Actors)
	{
		if (!Actor)
		{
			continue;
		}
		TArray<UNiagaraComponent*> NiagaraComponents;
		Actor->GetComponents(NiagaraComponents);
		for (UNiagaraComponent* Component : NiagaraComponents)
		{
			Component->Activate(true);
			++ActivatedVFX;
		}
	}
	PresentationVFXCount += ActivatedVFX;

	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.5][Presentation] authored Erebus zone active: %d actors, %d VFX components activated, anchor (%s)"),
		ActorCount, ActivatedVFX, *Definition.StageAnchor.ToCompactString());

	// Atmosphere/fire effects use the proven runtime spawn path: Niagara components saved
	// into the streamed level do not render in packaged builds (asset set headlessly),
	// while SpawnVisualEffect instances always have. Geometry, lights and decals stay
	// authored in the level; these effects anchor to the same authored positions.
	BuildErebusZoneEffects();
	return true;
}

void AAHChapterOneDirector::BuildErebusZoneEffects()
{
	// Every sprite system here is an authored NS_Erebus_* (Fountain-template
	// duplicate with re-authored module parameters and soft unlit sprite
	// materials). The factory NS_AshField/NS_SmokeColumn/NS_FireLarge fountains
	// rendered as giant opaque square-sprite clouds and are no longer spawned.
	// Ambient airborne ash over the corridor (SpawnVisualDust spawned the factory
	// NS_DustSheet fountain — giant opaque square sprites — and is gone for good).
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_AshAmbient.NS_Erebus_AshAmbient"), FVector(-400.0f, 0.0f, 420.0f), FVector(1.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_AshAmbient.NS_Erebus_AshAmbient"), FVector(1600.0f, 0.0f, 460.0f), FVector(1.0f));

	// Near fires, each on an authored visible source (barrel, crater, wreck, pipe).
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_FireSmall.NS_Erebus_FireSmall"), FVector(-960.0f, -210.0f, 40.0f), FVector(1.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"), FVector(-960.0f, -210.0f, 60.0f), FVector(1.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_FireSmall.NS_Erebus_FireSmall"), FVector(-350.0f, -620.0f, -40.0f), FVector(1.3f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"), FVector(-350.0f, -620.0f, -30.0f), FVector(1.1f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeLocal.NS_Erebus_SmokeLocal"), FVector(-350.0f, -620.0f, -20.0f), FVector(1.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_FireWreck.NS_Erebus_FireWreck"), FVector(1500.0f, 560.0f, -30.0f), FVector(1.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_EmbersNear.NS_Erebus_EmbersNear"), FVector(1500.0f, 560.0f, 10.0f), FVector(1.2f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeLocal.NS_Erebus_SmokeLocal"), FVector(1500.0f, 560.0f, 0.0f), FVector(1.4f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_FireSmall.NS_Erebus_FireSmall"), FVector(2620.0f, -700.0f, 120.0f), FVector(1.6f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeLocal.NS_Erebus_SmokeLocal"), FVector(2620.0f, -700.0f, 150.0f), FVector(1.2f));

	// Battlefield smoke columns, near-to-far; fog dissolves the far ones into haze.
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeColumn.NS_Erebus_SmokeColumn"), FVector(2160.0f, 500.0f, -40.0f), FVector(0.6f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeColumn.NS_Erebus_SmokeColumn"), FVector(950.0f, -840.0f, -40.0f), FVector(0.8f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeColumn.NS_Erebus_SmokeColumn"), FVector(6100.0f, -1900.0f, -40.0f), FVector(1.6f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeColumn.NS_Erebus_SmokeColumn"), FVector(8400.0f, 2300.0f, -40.0f), FVector(2.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeColumn.NS_Erebus_SmokeColumn"), FVector(11200.0f, -1500.0f, -40.0f), FVector(1.8f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeColumn.NS_Erebus_SmokeColumn"), FVector(12900.0f, 900.0f, -40.0f), FVector(2.2f));

	// Distant fire glows under the far columns (large authored wreck fire, scaled).
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_FireWreck.NS_Erebus_FireWreck"), FVector(6100.0f, -1900.0f, -46.0f), FVector(3.4f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_FireWreck.NS_Erebus_FireWreck"), FVector(8400.0f, 2300.0f, -46.0f), FVector(3.8f));
}

void AAHChapterOneDirector::BuildErebusArtTarget()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const TCHAR* Cylinder = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder");

	// The authored prop Blueprints are the normal gameplay presentation layer. The
	// primitive shapes below remain modular framing pieces, not the whole scene.
	//
	// Phase 4.4.2: the visible ground must cover the same space as the gameplay collision
	// floor for the whole Objective 01 corridor. The old slab began at X=2000 while the
	// player spawns at X=-1400, which put the fresh spawn (and the defensive-line trigger
	// at X=-600) over invisible collision inside a gray void. The slab now spans
	// X [-2400, 8400] with its top flush against the collision floor top at Z=-50.
	SpawnVisualShape(Cube, FVector(3000.0f, 0.0f, -61.0f), FVector(108.0f, 26.0f, 0.22f), FRotator::ZeroRotator, MudMaterial);

	// Rear boundary of the trench reads as a wall, not the edge of the universe. Its
	// collision twin is spawned in BuildGreybox at the identical transform.
	SpawnVisualShape(Cube, FVector(-2150.0f, 0.0f, 150.0f), FVector(0.6f, 26.0f, 4.0f), FRotator::ZeroRotator, WallConcreteMaterial);

	// Visible twins of the first greybox trench-wall pairs (exact collision transforms from
	// BuildGreybox), so the spawn strip is enclosed by the same walls the player collides with.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float X = -650.0f + Index * 1150.0f;
		SpawnVisualShape(Cube, FVector(X, -1050.0f, 220.0f), FVector(4.0f, 0.38f, 2.2f), FRotator::ZeroRotator, Index % 2 == 0 ? WallConcreteMaterial : DarkStructureMaterial);
		SpawnVisualShape(Cube, FVector(X + 480.0f, 980.0f, 260.0f), FVector(3.0f, 0.35f, 2.7f), FRotator::ZeroRotator, Index % 2 == 0 ? DarkStructureMaterial : WallConcreteMaterial);
	}

	// --- Ground read: mud ruts, puddles and settled rubble so the floor is a battlefield
	// surface, not a clean slab. Deterministic scatter keeps runs comparable.
	SpawnVisualShape(Cube, FVector(600.0f, -95.0f, -49.0f), FVector(24.0f, 0.34f, 0.02f), FRotator(0.0f, 1.5f, 0.0f), RuinSilhouetteMaterial);
	SpawnVisualShape(Cube, FVector(600.0f, 95.0f, -49.0f), FVector(24.0f, 0.34f, 0.02f), FRotator(0.0f, -1.0f, 0.0f), RuinSilhouetteMaterial);
	SpawnPuddle(FVector(-760.0f, -330.0f, 0.0f), FVector2D(1.9f, 1.2f), -35.0f);
	SpawnPuddle(FVector(-320.0f, 220.0f, 0.0f), FVector2D(2.2f, 1.5f), 60.0f);
	SpawnPuddle(FVector(340.0f, -110.0f, 0.0f), FVector2D(3.1f, 1.8f), 8.0f);
	SpawnPuddle(FVector(1240.0f, 420.0f, 0.0f), FVector2D(2.4f, 1.4f), -18.0f);
	SpawnPuddle(FVector(2300.0f, -260.0f, 0.0f), FVector2D(2.8f, 1.6f), 42.0f);
	SpawnRubblePatch(FVector(-1200.0f, -450.0f, 0.0f), 260.0f, 12, 11);
	SpawnRubblePatch(FVector(-880.0f, 380.0f, 0.0f), 240.0f, 10, 23);
	SpawnRubblePatch(FVector(-480.0f, -140.0f, 0.0f), 200.0f, 9, 37);
	SpawnRubblePatch(FVector(150.0f, 260.0f, 0.0f), 300.0f, 14, 51);
	SpawnRubblePatch(FVector(1050.0f, -420.0f, 0.0f), 320.0f, 14, 67);
	SpawnRubblePatch(FVector(2050.0f, 180.0f, 0.0f), 340.0f, 16, 83);
	SpawnRubblePatch(FVector(3100.0f, -350.0f, 0.0f), 320.0f, 12, 97);

	// --- Foreground framing at the spawn: fallen beam, burning barrel and flank wreckage
	// give the first frame occupied edges like the reference's trench mouth.
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Wreck.BP_Erebus_Wreck_C"), FVector(-1050.0f, -650.0f, 20.0f), FRotator(0.0f, 25.0f, 0.0f), FVector(1.2f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_PipeCluster.BP_Erebus_PipeCluster_C"), FVector(-900.0f, 700.0f, 49.0f), FRotator(0.0f, -15.0f, 0.0f), FVector(1.1f));
	SpawnVisualShape(Cylinder, FVector(-1120.0f, -280.0f, -34.0f), FVector(0.15f, 0.15f, 5.6f), FRotator(88.0f, 24.0f, 0.0f), DarkStructureMaterial);
	SpawnVisualShape(Cylinder, FVector(-990.0f, -240.0f, -20.0f), FVector(0.35f, 0.35f, 0.85f), FRotator(4.0f, 0.0f, 6.0f), RuinSilhouetteMaterial);
	// ponytail: no near-player fire sprites — the factory-default plume reads as a glowing
	// slab, not fire; the orange point light + embers + smoke carry the burning-barrel read.
	SpawnVisualLight(FVector(-990.0f, -240.0f, 120.0f), FLinearColor(1.0f, 0.42f, 0.12f), 1400.0f, 620.0f);
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_EmberDrift.NS_EmberDrift"), FVector(-1100.0f, 200.0f, 250.0f), FVector(0.4f));

	// The defensive line itself sits at the Objective 01 trigger (X=-600): flanking
	// barricades and a staggered sandbag row with an open lane, a practical light and a
	// small fire so the objective target is readable from the spawn.
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Barricade.BP_Erebus_Barricade_C"), FVector(-620.0f, -560.0f, -17.0f), FRotator(0.0f, -6.0f, 0.0f), FVector(1.2f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Barricade.BP_Erebus_Barricade_C"), FVector(-580.0f, 560.0f, -18.0f), FRotator(0.0f, 8.0f, 0.0f), FVector(1.15f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Human_ExpeditionLight.BP_Human_ExpeditionLight_C"), FVector(-520.0f, -140.0f, 10.0f), FRotator::ZeroRotator, FVector(1.0f));
	SpawnVisualShape(Cube, FVector(-600.0f, -250.0f, -15.0f), FVector(0.5f, 2.2f, 0.7f), FRotator(0.0f, 4.0f, 0.0f), WallConcreteMaterial);
	SpawnVisualShape(Cube, FVector(-600.0f, 260.0f, -15.0f), FVector(0.5f, 2.2f, 0.7f), FRotator(0.0f, -5.0f, 0.0f), WallConcreteMaterial);
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const float Y = -430.0f + Index * 60.0f + (Index >= 3 ? 500.0f : 0.0f);
		SpawnVisualShape(Cube, FVector(-640.0f + (Index % 2) * 26.0f, Y, -39.0f), FVector(0.42f, 0.26f, 0.22f), FRotator(0.0f, Index * 9.0f - 20.0f, 0.0f), MudMaterial);
		SpawnVisualShape(Cube, FVector(-635.0f + (Index % 2) * 20.0f, Y + 28.0f, -20.0f), FVector(0.38f, 0.24f, 0.20f), FRotator(0.0f, Index * -7.0f + 12.0f, 0.0f), DarkStructureMaterial);
	}
	SpawnVisualLight(FVector(-540.0f, -140.0f, 260.0f), FLinearColor(1.0f, 0.45f, 0.15f), 1200.0f, 700.0f);

	// --- Midground props along the route.
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_BlastWall.BP_Erebus_BlastWall_C"), FVector(800.0f, -620.0f, 112.0f), FRotator(0.0f, 8.0f, 0.0f), FVector(1.8f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Barricade.BP_Erebus_Barricade_C"), FVector(1500.0f, 460.0f, -16.0f), FRotator(0.0f, -5.0f, 0.0f), FVector(1.25f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_PipeCluster.BP_Erebus_PipeCluster_C"), FVector(2350.0f, 650.0f, 60.0f), FRotator(0.0f, 18.0f, 0.0f), FVector(1.35f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Wreck.BP_Erebus_Wreck_C"), FVector(3020.0f, -420.0f, 25.0f), FRotator(0.0f, -20.0f, -4.0f), FVector(1.4f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Human_ExpeditionLight.BP_Human_ExpeditionLight_C"), FVector(4100.0f, 540.0f, 10.0f), FRotator::ZeroRotator, FVector(1.0f));

	// Foreground: recoverable human fortification pieces with visible industrial mass.
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float X = 250.0f + Index * 520.0f;
		SpawnVisualShape(Cube, FVector(X, -540.0f, -20.0f), FVector(1.5f, 0.9f, 0.55f), FRotator(0.0f, Index % 2 == 0 ? 4.0f : -4.0f, 0.0f), DarkStructureMaterial);
		SpawnVisualShape(Cube, FVector(X + 160.0f, -520.0f, 70.0f), FVector(0.12f, 0.12f, 2.2f), FRotator::ZeroRotator, DarkStructureMaterial);
	}

	// Midground: a damaged defensive wall, logistics wreck and a readable fire source.
	SpawnVisualShape(Cube, FVector(1850.0f, 520.0f, 80.0f), FVector(5.5f, 0.28f, 2.6f), FRotator(0.0f, 0.0f, -8.0f), WallConcreteMaterial);
	SpawnVisualShape(Cube, FVector(2120.0f, 520.0f, 250.0f), FVector(1.1f, 0.42f, 0.22f), FRotator(0.0f, 0.0f, 18.0f), DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(1220.0f, 300.0f, -20.0f), FVector(2.2f, 1.0f, 0.3f), FRotator(0.0f, -12.0f, 0.0f), DarkStructureMaterial);
	SpawnVisualShape(Cylinder, FVector(1220.0f, 300.0f, 60.0f), FVector(0.75f, 0.75f, 1.0f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cylinder, FVector(1550.0f, 700.0f, -36.0f), FVector(0.18f, 0.18f, 7.5f), FRotator(0.0f, 90.0f, 0.0f), DarkStructureMaterial);
	SpawnVisualShape(Cylinder, FVector(2500.0f, 700.0f, -36.0f), FVector(0.14f, 0.14f, 9.0f), FRotator(0.0f, 90.0f, 0.0f), DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(2440.0f, 680.0f, 40.0f), FVector(1.7f, 0.55f, 0.16f), FRotator(0.0f, 0.0f, -13.0f), DarkStructureMaterial);
	SpawnVisualLight(FVector(1180.0f, 260.0f, 100.0f), FLinearColor(1.0f, 0.23f, 0.06f), 1800.0f, 850.0f);
	SpawnVisualLight(FVector(2160.0f, 500.0f, 300.0f), FLinearColor(1.0f, 0.48f, 0.12f), 900.0f, 500.0f);
	SpawnVisualLight(FVector(4100.0f, 1220.0f, 400.0f), FLinearColor(1.0f, 0.18f, 0.05f), 500.0f, 720.0f);

	// --- Banner monoliths: the reference's dominant midground vocabulary — elevated
	// near-black bunker masses carrying pale military emblems on hanging cloth.
	SpawnBannerMonolith(FVector(950.0f, -880.0f, -50.0f), FVector(6.0f, 4.0f, 5.0f), 6.0f);
	SpawnBannerMonolith(FVector(2150.0f, 820.0f, -50.0f), FVector(5.0f, 3.5f, 6.5f), -8.0f);
	SpawnBannerMonolith(FVector(3350.0f, -820.0f, -50.0f), FVector(7.0f, 4.0f, 7.0f), 4.0f);
	SpawnBannerMonolith(FVector(4600.0f, 900.0f, -50.0f), FVector(6.5f, 4.5f, 8.5f), -5.0f);

	// Right-flank gantry tower with hanging chains, echoing the reference's crane rigging.
	SpawnVisualShape(Cube, FVector(1650.0f, 1080.0f, 500.0f), FVector(3.0f, 3.0f, 11.0f), FRotator(0.0f, 4.0f, 0.0f), RuinSilhouetteMaterial);
	SpawnVisualShape(Cube, FVector(1650.0f, 880.0f, 1010.0f), FVector(0.4f, 5.0f, 0.4f), FRotator(0.0f, 0.0f, -4.0f), DarkStructureMaterial);
	SpawnHangingChain(FVector(1650.0f, 660.0f, 990.0f), FVector(1760.0f, 560.0f, 60.0f));
	SpawnHangingChain(FVector(1650.0f, 760.0f, 1000.0f), FVector(1560.0f, 700.0f, 220.0f));

	// Background: layered ruin forms frame the route and keep the Cathedral silhouette legible.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float X = 4300.0f + Index * 900.0f;
		SpawnVisualShape(Cube, FVector(X, 1500.0f, 650.0f + (Index % 2) * 180.0f), FVector(1.8f, 0.25f, 6.5f + (Index % 2) * 1.5f), FRotator(0.0f, 0.0f, Index % 2 == 0 ? 3.0f : -4.0f), RuinSilhouetteMaterial);
	}
	SpawnVisualShape(Cube, FVector(10300.0f, 1550.0f, 850.0f), FVector(1.0f, 0.3f, 8.5f), FRotator(0.0f, 0.0f, -3.0f), RuinSilhouetteMaterial);
	SpawnVisualShape(Cube, FVector(12600.0f, 1500.0f, 1250.0f), FVector(0.7f, 0.3f, 12.0f), FRotator(0.0f, 0.0f, 2.0f), RuinSilhouetteMaterial);

	// ponytail: no dust-sheet sprites over the Erebus lane — the opaque grime sheet reads
	// as a pale monolith, not haze. AshField particles + fog carry the airborne read.
	// ponytail: ash-field sprites cluster into a pale column over the lane (same failure
	// as the dust sheets). Height fog + ember drift carry the airborne ash read.
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_EmberDrift.NS_EmberDrift"), FVector(1250.0f, -260.0f, 210.0f), FVector(0.4f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(2160.0f, 500.0f, -40.0f), FVector(1.2f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(950.0f, -840.0f, -40.0f), FVector(1.6f));
	SpawnVisualLight(FVector(950.0f, -780.0f, 160.0f), FLinearColor(1.0f, 0.34f, 0.09f), 2200.0f, 1100.0f);
	SpawnLabel(FVector(700.0f, -1180.0f, 430.0f), TEXT("EREBUS / DEFENSIVE LINE"), FColor(180, 194, 202), 105.0f);
}

void AAHChapterOneDirector::BuildTransitStationArtTarget()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const TCHAR* Cylinder = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder");
	const TCHAR* DoorFrame = Cube;

	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Transit_Sign.BP_Transit_Sign_C"), FVector(3500.0f, -760.0f, 190.0f), FRotator::ZeroRotator, FVector(1.3f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Transit_Bench.BP_Transit_Bench_C"), FVector(3250.0f, -380.0f, 120.0f), FRotator(0.0f, 90.0f, 0.0f), FVector(1.0f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Transit_Bench.BP_Transit_Bench_C"), FVector(3780.0f, 380.0f, 120.0f), FRotator(0.0f, -90.0f, 0.0f), FVector(1.0f));

	// Platforms and rails form a recognizable transit corridor without changing gameplay collision.
	SpawnVisualShape(Cube, FVector(3500.0f, 0.0f, -30.0f), FVector(16.0f, 9.0f, 0.10f), FRotator::ZeroRotator, WallConcreteMaterial);
	for (const float RailY : {-360.0f, 360.0f})
	{
		SpawnVisualShape(Cylinder, FVector(3500.0f, RailY, 18.0f), FVector(0.10f, 0.10f, 28.0f), FRotator(90.0f, 0.0f, 0.0f), DarkStructureMaterial);
	}
	SpawnVisualShape(Cube, FVector(3500.0f, -720.0f, 95.0f), FVector(16.0f, 0.08f, 0.95f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(3500.0f, 720.0f, 95.0f), FVector(16.0f, 0.08f, 0.95f), FRotator::ZeroRotator, DarkStructureMaterial);

	// The station entrance is a strong frame on the route, with a practical overhead sign.
	for (const float PillarY : {-680.0f, 680.0f})
	{
		SpawnVisualShape(DoorFrame, FVector(3500.0f, PillarY, 370.0f), FVector(2.4f, 2.4f, 5.2f), FRotator::ZeroRotator, DarkStructureMaterial);
		SpawnVisualShape(Cube, FVector(3500.0f, PillarY, 740.0f), FVector(0.45f, 0.45f, 3.3f), FRotator::ZeroRotator, DarkStructureMaterial);
	}
	SpawnVisualShape(Cube, FVector(3500.0f, 0.0f, 760.0f), FVector(0.45f, 7.4f, 0.40f), FRotator::ZeroRotator, DarkStructureMaterial);
	// Centered gate block under the lintel: from the Erebus spawn axis the bare thin
	// lintel plus any nearer vertical silhouette composited into an accidental cross
	// (visual gate #18). A solid station header reads as architecture instead.
	SpawnVisualShape(Cube, FVector(3500.0f, 0.0f, 640.0f), FVector(0.50f, 3.6f, 1.30f), FRotator::ZeroRotator, DarkStructureMaterial);
	// A lit metal sign panel, not an emissive slab: from Objective 01 the emissive version
	// reads as a glowing beige monolith on the horizon (Phase 4.4.2 capture finding); the
	// pale emblem tint still glowed under the practical light, so the panel goes dark steel.
	UMaterialInterface* SignPanelMaterial = MakeTintedMaterial(HumanMetalMaterial, FLinearColor(0.10f, 0.10f, 0.095f), 0.7f, 0.0f, 0.45f);
	SpawnVisualShape(Cube, FVector(3500.0f, -735.0f, 760.0f), FVector(0.08f, 1.9f, 0.72f), FRotator::ZeroRotator, SignPanelMaterial);
	SpawnLabel(FVector(3500.0f, -820.0f, 770.0f), TEXT("TRANSIT\nSTATION"), FColor(224, 224, 210), 125.0f);
	SpawnLabel(FVector(3500.0f, -760.0f, 930.0f), TEXT("NORTH LINE  /  PLATFORM 02"), FColor(232, 190, 118), 72.0f);
	SpawnLabel(FVector(3500.0f, 790.0f, 710.0f), TEXT("CIVIL DEFENSE\nEVACUATION ROUTE"), FColor(222, 90, 62), 66.0f, FRotator(0.0f, -90.0f, 0.0f));

	// Civilian traces: a few abandoned cases and a control desk, not a prop carpet.
	SpawnVisualShape(Cube, FVector(3150.0f, -410.0f, 60.0f), FVector(0.45f, 0.30f, 0.28f), FRotator(0.0f, 18.0f, 0.0f), WallConcreteMaterial);
	SpawnVisualShape(Cube, FVector(3270.0f, -450.0f, 52.0f), FVector(0.32f, 0.22f, 0.20f), FRotator(0.0f, -12.0f, 0.0f), WallConcreteMaterial);
	SpawnVisualShape(Cube, FVector(3440.0f, -250.0f, 85.0f), FVector(1.8f, 0.24f, 0.12f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(3440.0f, -250.0f, 160.0f), FVector(1.65f, 0.12f, 0.58f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(3630.0f, 270.0f, 80.0f), FVector(1.3f, 0.22f, 0.10f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(3630.0f, 270.0f, 145.0f), FVector(1.15f, 0.10f, 0.45f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(3780.0f, 380.0f, 160.0f), FVector(1.3f, 0.65f, 0.72f), FRotator::ZeroRotator, DarkStructureMaterial);
	SpawnVisualShape(Cube, FVector(3780.0f, 380.0f, 305.0f), FVector(1.1f, 0.58f, 0.08f), FRotator::ZeroRotator, BannerEmblemMaterial);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SpawnVisualShape(Cylinder, FVector(3500.0f + Index * 480.0f, 0.0f, 1080.0f), FVector(0.08f, 0.08f, 4.5f), FRotator(0.0f, 90.0f, 0.0f), DarkStructureMaterial);
	}

	SpawnVisualLight(FVector(3300.0f, -500.0f, 540.0f), FLinearColor(1.0f, 0.48f, 0.16f), 620.0f, 700.0f);
	SpawnVisualLight(FVector(3820.0f, 500.0f, 460.0f), FLinearColor(0.95f, 0.08f, 0.03f), 360.0f, 520.0f);
	// Authored soft smoke instead of the factory dust fountains (square sprites).
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_Erebus_SmokeLocal.NS_Erebus_SmokeLocal"), FVector(3500.0f, 0.0f, 540.0f), FVector(1.1f));
}

void AAHChapterOneDirector::BuildCathedralArtTarget()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const TCHAR* Cylinder = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder");
	const FVector CathedralOrigin = AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralApproach).StageAnchor;
	const auto Local = [&CathedralOrigin](const FVector& Offset)
	{
		return CathedralOrigin + Offset;
	};

	// The Cathedral route is authored at the same raised elevation as its collision
	// twin. The old base slab at Z=-50 left the player walking on an invisible floor
	// while the human walkway and triggers floated around Z=790-850.
	for (int32 Index = 0; Index < 16; ++Index)
	{
		const float X = CathedralOrigin.X - 500.0f + Index * 700.0f;
		SpawnVisualShape(Cube, Local(FVector(X - CathedralOrigin.X, 0.0f, -5.0f)), FVector(7.0f, 3.0f, 0.10f), FRotator::ZeroRotator, HumanMetalMaterial);
		SpawnVisualShape(Cube, Local(FVector(X - CathedralOrigin.X, -285.0f, 145.0f)), FVector(7.0f, 0.06f, 1.5f), FRotator::ZeroRotator, HumanMetalMaterial);
		SpawnVisualShape(Cube, Local(FVector(X - CathedralOrigin.X, 285.0f, 145.0f)), FVector(7.0f, 0.06f, 1.5f), FRotator::ZeroRotator, HumanMetalMaterial);
	}
	for (int32 Index = 0; Index < 10; ++Index)
	{
		const float X = CathedralOrigin.X - 2900.0f + Index * 240.0f;
		const float TopZ = CathedralOrigin.Z - 756.0f + Index * 84.0f;
		SpawnVisualShape(Cube, Local(FVector(X - CathedralOrigin.X, 0.0f, TopZ - 25.0f - CathedralOrigin.Z)), FVector(1.2f, 3.0f, 0.50f), FRotator::ZeroRotator, HumanMetalMaterial);
	}
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_Fin.BP_Cathedral_Fin_C"), Local(FVector(600.0f, -700.0f, 90.0f)), FRotator(0.0f, 0.0f, -3.0f), FVector(2.8f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_Fin.BP_Cathedral_Fin_C"), Local(FVector(2300.0f, 700.0f, 210.0f)), FRotator(0.0f, 180.0f, 4.0f), FVector(2.4f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_GlyphPanel.BP_Cathedral_GlyphPanel_C"), Local(FVector(3100.0f, -250.0f, 460.0f)), FRotator(0.0f, 0.0f, 0.0f), FVector(1.4f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Human_ExpeditionLight.BP_Human_ExpeditionLight_C"), Local(FVector(1500.0f, -360.0f, 0.0f)), FRotator::ZeroRotator, FVector(0.85f));

	// A controlled vocabulary of fins, frames and voids establishes the Cathedral language.
	for (const float XOffset : {-300.0f, 600.0f, 1500.0f, 2700.0f})
	{
		SpawnVisualShape(Cube, Local(FVector(XOffset, -1050.0f, 510.0f)), FVector(0.30f, 0.25f, 13.0f), FRotator(0.0f, 0.0f, 2.0f), VeilObsidianMaterial);
		SpawnVisualShape(Cube, Local(FVector(XOffset, 1050.0f, 510.0f)), FVector(0.30f, 0.25f, 13.0f), FRotator(0.0f, 0.0f, -2.0f), VeilObsidianMaterial);
	}
	SpawnVisualShape(Cube, Local(FVector(500.0f, 0.0f, 1510.0f)), FVector(0.28f, 14.0f, 0.28f), FRotator::ZeroRotator, VeilObsidianMaterial);
	SpawnVisualShape(Cube, Local(FVector(1900.0f, 0.0f, 910.0f)), FVector(0.18f, 9.0f, 0.18f), FRotator(0.0f, 0.0f, 8.0f), CathedralMaterial);
	SpawnVisualShape(Cube, Local(FVector(3300.0f, 0.0f, 1310.0f)), FVector(0.22f, 11.0f, 0.22f), FRotator(0.0f, 0.0f, -5.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, Local(FVector(2300.0f, -700.0f, 1260.0f)), FVector(1.4f, 3.8f, 0.38f), FRotator(0.0f, 0.0f, 7.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, Local(FVector(2900.0f, 650.0f, 1760.0f)), FVector(1.0f, 2.8f, 0.32f), FRotator(0.0f, 0.0f, -6.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, Local(FVector(3600.0f, 0.0f, 710.0f)), FVector(0.20f, 7.0f, 0.20f), FRotator::ZeroRotator, VeilObsidianMaterial);

	// Familiar human walkway and expedition equipment provide scale against the void.
	SpawnVisualShape(Cube, Local(FVector(1500.0f, 0.0f, 0.0f)), FVector(16.0f, 2.4f, 0.08f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, Local(FVector(1500.0f, -245.0f, 110.0f)), FVector(16.0f, 0.05f, 0.55f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, Local(FVector(1500.0f, 245.0f, 110.0f)), FVector(16.0f, 0.05f, 0.55f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, Local(FVector(2900.0f, -280.0f, 110.0f)), FVector(1.5f, 0.85f, 0.65f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cylinder, Local(FVector(2900.0f, -280.0f, 270.0f)), FVector(0.5f, 0.5f, 1.2f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, Local(FVector(3150.0f, -280.0f, 130.0f)), FVector(0.12f, 0.12f, 1.0f), FRotator(0.0f, 0.0f, 25.0f), EmissiveTechnologyMaterial);
	SpawnCathedralGlyph(Local(FVector(1500.0f, -255.0f, 630.0f)), 150.0f, 1.0f);
	SpawnCathedralGlyph(Local(FVector(3100.0f, -255.0f, 920.0f)), 95.0f, 0.72f);
	SpawnCathedralGlyph(Local(FVector(3600.0f, -250.0f, 700.0f)), 210.0f, 1.35f);

	SpawnVisualLight(Local(FVector(1300.0f, 0.0f, 560.0f)), FLinearColor(0.42f, 0.56f, 1.0f), 700.0f, 1000.0f);
	SpawnVisualLight(Local(FVector(3400.0f, 0.0f, 210.0f)), FLinearColor(0.72f, 0.82f, 1.0f), 420.0f, 650.0f);
	SpawnVisualDust(Local(FVector(1300.0f, 0.0f, 810.0f)), 1.4f);
	SpawnVisualDust(Local(FVector(3200.0f, 260.0f, 460.0f)), 0.9f);
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_CathedralMotes.NS_CathedralMotes"), Local(FVector(2300.0f, 0.0f, 710.0f)), FVector(1.5f));
	SpawnLabel(Local(FVector(1200.0f, -1320.0f, 1910.0f)), TEXT("CATHEDRAL / INNER VOID"), FColor(190, 200, 232), 120.0f);
}

void AAHChapterOneDirector::BuildPresentDayArtTarget()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	SpawnVisualShape(Cube, FVector(30000.0f, 0.0f, -60.0f), FVector(18.0f, 10.0f, 0.08f), FRotator::ZeroRotator, ConcreteMaterial);
	SpawnVisualShape(Cube, FVector(30200.0f, -850.0f, 260.0f), FVector(5.0f, 0.08f, 2.6f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(30200.0f, 850.0f, 260.0f), FVector(5.0f, 0.08f, 2.6f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(29700.0f, 540.0f, 120.0f), FVector(0.8f, 0.65f, 0.35f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(29700.0f, 540.0f, 240.0f), FVector(0.25f, 0.55f, 0.03f), FRotator::ZeroRotator, EmissiveTechnologyMaterial);
	SpawnVisualShape(Cube, FVector(29900.0f, 0.0f, 150.0f), FVector(2.3f, 1.0f, 0.12f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(29900.0f, 0.0f, 230.0f), FVector(0.12f, 0.82f, 0.72f), FRotator::ZeroRotator, HumanMetalMaterial);

	// Existing mannequin assets are used as an honest character scaffold; final Lucian/Maya
	// meshes, hair, paint, gauntlets and facial animation remain explicit external art work.
	SpawnVisualCharacter(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"), TEXT("/Game/Characters/Mannequins/Materials/Manny/MI_Manny_02_New.MI_Manny_02_New"), FVector(29780.0f, -240.0f, 120.0f), FRotator(0.0f, 180.0f, 0.0f), 1.0f, FName(TEXT("LucianPresentDay")));
	SpawnVisualCharacter(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple.SKM_Quinn_Simple"), TEXT("/Game/Characters/Mannequins/Materials/Quinn/MI_Quinn_02.MI_Quinn_02"), FVector(30000.0f, 250.0f, 120.0f), FRotator(0.0f, 180.0f, 0.0f), 1.0f, FName(TEXT("MayaPresentDay")));
	SpawnVisualLight(FVector(29700.0f, -500.0f, 500.0f), FLinearColor(0.52f, 0.64f, 1.0f), 700.0f, 900.0f);
	SpawnVisualLight(FVector(30250.0f, 420.0f, 330.0f), FLinearColor(1.0f, 0.42f, 0.16f), 260.0f, 500.0f);
	SpawnLabel(FVector(30000.0f, -1050.0f, 520.0f), TEXT("PRESENT DAY / UNS VIGIL"), FColor(184, 202, 214), 105.0f);
}

AStaticMeshActor* AAHChapterOneDirector::SpawnVisualShape(const TCHAR* MeshPath, const FVector& Location, const FVector& Scale, const FRotator& Rotation, UMaterialInterface* MaterialOverride)
{
	if (!GetWorld() || !MeshPath)
	{
		return nullptr;
	}
	UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath);
	if (!Mesh)
	{
		++FailedMeshLoads;
		++MissingPresentationAssets;
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Presentation] mesh failed to load path=%s"), MeshPath);
		#endif
		return nullptr;
	}
	AStaticMeshActor* Shape = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Location, Scale));
	if (Shape && Shape->GetStaticMeshComponent())
	{
		UStaticMeshComponent* Component = Shape->GetStaticMeshComponent();
		// Mobility first: UStaticMeshComponent::SetStaticMesh rejects static-mobility
		// components once the world has begun play, and this helper must work no matter
		// where in the world's begin-play sequence it is called from.
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Shape->Tags.Add(FName(TEXT("Phase4Visual")));
		Shape->Tags.Add(FName(TEXT("Phase4Presentation")));
		const FName ZoneId = ResolvePresentationZone(Location);
		if (ZoneId != NAME_None)
		{
			Shape->Tags.Add(FName(*FString::Printf(TEXT("AH.Zone.%s"), *ZoneId.ToString())));
		}
		++PresentationActorCount;
		if (MaterialOverride || BlockMaterial)
		{
			Component->SetMaterial(0, MaterialOverride ? MaterialOverride : BlockMaterial.Get());
		}
	}
	return Shape;
}

AActor* AAHChapterOneDirector::SpawnPresentationProp(const TCHAR* BlueprintPath, const FVector& Location, const FRotator& Rotation, const FVector& Scale)
{
	if (!GetWorld() || !BlueprintPath)
	{
		return nullptr;
	}
	UClass* PropClass = LoadClass<AActor>(nullptr, BlueprintPath);
	if (!PropClass)
	{
		++FailedBlueprintSpawns;
		++MissingPresentationAssets;
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Presentation] prop failed to load path=%s"), BlueprintPath);
		#endif
		return nullptr;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Prop = GetWorld()->SpawnActor<AActor>(PropClass, FTransform(Rotation, Location, Scale), SpawnParams);
	if (Prop)
	{
		Prop->Tags.Add(FName(TEXT("Phase4Presentation")));
		Prop->Tags.Add(FName(TEXT("Phase4RuntimeProp")));
		const FName ZoneId = ResolvePresentationZone(Location);
		if (ZoneId != NAME_None)
		{
			Prop->Tags.Add(FName(*FString::Printf(TEXT("AH.Zone.%s"), *ZoneId.ToString())));
		}
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Prop->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (Component)
			{
				Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Component->SetCanEverAffectNavigation(false);
			}
		}
		++PresentationActorCount;
	}
	return Prop;
}

FName AAHChapterOneDirector::ResolvePresentationZone(const FVector& Location) const
{
	FName BestZone = NAME_None;
	float BestDistanceSquared = TNumericLimits<float>::Max();
	for (const FAHStageSpatialDefinition& Definition : AHChapterSpatial::GetStageDefinitions())
	{
		const bool bInsideBounds = Location.X >= Definition.ExpectedBoundsMin.X
			&& Location.X <= Definition.ExpectedBoundsMax.X
			&& Location.Y >= Definition.ExpectedBoundsMin.Y
			&& Location.Y <= Definition.ExpectedBoundsMax.Y;
		const float DistanceSquared = FVector::DistSquared2D(Location, Definition.StageAnchor);
		if (bInsideBounds && DistanceSquared < BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestZone = Definition.ZoneId;
		}
	}
	if (BestZone == NAME_None)
	{
		for (const FAHStageSpatialDefinition& Definition : AHChapterSpatial::GetStageDefinitions())
		{
			const float DistanceSquared = FVector::DistSquared2D(Location, Definition.StageAnchor);
			if (DistanceSquared < BestDistanceSquared)
			{
				BestDistanceSquared = DistanceSquared;
				BestZone = Definition.ZoneId;
			}
		}
	}
	return BestZone;
}

void AAHChapterOneDirector::SpawnVisualEffect(const TCHAR* SystemPath, const FVector& Location, const FVector& Scale)
{
	if (!GetWorld() || !SystemPath)
	{
		return;
	}
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, SystemPath);
	if (!System)
	{
		++FailedVFXLoads;
		++MissingPresentationAssets;
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Presentation] VFX failed to load path=%s"), SystemPath);
		#endif
		return;
	}
	if (UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, FRotator::ZeroRotator, Scale, true, true, ENCPoolMethod::AutoRelease))
	{
		Component->ComponentTags.Add(FName(TEXT("Phase4PresentationFX")));
		const FName ZoneId = ResolvePresentationZone(Location);
		if (ZoneId != NAME_None)
		{
			Component->ComponentTags.Add(FName(*FString::Printf(TEXT("AH.Zone.%s"), *ZoneId.ToString())));
		}
		++PresentationVFXCount;
	}
}

void AAHChapterOneDirector::SpawnVisualLight(const FVector& Location, const FLinearColor& Color, float Intensity, float Radius)
{
	if (!GetWorld())
	{
		return;
	}
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (APointLight* PointLight = GetWorld()->SpawnActor<APointLight>(APointLight::StaticClass(), Location, FRotator::ZeroRotator, SpawnParams))
	{
		PointLight->SetMobility(EComponentMobility::Movable);
		if (UPointLightComponent* Light = Cast<UPointLightComponent>(PointLight->GetLightComponent()))
		{
			Light->SetLightColor(Color);
			Light->SetIntensity(Intensity);
			Light->SetAttenuationRadius(Radius);
			Light->SetCastShadows(false);
		}
	}
}

void AAHChapterOneDirector::SpawnVisualDust(const FVector& Location, float Scale)
{
	UNiagaraSystem* DustSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/Ashes/VFX/NS_DustSheet.NS_DustSheet"));
	if (DustSystem && GetWorld())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), DustSystem, Location, FRotator::ZeroRotator, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
	}
}

void AAHChapterOneDirector::SpawnCathedralGlyph(const FVector& Location, float Radius, float Scale)
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const float SafeScale = FMath::Max(0.1f, Scale);
	// Original glyph grammar: a central axis, a crossbar, four interrupted radial
	// segments, and two short orbit marks. It is deliberately made from simple
	// primitives so the language can scale from terminals to monumental surfaces.
	SpawnVisualShape(Cube, Location, FVector(0.08f, 0.08f, 0.80f * SafeScale), FRotator::ZeroRotator, EmissiveTechnologyMaterial);
	SpawnVisualShape(Cube, Location, FVector(0.08f, 0.72f * SafeScale, 0.08f), FRotator(0.0f, 0.0f, 90.0f), EmissiveTechnologyMaterial);
	for (int32 Index = 0; Index < 8; ++Index)
	{
		const float Angle = Index * 45.0f;
		const float Radians = FMath::DegreesToRadians(Angle);
		const FVector Offset(0.0f, FMath::Sin(Radians) * Radius * SafeScale, FMath::Cos(Radians) * Radius * SafeScale);
		SpawnVisualShape(Cube, Location + Offset, FVector(0.06f, 0.34f * SafeScale, 0.06f), FRotator(0.0f, 0.0f, Angle), CathedralMaterial);
	}
}

UMaterialInterface* AAHChapterOneDirector::MakeTintedMaterial(UMaterialInterface* Parent, const FLinearColor& BaseTint, float Roughness, float Wetness, float Grime, float Metallic)
{
	if (!Parent)
	{
		return nullptr;
	}
	UMaterialInstanceDynamic* Tinted = UMaterialInstanceDynamic::Create(Parent, this);
	if (!Tinted)
	{
		return Parent;
	}
	Tinted->SetVectorParameterValue(FName(TEXT("BaseTint")), BaseTint);
	// The masters add a fresnel edge term that washes out surfaces seen at grazing angles
	// (floors especially). The battlefield read wants matte, light-absorbing surfaces.
	Tinted->SetScalarParameterValue(FName(TEXT("EdgeVariation")), 0.0f);
	if (Roughness >= 0.0f)
	{
		Tinted->SetScalarParameterValue(FName(TEXT("Roughness")), Roughness);
	}
	if (Wetness >= 0.0f)
	{
		Tinted->SetScalarParameterValue(FName(TEXT("Wetness")), Wetness);
	}
	if (Grime >= 0.0f)
	{
		Tinted->SetScalarParameterValue(FName(TEXT("GrimeAmount")), Grime);
	}
	if (Metallic >= 0.0f)
	{
		Tinted->SetScalarParameterValue(FName(TEXT("Metallic")), Metallic);
	}
	return Tinted;
}

void AAHChapterOneDirector::SpawnBannerMonolith(const FVector& BaseCenter, const FVector& Scale, float YawDegrees)
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const FRotator Yaw(0.0f, YawDegrees, 0.0f);
	const float FootingHeight = 160.0f;
	const FVector BlockCenter = BaseCenter + FVector(0.0f, 0.0f, FootingHeight + Scale.Z * 50.0f);
	// Footings lift the mass the way the reference elevates its bunker blocks.
	for (const float FootY : {-Scale.Y * 32.0f, Scale.Y * 32.0f})
	{
		SpawnVisualShape(Cube, BaseCenter + Yaw.RotateVector(FVector(0.0f, FootY, FootingHeight * 0.5f)), FVector(Scale.X * 0.55f, Scale.Y * 0.22f, FootingHeight / 100.0f), Yaw, DarkStructureMaterial);
	}
	SpawnVisualShape(Cube, BlockCenter, Scale, Yaw + FRotator(0.0f, 0.0f, 0.6f), DarkStructureMaterial);
	// Damage read: a scorched side plate and a broken parapet lip.
	SpawnVisualShape(Cube, BlockCenter + Yaw.RotateVector(FVector(Scale.X * 50.0f + 3.0f, Scale.Y * 18.0f, Scale.Z * 12.0f)), FVector(0.04f, Scale.Y * 0.34f, Scale.Z * 0.30f), Yaw, RuinSilhouetteMaterial);
	SpawnVisualShape(Cube, BlockCenter + FVector(0.0f, 0.0f, Scale.Z * 50.0f + 8.0f), FVector(Scale.X * 0.72f, Scale.Y * 0.80f, 0.16f), Yaw + FRotator(0.0f, 3.0f, 1.5f), RuinSilhouetteMaterial);
	// Banner cloth hangs on the face toward the route (-X) with a pale military emblem —
	// original Ashes mark, deliberately not a copy of the reference's accidental heraldry.
	const FVector FaceOut = Yaw.RotateVector(FVector(-Scale.X * 50.0f - 6.0f, 0.0f, 0.0f));
	const FVector BannerCenter = BlockCenter + FaceOut + FVector(0.0f, 0.0f, Scale.Z * 8.0f);
	const float BannerHeightScale = Scale.Z * 0.62f;
	const float BannerWidthScale = Scale.Y * 0.30f;
	SpawnVisualShape(Cube, BannerCenter, FVector(0.03f, BannerWidthScale, BannerHeightScale), Yaw, BannerClothMaterial);
	// Sword-blade mark: tapered blade, high guard, offset pommel, twin campaign ticks.
	// Reads military at distance without becoming a plain cross or copied heraldry.
	const FVector EmblemOut = Yaw.RotateVector(FVector(-4.0f, 0.0f, 0.0f));
	const float BarHalf = BannerHeightScale * 50.0f * 0.55f;
	SpawnVisualShape(Cube, BannerCenter + EmblemOut, FVector(0.02f, BannerWidthScale * 0.10f, BannerHeightScale * 0.50f), Yaw, BannerEmblemMaterial);
	SpawnVisualShape(Cube, BannerCenter + EmblemOut - FVector(0.0f, 0.0f, BarHalf * 1.06f), FVector(0.02f, BannerWidthScale * 0.06f, BannerHeightScale * 0.10f), Yaw + FRotator(0.0f, 0.0f, 45.0f), BannerEmblemMaterial);
	SpawnVisualShape(Cube, BannerCenter + EmblemOut + FVector(0.0f, 0.0f, BarHalf * 0.72f), FVector(0.02f, BannerWidthScale * 0.44f, BannerHeightScale * 0.055f), Yaw, BannerEmblemMaterial);
	SpawnVisualShape(Cube, BannerCenter + EmblemOut + FVector(0.0f, 0.0f, BarHalf * 1.05f), FVector(0.02f, BannerWidthScale * 0.14f, BannerHeightScale * 0.045f), Yaw, BannerEmblemMaterial);
	SpawnVisualShape(Cube, BannerCenter + EmblemOut + Yaw.RotateVector(FVector(0.0f, BannerWidthScale * 30.0f, -BarHalf * 0.35f)), FVector(0.02f, BannerWidthScale * 0.05f, BannerHeightScale * 0.16f), Yaw + FRotator(0.0f, 0.0f, -12.0f), BannerEmblemMaterial);
	SpawnVisualShape(Cube, BannerCenter + EmblemOut + Yaw.RotateVector(FVector(0.0f, -BannerWidthScale * 30.0f, -BarHalf * 0.35f)), FVector(0.02f, BannerWidthScale * 0.05f, BannerHeightScale * 0.16f), Yaw + FRotator(0.0f, 0.0f, 12.0f), BannerEmblemMaterial);
}

void AAHChapterOneDirector::SpawnRubblePatch(const FVector& Center, float Radius, int32 Count, uint32 Seed)
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	FRandomStream Stream(static_cast<int32>(Seed));
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Angle = Stream.FRandRange(0.0f, 2.0f * PI);
		const float Distance = Radius * FMath::Sqrt(Stream.FRand());
		const float Size = Stream.FRandRange(0.10f, 0.55f);
		// Embed chunks slightly so they read as settled debris, not placed boxes.
		const FVector Spot(Center.X + FMath::Cos(Angle) * Distance, Center.Y + FMath::Sin(Angle) * Distance, -50.0f + Size * 26.0f);
		const FRotator Tumble(Stream.FRandRange(-14.0f, 14.0f), Stream.FRandRange(0.0f, 360.0f), Stream.FRandRange(-16.0f, 16.0f));
		SpawnVisualShape(Cube, Spot, FVector(Size, Size * Stream.FRandRange(0.6f, 1.3f), Size * Stream.FRandRange(0.4f, 0.9f)), Tumble, (Index % 3 == 0) ? RuinSilhouetteMaterial : MudMaterial);
	}
}

void AAHChapterOneDirector::SpawnPuddle(const FVector& Center, const FVector2D& Extent, float YawDegrees)
{
	const TCHAR* Plane = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Plane.SM_AH_Plane");
	// A hair above the mud plane so the water sorts on top without z-fighting.
	SpawnVisualShape(Plane, FVector(Center.X, Center.Y, -49.2f), FVector(Extent.X, Extent.Y, 1.0f), FRotator(0.0f, YawDegrees, 0.0f), PuddleMaterial);
}

void AAHChapterOneDirector::SpawnHangingChain(const FVector& Top, const FVector& Bottom)
{
	const TCHAR* Cylinder = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder");
	const FVector Span = Bottom - Top;
	const float Length = Span.Size();
	if (Length < 1.0f)
	{
		return;
	}
	const FRotator Orientation = FRotationMatrix::MakeFromZ(Span).Rotator();
	SpawnVisualShape(Cylinder, (Top + Bottom) * 0.5f, FVector(0.07f, 0.07f, Length / 100.0f), Orientation, DarkStructureMaterial);
	// Shackle knuckles give the cable a chain read at distance.
	for (const float Alpha : {0.22f, 0.5f, 0.78f})
	{
		SpawnVisualShape(Cylinder, Top + Span * Alpha, FVector(0.13f, 0.13f, 0.22f), Orientation, DarkStructureMaterial);
	}
}

void AAHChapterOneDirector::BuildErebusSkyline()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const TCHAR* Cone = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cone.SM_AH_Cone");
	FRandomStream Stream(20260822);
	// Two depth rows of ruined city mass framing the corridor; silhouettes only, so they
	// stay outside the play band and never touch gameplay collision.
	for (int32 Index = 0; Index < 12; ++Index)
	{
		const float X = 4800.0f + Index * 720.0f;
		const float Side = (Index % 2 == 0) ? -1.0f : 1.0f;
		const float Height = Stream.FRandRange(9.0f, 24.0f);
		SpawnVisualShape(Cube, FVector(X, Side * Stream.FRandRange(1550.0f, 2100.0f), -50.0f + Height * 50.0f), FVector(Stream.FRandRange(1.6f, 3.4f), Stream.FRandRange(1.2f, 2.2f), Height), FRotator(0.0f, Stream.FRandRange(-8.0f, 8.0f), Stream.FRandRange(-2.5f, 2.5f)), RuinSilhouetteMaterial);
	}
	for (int32 Index = 0; Index < 9; ++Index)
	{
		const float X = 5400.0f + Index * 980.0f;
		const float Side = (Index % 2 == 0) ? 1.0f : -1.0f;
		const float Height = Stream.FRandRange(16.0f, 34.0f);
		SpawnVisualShape(Cube, FVector(X, Side * Stream.FRandRange(2500.0f, 3400.0f), -50.0f + Height * 50.0f), FVector(Stream.FRandRange(2.4f, 4.6f), Stream.FRandRange(1.8f, 3.2f), Height), FRotator(0.0f, Stream.FRandRange(-10.0f, 10.0f), 0.0f), RuinSilhouetteMaterial);
	}
	// Cathedral spire cluster: the destination silhouette, center-right of the route.
	const FVector SpireBase(12800.0f, 650.0f, -50.0f);
	const float SpireHeights[5] = {58.0f, 44.0f, 34.0f, 26.0f, 49.0f};
	const FVector2D SpireOffsets[5] = {FVector2D(0.0f, 0.0f), FVector2D(-450.0f, -380.0f), FVector2D(380.0f, 300.0f), FVector2D(-260.0f, 520.0f), FVector2D(520.0f, -240.0f)};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float Height = SpireHeights[Index];
		const FVector Base = SpireBase + FVector(SpireOffsets[Index].X, SpireOffsets[Index].Y, 0.0f);
		const float Width = 1.7f - Index * 0.15f;
		SpawnVisualShape(Cube, Base + FVector(0.0f, 0.0f, Height * 50.0f), FVector(Width, Width, Height), FRotator(0.0f, Index * 17.0f, 0.0f), RuinSilhouetteMaterial);
		SpawnVisualShape(Cone, Base + FVector(0.0f, 0.0f, Height * 100.0f + 170.0f), FVector(Width * 0.8f, Width * 0.8f, 3.4f), FRotator::ZeroRotator, RuinSilhouetteMaterial);
	}
	// Distant war activity: smoke columns, burning bases, low warm horizon glows.
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(6100.0f, -1900.0f, -40.0f), FVector(4.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(8400.0f, 2300.0f, -40.0f), FVector(5.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(11200.0f, -1500.0f, -40.0f), FVector(4.5f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(12900.0f, 900.0f, -40.0f), FVector(5.5f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_FireLarge.NS_FireLarge"), FVector(6100.0f, -1900.0f, -46.0f), FVector(1.6f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_FireLarge.NS_FireLarge"), FVector(8400.0f, 2300.0f, -46.0f), FVector(1.8f));
	SpawnVisualLight(FVector(6100.0f, -1900.0f, 250.0f), FLinearColor(1.0f, 0.36f, 0.10f), 2600.0f, 2600.0f);
	SpawnVisualLight(FVector(8400.0f, 2300.0f, 250.0f), FLinearColor(1.0f, 0.32f, 0.08f), 2600.0f, 2800.0f);
}

ASkeletalMeshActor* AAHChapterOneDirector::SpawnVisualCharacter(const TCHAR* MeshPath, const TCHAR* MaterialPath, const FVector& Location, const FRotator& Rotation, float Scale, FName DisplayId)
{
	if (!GetWorld() || !MeshPath)
	{
		return nullptr;
	}
	USkeletalMesh* SkeletalMesh = LoadObject<USkeletalMesh>(nullptr, MeshPath);
	UMaterialInterface* Material = MaterialPath ? LoadObject<UMaterialInterface>(nullptr, MaterialPath) : nullptr;
	if (!SkeletalMesh)
	{
		return nullptr;
	}
	ASkeletalMeshActor* Character = GetWorld()->SpawnActor<ASkeletalMeshActor>(ASkeletalMeshActor::StaticClass(), Location, Rotation);
	if (!Character || !Character->GetSkeletalMeshComponent())
	{
		return Character;
	}
	USkeletalMeshComponent* Mesh = Character->GetSkeletalMeshComponent();
	Mesh->SetSkeletalMesh(SkeletalMesh);
	Mesh->SetRelativeScale3D(FVector(Scale));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCanEverAffectNavigation(false);
	if (Material)
	{
		Mesh->SetMaterial(0, Material);
	}
	if (DisplayId != NAME_None)
	{
		Character->Tags.Add(DisplayId);
	}
	return Character;
}

void AAHChapterOneDirector::BuildMissionActors()
{
	if (bMissionActorsBuilt)
	{
		return;
	}
	bMissionActorsBuilt = true;
	const FAHStageSpatialDefinition& ErebusOpening = AHChapterSpatial::GetStageDefinition(EAHChapterStage::ErebusOpening);
	AAHChapterTrigger* Trigger = SpawnTrigger(ErebusOpening.ObjectiveTargetLocation, FVector(280.0f, 1000.0f, 160.0f), FName(TEXT("ReachDefensiveLine")), EAHChapterStage::ErebusOpening);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	const FAHStageSpatialDefinition& Transit = AHChapterSpatial::GetStageDefinition(EAHChapterStage::TransitStation);
	Trigger = SpawnTrigger(Transit.ObjectiveTargetLocation, FVector(280.0f, 900.0f, 180.0f), FName(TEXT("ReachTransitStation")), EAHChapterStage::TransitStation);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	const FAHStageSpatialDefinition& Battlefield = AHChapterSpatial::GetStageDefinition(EAHChapterStage::OpenBattlefield);
	Trigger = SpawnTrigger(Battlefield.ObjectiveTargetLocation, FVector(300.0f, 1100.0f, 180.0f), FName(TEXT("CrossBattlefield")), EAHChapterStage::OpenBattlefield);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	const FAHStageSpatialDefinition& CathedralApproach = AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralApproach);
	Trigger = SpawnTrigger(CathedralApproach.ObjectiveTargetLocation, FVector(400.0f, 650.0f, 220.0f), FName(TEXT("EnterCathedral")), EAHChapterStage::CathedralApproach);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	const FAHStageSpatialDefinition& CathedralInterior = AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralInterior);
	Trigger = SpawnTrigger(CathedralInterior.ObjectiveTargetLocation, FVector(300.0f, 650.0f, 220.0f), FName(TEXT("ReachTerminal")), EAHChapterStage::CathedralInterior);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	const FAHStageSpatialDefinition& Escape = AHChapterSpatial::GetStageDefinition(EAHChapterStage::Escape);
	Trigger = SpawnTrigger(Escape.ObjectiveTargetLocation, FVector(450.0f, 650.0f, 220.0f), FName(TEXT("EscapeCathedral")), EAHChapterStage::Escape);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	const FAHStageSpatialDefinition& OtherLucian = AHChapterSpatial::GetStageDefinition(EAHChapterStage::OtherLucian);
	Trigger = SpawnTrigger(OtherLucian.ObjectiveTargetLocation, FVector(250.0f, 650.0f, 220.0f), FName(TEXT("OtherLucian")), EAHChapterStage::Escape);
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);

	OpeningEncounter = SpawnEncounter(FName(TEXT("Ch01_OpeningBattle")), FVector(900.0f, 0.0f, 120.0f), 5, OpeningBattleObjective,
		{FVector(1200.0f, -520.0f, 120.0f), FVector(1500.0f, 520.0f, 120.0f), FVector(1900.0f, -600.0f, 120.0f), FVector(2250.0f, 620.0f, 120.0f), FVector(2600.0f, 0.0f, 120.0f)});
	BattlefieldEncounter = SpawnEncounter(FName(TEXT("Ch01_Battlefield")), FVector(7600.0f, 0.0f, 120.0f), 7, NAME_None,
		{FVector(7800.0f, -900.0f, 120.0f), FVector(8200.0f, 900.0f, 120.0f), FVector(8600.0f, -750.0f, 120.0f), FVector(9000.0f, 750.0f, 120.0f), FVector(9400.0f, -500.0f, 120.0f), FVector(9700.0f, 500.0f, 120.0f), FVector(10100.0f, 0.0f, 120.0f)});
	BattlefieldEncounter->AdditionalEnemyClasses.Add(AAHVeilWardenCharacter::StaticClass());
	const FVector EscapeOrigin = Escape.StageAnchor;
	EscapeEncounter = SpawnEncounter(FName(TEXT("Ch01_Escape")), EscapeOrigin + FVector(-500.0f, 0.0f, 100.0f), 5, NAME_None,
		{EscapeOrigin + FVector(-300.0f, -260.0f, 100.0f), EscapeOrigin + FVector(-100.0f, 260.0f, 100.0f), EscapeOrigin + FVector(400.0f, -240.0f, 100.0f), EscapeOrigin + FVector(800.0f, 240.0f, 100.0f), EscapeOrigin + FVector(1400.0f, 0.0f, 100.0f)});
	EscapeEncounter->AdditionalEnemyClasses.Add(AAHVeilWardenCharacter::StaticClass());

	for (const FAHCheckpointSpatialDefinition& Checkpoint : AHChapterSpatial::GetCheckpointDefinitions())
	{
		SpawnCheckpoint(Checkpoint.Location, Checkpoint.CheckpointId);
	}
	SpawnFriendly(FVector(4800.0f, -240.0f, 120.0f), FName(TEXT("Kell")));
	SpawnFriendly(FVector(5600.0f, 320.0f, 120.0f));
	SpawnFriendly(FVector(6800.0f, -420.0f, 120.0f));
}

void AAHChapterOneDirector::SpawnOpeningBattle()
{
	if (OpeningEncounter && !OpeningEncounter->IsComplete() && !OpeningEncounter->IsActive())
	{
		OpeningEncounter->bActivateOnPlayerOverlap = false;
		OpeningEncounter->ActivateEncounter();
	}
}

void AAHChapterOneDirector::SpawnOpenBattlefieldEncounter()
{
	if (BattlefieldEncounter && !BattlefieldEncounter->IsComplete() && !BattlefieldEncounter->IsActive())
	{
		BattlefieldEncounter->bActivateOnPlayerOverlap = false;
		BattlefieldEncounter->ActivateEncounter();
	}
}

void AAHChapterOneDirector::SpawnEscapeEncounter()
{
	if (EscapeEncounter && !EscapeEncounter->IsComplete() && !EscapeEncounter->IsActive())
	{
		EscapeEncounter->bActivateOnPlayerOverlap = false;
		EscapeEncounter->ActivateEncounter();
	}
}

void AAHChapterOneDirector::SpawnPresentDayScene()
{
	const FAHStageSpatialDefinition& PresentDay = AHChapterSpatial::GetStageDefinition(EAHChapterStage::TenYearsLater);
	const FVector Origin = PresentDay.StageAnchor;
	TeleportPlayer(PresentDay.SafePlayerLocation, PresentDay.SafePlayerRotation);
	SpawnBlock(Origin + FVector(0.0f, 0.0f, -40.0f), FVector(18.0f, 10.0f, 0.8f));
	SpawnBlock(Origin + FVector(200.0f, -850.0f, 290.0f), FVector(5.0f, 0.25f, 2.5f));
	SpawnBlock(Origin + FVector(200.0f, 850.0f, 290.0f), FVector(5.0f, 0.25f, 2.5f));
	SpawnLabel(Origin + FVector(0.0f, -1000.0f, 540.0f), TEXT("CAPTAIN MAYA SOL\nNYSA TRANSMISSION"), FColor(220, 220, 220), 105.0f);
}

void AAHChapterOneDirector::ActivateArtTargetView(FString TargetName)
{
	if (TargetName.Equals(TEXT("Erebus"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("Battlefield"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("M91"), ESearchCase::IgnoreCase))
	{
		StartStage(EAHChapterStage::ErebusOpening);
		// Reference-comparable review camera: slightly above the fresh spawn, looking down
		// the full route (defensive line, monoliths, smoke, Cathedral landmark).
		TeleportPlayer(FVector(-1380.0f, -120.0f, 210.0f), FRotator::ZeroRotator);
	}
	else if (TargetName.Equals(TEXT("Transit"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("TransitStation"), ESearchCase::IgnoreCase))
	{
		StartStage(EAHChapterStage::TransitStation);
		TeleportPlayer(FVector(3150.0f, -100.0f, 150.0f), FRotator::ZeroRotator);
	}
	else if (TargetName.Equals(TEXT("Cathedral"), ESearchCase::IgnoreCase))
	{
		StartStage(EAHChapterStage::CathedralInterior);
		TeleportPlayer(FVector(15100.0f, -100.0f, 850.0f), FRotator::ZeroRotator);
	}
	else if (TargetName.Equals(TEXT("Present"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("PresentDay"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("LucianMaya"), ESearchCase::IgnoreCase))
	{
		StartStage(EAHChapterStage::TenYearsLater);
		TeleportPlayer(FVector(29200.0f, 0.0f, 150.0f), FRotator::ZeroRotator);
	}
	else if (TargetName.Equals(TEXT("UI"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("Audio"), ESearchCase::IgnoreCase))
	{
		if (TargetName.Equals(TEXT("Audio"), ESearchCase::IgnoreCase) && GetWorld())
		{
			if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
			{
				Audio->PlayUICue(EAHAudioCue::Objective);
			}
		}
	}
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4][ArtTarget] activated=%s"), *TargetName);
	#endif
}

void AAHChapterOneDirector::SpawnCathedralTerminal()
{
	if (FailsafeTerminal)
	{
		return;
	}
	FailsafeTerminal = GetWorld()->SpawnActor<AAHChapterTerminal>(AAHChapterTerminal::StaticClass(), AHChapterSpatial::GetStageDefinition(EAHChapterStage::FailsafeTerminal).ObjectiveTargetLocation, FRotator::ZeroRotator);
	if (FailsafeTerminal)
	{
		FailsafeTerminal->TerminalMesh->SetStaticMesh(BlockMesh);
		FailsafeTerminal->TerminalMesh->SetRelativeScale3D(FVector(0.7f, 0.7f, 1.6f));
		FailsafeTerminal->TerminalMesh->SetMaterial(0, CathedralMaterial);
		FailsafeTerminal->OnConfirmed.AddDynamic(this, &AAHChapterOneDirector::HandleTerminalConfirmed);
	}
}

void AAHChapterOneDirector::SpawnManticore()
{
	if (Manticore)
	{
		return;
	}
	Manticore = GetWorld()->SpawnActor<AAHManticoreVehicle>(AAHManticoreVehicle::StaticClass(), AHChapterSpatial::GetStageDefinition(EAHChapterStage::ManticoreSection).ObjectiveTargetLocation, FRotator::ZeroRotator);
	if (Manticore)
	{
		Manticore->VehicleMesh->SetStaticMesh(BlockMesh);
		Manticore->VehicleMesh->SetMaterial(0, HumanMetalMaterial ? HumanMetalMaterial : BlockMaterial);
		if (Manticore->HullArmor)
		{
			Manticore->HullArmor->SetMaterial(0, ConcreteMaterial ? ConcreteMaterial : BlockMaterial);
		}
		if (Manticore->TurretAssembly)
		{
			Manticore->TurretAssembly->SetMaterial(0, HumanMetalMaterial ? HumanMetalMaterial : BlockMaterial);
		}
		if (Manticore->MountedWeapon)
		{
			Manticore->MountedWeapon->SetMaterial(0, EmissiveTechnologyMaterial ? EmissiveTechnologyMaterial : HumanMetalMaterial);
		}
		for (UStaticMeshComponent* Wheel : Manticore->WheelVisuals)
		{
			if (Wheel)
			{
				Wheel->SetMaterial(0, VeilObsidianMaterial ? VeilObsidianMaterial : BlockMaterial);
			}
		}
		Manticore->OnDriverEntered.AddDynamic(this, &AAHChapterOneDirector::HandleVehicleEntered);
		Manticore->OnDriverExited.AddDynamic(this, &AAHChapterOneDirector::HandleVehicleExited);
		Manticore->OnVehicleDestroyed.AddDynamic(this, &AAHChapterOneDirector::HandleVehicleDestroyed);
		if (Chapter && Chapter->GetState().Vehicle.bSpawned)
		{
			Manticore->RestoreVehicleState(Chapter->GetState().Vehicle);
		}
	}
}

void AAHChapterOneDirector::SpawnGreyboxLighting()
{
	if (!GetWorld())
	{
		return;
	}

	// The map is generated empty and every actor here is spawned at runtime, so the
	// presentation layer owns the lighting. This is deliberately authored as a low-sat,
	// smoke-heavy war profile; it is not the old neutral black-screen workaround.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADirectionalLight* SunLight = GetWorld()->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), FVector(0.0f, 0.0f, 6000.0f), FRotator(-24.0f, -18.0f, 0.0f), SpawnParams))
	{
		SunLight->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			// Low raking sun diffused by the cloud deck: cold, desaturated. Phase 4.5: raised
			// from 1.05 — packaged captures read as night while the approved target is a dark
			// overcast day; architecture must stay readable.
			SunComponent->SetIntensity(2.6f);
			SunComponent->SetLightColor(FLinearColor(0.46f, 0.52f, 0.62f));
			SunComponent->SetAtmosphereSunLight(true);
		}
	}

	if (ASkyAtmosphere* SkyAtmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		if (USkyAtmosphereComponent* Atmosphere = SkyAtmosphere->GetComponent())
		{
			// Overcast gray war sky, not a black void: packaged captures with heavy Mie
			// absorption rendered the deck near-black with a visible sun disk. Scattering
			// up, absorption down, multiscatter up = diffuse gray dome the clouds sit in.
			Atmosphere->GroundAlbedo = FColor(34, 36, 39);
			Atmosphere->RayleighScattering = FLinearColor(0.042f, 0.048f, 0.060f);
			Atmosphere->MieScattering = FLinearColor(0.085f, 0.088f, 0.094f);
			Atmosphere->MieAbsorption = FLinearColor(0.012f, 0.013f, 0.014f);
			Atmosphere->MieAnisotropy = 0.55f;
			Atmosphere->MultiScatteringFactor = 1.0f;
			Atmosphere->MarkRenderStateDirty();
		}
	}

	// Volumetric cloud deck supplies the dark war-sky mass; the engine default cloud
	// material is cooked via DefaultGame.ini (DirectoriesToAlwaysCook=/Engine/EngineSky).
	if (AVolumetricCloud* Clouds = GetWorld()->SpawnActor<AVolumetricCloud>(AVolumetricCloud::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		if (UVolumetricCloudComponent* CloudComponent = Clouds->FindComponentByClass<UVolumetricCloudComponent>())
		{
			CloudComponent->SetLayerBottomAltitude(1.4f);
			CloudComponent->SetLayerHeight(6.0f);
		}
	}

	if (ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(), FVector(0.0f, 0.0f, 4000.0f), FRotator::ZeroRotator, SpawnParams))
	{
		if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			// Real-time capture sources ambient from the atmosphere; a static capture of an
			// unlit scene would just bake black.
			SkyComponent->SetRealTimeCapture(true);
			SkyComponent->SetIntensity(3.2f);
			// Grey uplight instead of a second directional: lifts vertical architecture
			// without competing for the single forward-shading directional slot (the
			// competition warning renders on screen in Development builds).
			SkyComponent->SetLowerHemisphereColor(FLinearColor(0.28f, 0.30f, 0.34f));
			if (USkyLightComponent* LowerHemi = SkyComponent)
			{
				LowerHemi->bLowerHemisphereIsBlack = false;
			}
		}
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetMobility(EComponentMobility::Movable);
			// Deeper, brighter haze: distance dissolves into gray smoke instead of black,
			// which is what layers the reference's midground/background depth.
			FogComponent->SetFogDensity(0.028f);
			FogComponent->SetFogHeightFalloff(0.10f);
			FogComponent->SetFogInscatteringColor(FLinearColor(0.058f, 0.064f, 0.078f));
			FogComponent->SetStartDistance(260.0f);
			// Distant landmarks must ghost through the smoke instead of vanishing: the
			// Cathedral silhouette is the route's destination read.
			FogComponent->SetFogMaxOpacity(0.86f);
			// Volumetric fog carries the fire glow and sun shafts through the smoke.
			FogComponent->SetVolumetricFog(true);
		}
	}

	// One unbound post volume unifies grade across the chapter: restrained saturation,
	// slight contrast, low bloom so emissives stay embers rather than neon.
	if (APostProcessVolume* Post = GetWorld()->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		Post->bUnbound = true;
		Post->BlendWeight = 1.0f;
		Post->Settings.bOverride_ColorSaturation = true;
		Post->Settings.ColorSaturation = FVector4(0.88f, 0.90f, 0.96f, 1.0f);
		Post->Settings.bOverride_ColorContrast = true;
		Post->Settings.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
		Post->Settings.bOverride_BloomIntensity = true;
		Post->Settings.BloomIntensity = 0.42f;
		Post->Settings.bOverride_VignetteIntensity = true;
		Post->Settings.VignetteIntensity = 0.38f;
		// Clamp auto-exposure so the war gloom neither crushes structures to black nor
		// blows the fire accents out; the approved target holds a stable dark-overcast key.
		// A high min brightness LOCKS dark scenes dark (adaptation clamps at the min), so
		// the floor stays low and a positive bias lifts the whole key instead.
		Post->Settings.bOverride_AutoExposureMinBrightness = true;
		Post->Settings.AutoExposureMinBrightness = 0.05f;
		Post->Settings.bOverride_AutoExposureMaxBrightness = true;
		Post->Settings.AutoExposureMaxBrightness = 0.8f;
		Post->Settings.bOverride_AutoExposureBias = true;
		Post->Settings.AutoExposureBias = 0.25f;
	}

	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.5][Presentation] lighting profile=ErebusWar clouds=volumetric fog=volumetric post=graded"));
}

void AAHChapterOneDirector::SpawnBlock(const FVector& Location, const FVector& Scale, const FRotator& Rotation, UMaterialInterface* MaterialOverride)
{
	if (!GetWorld())
	{
		return;
	}
	if (!BlockMesh)
	{
		// This actor IS the gameplay collision layer; a silent no-op here deletes the floor
		// under the player. Fall back to the engine cube and log loudly.
		BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4.2][Greybox] SM_AH_Cube failed to load; collision layer falling back to engine cube (%s)"), BlockMesh ? TEXT("ok") : TEXT("unavailable"));
		if (!BlockMesh)
		{
			return;
		}
	}
	AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Location, Scale));
	if (Block && Block->GetStaticMeshComponent())
	{
		// Mobility first — see SpawnVisualShape.
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Block->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);
		// This actor is the gameplay collision layer. Its prototype visual is disabled in
		// normal play; Phase 4 presentation actors provide the visible ground and cover.
		Block->GetStaticMeshComponent()->SetVisibility(false);
		Block->Tags.Add(FName(TEXT("Phase4GreyboxCollision")));
		if (MaterialOverride || BlockMaterial)
		{
			UMaterialInterface* Material = MaterialOverride ? MaterialOverride : BlockMaterial.Get();
			Block->GetStaticMeshComponent()->SetMaterial(0, Material);
		}
	}
}

void AAHChapterOneDirector::SpawnCheckpoint(const FVector& Location, FName Id)
{
	AAHCheckpointActor* Checkpoint = GetWorld()->SpawnActor<AAHCheckpointActor>(AAHCheckpointActor::StaticClass(), Location, FRotator::ZeroRotator);
	if (Checkpoint)
	{
		Checkpoint->CheckpointId = Id;
		if (const FAHCheckpointSpatialDefinition* Definition = AHChapterSpatial::FindCheckpointDefinition(Id))
		{
			Checkpoint->Stage = Definition->Stage;
			Checkpoint->ZoneId = Definition->ZoneId;
			Checkpoint->SetActorRotation(Definition->Rotation);
		}
		Checkpoint->Tags.Add(FName(TEXT("AH.SpatialCheckpoint")));
	}
}

AAHChapterTrigger* AAHChapterOneDirector::SpawnTrigger(const FVector& Location, const FVector& Extent, FName Id, EAHChapterStage Stage)
{
	AAHChapterTrigger* Trigger = GetWorld()->SpawnActor<AAHChapterTrigger>(AAHChapterTrigger::StaticClass(), Location, FRotator::ZeroRotator);
	if (Trigger)
	{
		Trigger->TriggerId = Id;
		Trigger->Stage = Stage;
		Trigger->ZoneId = AHChapterSpatial::GetStageDefinition(Stage).ZoneId;
		Trigger->Trigger->SetBoxExtent(Extent);
		Trigger->Tags.Add(FName(TEXT("AH.SpatialTrigger")));
		Triggers.Add(Trigger);
	}
	return Trigger;
}

AAHCombatEncounter* AAHChapterOneDirector::SpawnEncounter(FName Id, const FVector& Location, int32 Count, FName ObjectiveOnComplete, const TArray<FVector>& Spawns, bool bAutoActivate)
{
	AAHCombatEncounter* Encounter = GetWorld()->SpawnActor<AAHCombatEncounter>(AAHCombatEncounter::StaticClass(), Location, FRotator::ZeroRotator);
	if (Encounter)
	{
		Encounter->EncounterId = Id;
		Encounter->EnemyCount = Count;
		Encounter->ObjectiveOnComplete = ObjectiveOnComplete;
		Encounter->SpawnLocations = Spawns;
		Encounter->bActivateOnPlayerOverlap = !bAutoActivate;
	}
	return Encounter;
}

void AAHChapterOneDirector::SpawnFriendly(const FVector& Location, FName DisplayId)
{
	if (AAHHumanSoldierCharacter* Friendly = GetWorld()->SpawnActor<AAHHumanSoldierCharacter>(AAHHumanSoldierCharacter::StaticClass(), Location, FRotator::ZeroRotator))
	{
		if (DisplayId != NAME_None)
		{
			Friendly->Tags.Add(DisplayId);
		}
	}
}

void AAHChapterOneDirector::SpawnBattlefieldSimulation()
{
	for (int32 Index = 0; Index < 18; ++Index)
	{
		const float X = 200.0f + Index * 1600.0f;
		SpawnBlock(FVector(X, (Index % 2 == 0 ? -1700.0f : 1700.0f), 120.0f), FVector(0.25f, 0.75f, 1.2f));
	}
	for (int32 Index = 0; Index < 8; ++Index)
	{
		SpawnBlock(FVector(900.0f + Index * 1900.0f, 0.0f, 180.0f), FVector(0.8f, 0.15f, 1.5f));
	}
}

void AAHChapterOneDirector::SpawnLabel(const FVector& Location, const FString& Text, const FColor& Color, float WorldSize, const FRotator& Rotation)
{
	if (ATextRenderActor* Label = GetWorld()->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), Location, Rotation))
	{
		Label->GetTextRender()->SetText(FText::FromString(Text));
		Label->GetTextRender()->SetTextRenderColor(Color);
		Label->GetTextRender()->SetWorldSize(WorldSize);
		Label->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
		Label->GetTextRender()->SetVerticalAlignment(EVRTA_TextCenter);
		Label->Tags.Add(FName(TEXT("Phase4DebugVisual")));
		Label->GetTextRender()->SetVisibility(false);
	}
}

void AAHChapterOneDirector::SetGreyboxVisualVisibility(bool bVisible)
{
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || !Actor->ActorHasTag(FName(TEXT("Phase4GreyboxCollision"))))
		{
			continue;
		}
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents(PrimitiveComponents);
		for (UPrimitiveComponent* Component : PrimitiveComponents)
		{
			if (Component)
			{
				Component->SetVisibility(bVisible, true);
			}
		}
	}
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Presentation] GreyboxVisualLayer=%s collision_preserved=true"), bVisible ? TEXT("visible") : TEXT("hidden"));
}

void AAHChapterOneDirector::SetPresentationVisualVisibility(bool bVisible)
{
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor)
		{
			continue;
		}
		if (Actor->ActorHasTag(FName(TEXT("Phase4Presentation"))))
		{
			TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents;
			Actor->GetComponents(PrimitiveComponents);
			for (UPrimitiveComponent* Component : PrimitiveComponents)
			{
				if (Component)
				{
					Component->SetVisibility(bVisible, true);
				}
			}
		}
		TInlineComponentArray<USceneComponent*> SceneComponents;
		Actor->GetComponents(SceneComponents);
		for (USceneComponent* Component : SceneComponents)
		{
			if (Component && Component->ComponentTags.Contains(FName(TEXT("Phase4PresentationFX"))))
			{
				Component->SetVisibility(bVisible, true);
			}
		}
	}
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4.1][Presentation] AuthoredVisualLayer=%s collision_preserved=true"), bVisible ? TEXT("visible") : TEXT("hidden"));
}

void AAHChapterOneDirector::LogPresentationState(EAHChapterStage Stage)
{
	if (bHasLoggedPresentationStage && LastLoggedPresentationStage == Stage)
	{
		return;
	}
	bHasLoggedPresentationStage = true;
	LastLoggedPresentationStage = Stage;

	FString Profile = TEXT("Erebus");
	if (Stage == EAHChapterStage::TransitStation)
	{
		Profile = TEXT("Transit");
	}
	else if (Stage >= EAHChapterStage::CathedralApproach && Stage <= EAHChapterStage::Escape)
	{
		Profile = TEXT("Cathedral");
	}
	else if (Stage >= EAHChapterStage::TenYearsLater && Stage < EAHChapterStage::ChapterComplete)
	{
		Profile = TEXT("PresentDay");
	}
	const FString ProfilePath = FString::Printf(TEXT("/Game/Ashes/Presentation/DA_EnvironmentStyle_%s.DA_EnvironmentStyle_%s"), *Profile, *Profile);
	const UAHEnvironmentStyleData* EnvironmentStyle = LoadObject<UAHEnvironmentStyleData>(nullptr, *ProfilePath);
	const FName EnvironmentAudio = Stage == EAHChapterStage::TransitStation
		? FName(TEXT("Environment.Transit"))
		: Stage == EAHChapterStage::ManticoreSection
		? FName(TEXT("Environment.Manticore"))
		: Stage >= EAHChapterStage::TenYearsLater && Stage < EAHChapterStage::ChapterComplete
		? FName(TEXT("Environment.PresentDay"))
		: Stage >= EAHChapterStage::CathedralApproach && Stage <= EAHChapterStage::Escape
		? FName(TEXT("Environment.Cathedral"))
		: FName(TEXT("Environment.Erebus"));
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Presentation] Stage=%s Profile=%s MaterialFamily=%s Atmosphere=%s Audio=%s PlacedActors=%d VFX=%d LoadedPresentationAssets=%d MissingPresentationAssets=%d FailedBlueprintSpawns=%d FailedMeshLoads=%d FailedVFXLoads=%d"),
		*UEnum::GetValueAsString(Stage),
		*Profile,
		EnvironmentStyle ? TEXT("authored") : TEXT("missing"),
		EnvironmentStyle ? TEXT("fog+sky+lighting") : TEXT("missing"),
		*EnvironmentAudio.ToString(),
		PresentationActorCount,
		PresentationVFXCount,
		PresentationActorCount + PresentationVFXCount,
		MissingPresentationAssets,
		FailedBlueprintSpawns,
		FailedMeshLoads,
		FailedVFXLoads);
	#if !UE_BUILD_SHIPPING
	if (!EnvironmentStyle)
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Presentation] environment profile failed to load path=%s"), *ProfilePath);
	}
	#endif
}

void AAHChapterOneDirector::TeleportPlayer(const FVector& Location, const FRotator& Rotation)
{
	if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)))
	{
		Player->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void AAHChapterOneDirector::DebugSkipToStage(EAHChapterStage Stage)
{
	if (Chapter)
	{
		const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(Stage);
		const int32 ObjectiveIndex = UAHChapterSubsystem::ObjectiveIndexForStage(Stage);
		if (Objectives && ObjectiveIndex != INDEX_NONE)
		{
			Objectives->RestoreState(ObjectiveIndex);
			Chapter->SetObjectiveIndex(ObjectiveIndex);
		}
		TeleportPlayer(Definition.SafePlayerLocation, Definition.SafePlayerRotation);
		StartStage(Stage);
	}
}

void AAHChapterOneDirector::DebugLoadCheckpoint(FName CheckpointId)
{
	if (Chapter)
	{
		Chapter->SetCheckpoint(CheckpointId);
	}
	if (UAHCheckpointSubsystem* Checkpoints = GetWorld()->GetSubsystem<UAHCheckpointSubsystem>())
	{
		Checkpoints->ReloadLatestCheckpoint();
	}
}

void AAHChapterOneDirector::DebugResetChapter()
{
	if (Chapter)
	{
		Chapter->RestoreState(FAHChapterState());
	}
	UGameplayStatics::OpenLevel(GetWorld(), FName(*GetWorld()->GetName()));
}

void AAHChapterOneDirector::DebugCompleteCurrentEncounter()
{
	if (AAHCombatPlayerController* Controller = Cast<AAHCombatPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		Controller->KillAllEnemies();
	}
}

void AAHChapterOneDirector::DebugSetCountdown(float Seconds)
{
	if (Chapter)
	{
		Chapter->StartCountdown(Seconds);
	}
}

void AAHChapterOneDirector::DebugSpawnManticore()
{
	SpawnManticore();
}

void AAHChapterOneDirector::DebugTeleportToCathedral()
{
	TeleportPlayer(AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralInterior).SafePlayerLocation, AHChapterSpatial::GetStageDefinition(EAHChapterStage::CathedralInterior).SafePlayerRotation);
}

void AAHChapterOneDirector::DebugTeleportToPresentDay()
{
	StartStage(EAHChapterStage::TenYearsLater);
}

void AAHChapterOneDirector::DebugSpatialAudit()
{
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[SpatialAudit] BEGIN current=%s"), *UEnum::GetValueAsString(GetCurrentStage()));
	for (const FAHStageSpatialDefinition& Definition : AHChapterSpatial::GetStageDefinitions())
	{
		const bool bPass = ValidateStageSpatialDefinition(Definition, true);
		const bool bTargetBounds = Definition.ObjectiveTargetLocation.X >= Definition.ExpectedBoundsMin.X
			&& Definition.ObjectiveTargetLocation.X <= Definition.ExpectedBoundsMax.X
			&& Definition.ObjectiveTargetLocation.Y >= Definition.ExpectedBoundsMin.Y
			&& Definition.ObjectiveTargetLocation.Y <= Definition.ExpectedBoundsMax.Y;
		if (!bTargetBounds)
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("[SpatialAudit] %s FAIL objective target outside bounds target=%s"), *UEnum::GetValueAsString(Definition.Stage), *Definition.ObjectiveTargetLocation.ToCompactString());
		}
		if (!bPass)
		{
			UE_LOG(LogAshesOfHeaven, Error, TEXT("[SpatialAudit] %s FAIL canonical definition"), *UEnum::GetValueAsString(Definition.Stage));
		}
	}
	ValidateStageSpatialState(GetCurrentStage(), true);
	ValidateStageObjectiveConsistency(GetCurrentStage());
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[SpatialAudit] END"));
}
