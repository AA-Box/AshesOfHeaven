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
		if (!bOtherLucianShown)
		{
			bOtherLucianShown = true;
			StartStage(EAHChapterStage::OtherLucian);
		}
		break;
	case EAHChapterStage::OtherLucian:
		SpawnFriendly(FVector(21900.0f, 500.0f, 160.0f), FName(TEXT("OtherLucian")));
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
	SpawnLabel(FVector(300.0f, -1180.0f, 300.0f), TEXT("EREBUS\nTEN YEARS EARLIER"), FColor(220, 220, 220));
	SpawnLabel(FVector(3600.0f, -1180.0f, 300.0f), TEXT("TRANSIT STATION"), FColor(120, 180, 220));
	SpawnLabel(FVector(7000.0f, -1180.0f, 300.0f), TEXT("OPEN BATTLEFIELD"), FColor(220, 140, 80));
	SpawnLabel(FVector(13000.0f, -1600.0f, 2600.0f), TEXT("CATHEDRAL"), FColor(160, 120, 255));
	SpawnLabel(FVector(30000.0f, -1000.0f, 400.0f), TEXT("TEN YEARS LATER"), FColor(120, 220, 190));
	SpawnBattlefieldSimulation();
}

void AAHChapterOneDirector::BuildVisualArtTargets()
{
	if (bVisualArtTargetsBuilt)
	{
		return;
	}
	bVisualArtTargetsBuilt = true;

	// These are non-colliding presentation layers over the proven Phase 3 layout. The
	// gameplay blocks, triggers, checkpoints and nav data remain authoritative underneath.
	BuildErebusArtTarget();
	BuildTransitStationArtTarget();
	BuildCathedralArtTarget();
	BuildPresentDayArtTarget();
}

void AAHChapterOneDirector::BuildErebusArtTarget()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const TCHAR* Cylinder = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder");
	// Keep the presentation layer project-owned so a cooked game never depends on
	// editor-only LevelPrototyping content.
	const TCHAR* ChamferCube = Cube;

	// The authored prop Blueprints are the normal gameplay presentation layer. The
	// primitive shapes below remain modular framing pieces, not the whole scene.
	SpawnVisualShape(Cube, FVector(5200.0f, 0.0f, -72.0f), FVector(64.0f, 22.0f, 0.22f), FRotator::ZeroRotator, ConcreteMaterial);
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_BlastWall.BP_Erebus_BlastWall_C"), FVector(800.0f, -620.0f, 180.0f), FRotator(0.0f, 8.0f, 0.0f), FVector(1.8f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Barricade.BP_Erebus_Barricade_C"), FVector(1500.0f, 460.0f, 100.0f), FRotator(0.0f, -5.0f, 0.0f), FVector(1.25f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_PipeCluster.BP_Erebus_PipeCluster_C"), FVector(2350.0f, 650.0f, 120.0f), FRotator(0.0f, 18.0f, 0.0f), FVector(1.35f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Erebus_Wreck.BP_Erebus_Wreck_C"), FVector(3020.0f, -420.0f, 110.0f), FRotator(0.0f, -20.0f, -4.0f), FVector(1.4f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Human_ExpeditionLight.BP_Human_ExpeditionLight_C"), FVector(4100.0f, 540.0f, 120.0f), FRotator::ZeroRotator, FVector(1.0f));

	// Foreground: recoverable human fortification pieces with visible industrial mass.
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const float X = 250.0f + Index * 520.0f;
		SpawnVisualShape(ChamferCube, FVector(X, -540.0f, 155.0f), FVector(1.5f, 0.9f, 0.55f), FRotator(0.0f, Index % 2 == 0 ? 4.0f : -4.0f, 0.0f), HumanMetalMaterial);
		SpawnVisualShape(Cube, FVector(X + 160.0f, -520.0f, 320.0f), FVector(0.12f, 0.12f, 2.2f), FRotator::ZeroRotator, HumanMetalMaterial);
	}

	// Midground: a damaged defensive wall, logistics wreck and a readable fire source.
	SpawnVisualShape(Cube, FVector(1850.0f, 520.0f, 260.0f), FVector(5.5f, 0.28f, 2.6f), FRotator(0.0f, 0.0f, -8.0f), ConcreteMaterial);
	SpawnVisualShape(Cube, FVector(2120.0f, 520.0f, 430.0f), FVector(1.1f, 0.42f, 0.22f), FRotator(0.0f, 0.0f, 18.0f), HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(1220.0f, 300.0f, 115.0f), FVector(2.2f, 1.0f, 0.3f), FRotator(0.0f, -12.0f, 0.0f), HumanMetalMaterial);
	SpawnVisualShape(Cylinder, FVector(1220.0f, 300.0f, 215.0f), FVector(0.75f, 0.75f, 1.0f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cylinder, FVector(1550.0f, 700.0f, 74.0f), FVector(0.18f, 0.18f, 7.5f), FRotator(0.0f, 90.0f, 0.0f), HumanMetalMaterial);
	SpawnVisualShape(Cylinder, FVector(2500.0f, 700.0f, 110.0f), FVector(0.14f, 0.14f, 9.0f), FRotator(0.0f, 90.0f, 0.0f), HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(2440.0f, 680.0f, 185.0f), FVector(1.7f, 0.55f, 0.16f), FRotator(0.0f, 0.0f, -13.0f), HumanMetalMaterial);
	SpawnVisualLight(FVector(1180.0f, 260.0f, 230.0f), FLinearColor(1.0f, 0.23f, 0.06f), 1800.0f, 850.0f);
	SpawnVisualLight(FVector(2160.0f, 500.0f, 420.0f), FLinearColor(1.0f, 0.48f, 0.12f), 900.0f, 500.0f);
	SpawnVisualLight(FVector(4100.0f, 1220.0f, 400.0f), FLinearColor(1.0f, 0.18f, 0.05f), 500.0f, 720.0f);

	// Background: layered ruin forms frame the route and keep the Cathedral silhouette legible.
	for (int32 Index = 0; Index < 4; ++Index)
	{
		const float X = 4300.0f + Index * 900.0f;
		SpawnVisualShape(Cube, FVector(X, 1500.0f, 650.0f + (Index % 2) * 180.0f), FVector(1.8f, 0.25f, 6.5f + (Index % 2) * 1.5f), FRotator(0.0f, 0.0f, Index % 2 == 0 ? 3.0f : -4.0f), ConcreteMaterial);
	}
	SpawnVisualShape(Cube, FVector(10300.0f, 1550.0f, 850.0f), FVector(1.0f, 0.3f, 8.5f), FRotator(0.0f, 0.0f, -3.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, FVector(12600.0f, 1500.0f, 1250.0f), FVector(0.7f, 0.3f, 12.0f), FRotator(0.0f, 0.0f, 2.0f), VeilObsidianMaterial);

	SpawnVisualDust(FVector(950.0f, 0.0f, 420.0f), 1.7f);
	SpawnVisualDust(FVector(2550.0f, 760.0f, 520.0f), 1.2f);
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_AshField.NS_AshField"), FVector(2500.0f, 0.0f, 480.0f), FVector(2.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_EmberDrift.NS_EmberDrift"), FVector(1250.0f, -260.0f, 210.0f), FVector(1.2f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_FireSmall.NS_FireSmall"), FVector(1180.0f, 260.0f, 80.0f), FVector(1.0f));
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_SmokeColumn.NS_SmokeColumn"), FVector(2160.0f, 500.0f, 120.0f), FVector(1.0f));
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
	SpawnVisualShape(Cube, FVector(3500.0f, 0.0f, -30.0f), FVector(16.0f, 9.0f, 0.10f), FRotator::ZeroRotator, ConcreteMaterial);
	for (const float RailY : {-360.0f, 360.0f})
	{
		SpawnVisualShape(Cylinder, FVector(3500.0f, RailY, 18.0f), FVector(0.10f, 0.10f, 28.0f), FRotator(90.0f, 0.0f, 0.0f), HumanMetalMaterial);
	}
	SpawnVisualShape(Cube, FVector(3500.0f, -720.0f, 95.0f), FVector(16.0f, 0.08f, 0.95f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3500.0f, 720.0f, 95.0f), FVector(16.0f, 0.08f, 0.95f), FRotator::ZeroRotator, HumanMetalMaterial);

	// The station entrance is a strong frame on the route, with a practical overhead sign.
	for (const float PillarY : {-680.0f, 680.0f})
	{
		SpawnVisualShape(DoorFrame, FVector(3500.0f, PillarY, 370.0f), FVector(2.4f, 2.4f, 5.2f), FRotator::ZeroRotator, HumanMetalMaterial);
		SpawnVisualShape(Cube, FVector(3500.0f, PillarY, 740.0f), FVector(0.45f, 0.45f, 3.3f), FRotator::ZeroRotator, HumanMetalMaterial);
	}
	SpawnVisualShape(Cube, FVector(3500.0f, 0.0f, 760.0f), FVector(0.45f, 7.4f, 0.40f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3500.0f, -735.0f, 760.0f), FVector(0.08f, 1.9f, 0.72f), FRotator::ZeroRotator, EmissiveTechnologyMaterial);
	SpawnLabel(FVector(3500.0f, -820.0f, 770.0f), TEXT("TRANSIT\nSTATION"), FColor(224, 224, 210), 125.0f);
	SpawnLabel(FVector(3500.0f, -760.0f, 930.0f), TEXT("NORTH LINE  /  PLATFORM 02"), FColor(232, 190, 118), 72.0f);
	SpawnLabel(FVector(3500.0f, 790.0f, 710.0f), TEXT("CIVIL DEFENSE\nEVACUATION ROUTE"), FColor(222, 90, 62), 66.0f, FRotator(0.0f, -90.0f, 0.0f));

	// Civilian traces: a few abandoned cases and a control desk, not a prop carpet.
	SpawnVisualShape(Cube, FVector(3150.0f, -410.0f, 60.0f), FVector(0.45f, 0.30f, 0.28f), FRotator(0.0f, 18.0f, 0.0f), ConcreteMaterial);
	SpawnVisualShape(Cube, FVector(3270.0f, -450.0f, 52.0f), FVector(0.32f, 0.22f, 0.20f), FRotator(0.0f, -12.0f, 0.0f), ConcreteMaterial);
	SpawnVisualShape(Cube, FVector(3440.0f, -250.0f, 85.0f), FVector(1.8f, 0.24f, 0.12f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3440.0f, -250.0f, 160.0f), FVector(1.65f, 0.12f, 0.58f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3630.0f, 270.0f, 80.0f), FVector(1.3f, 0.22f, 0.10f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3630.0f, 270.0f, 145.0f), FVector(1.15f, 0.10f, 0.45f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3780.0f, 380.0f, 160.0f), FVector(1.3f, 0.65f, 0.72f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(3780.0f, 380.0f, 305.0f), FVector(1.1f, 0.58f, 0.08f), FRotator::ZeroRotator, EmissiveTechnologyMaterial);
	for (int32 Index = 0; Index < 4; ++Index)
	{
		SpawnVisualShape(Cylinder, FVector(3500.0f + Index * 480.0f, 0.0f, 1080.0f), FVector(0.08f, 0.08f, 4.5f), FRotator(0.0f, 90.0f, 0.0f), HumanMetalMaterial);
	}

	SpawnVisualLight(FVector(3300.0f, -500.0f, 540.0f), FLinearColor(1.0f, 0.48f, 0.16f), 620.0f, 700.0f);
	SpawnVisualLight(FVector(3820.0f, 500.0f, 460.0f), FLinearColor(0.95f, 0.08f, 0.03f), 360.0f, 520.0f);
	SpawnVisualDust(FVector(3500.0f, 0.0f, 520.0f), 0.8f);
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_DustSheet.NS_DustSheet"), FVector(3500.0f, 0.0f, 620.0f), FVector(0.8f));
}

void AAHChapterOneDirector::BuildCathedralArtTarget()
{
	const TCHAR* Cube = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube");
	const TCHAR* Cylinder = TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cylinder.SM_AH_Cylinder");

	SpawnVisualShape(Cube, FVector(16400.0f, 0.0f, -50.0f), FVector(20.0f, 5.0f, 0.18f), FRotator::ZeroRotator, CathedralMaterial);
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_Fin.BP_Cathedral_Fin_C"), FVector(15100.0f, -700.0f, 880.0f), FRotator(0.0f, 0.0f, -3.0f), FVector(2.8f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_Fin.BP_Cathedral_Fin_C"), FVector(16800.0f, 700.0f, 1000.0f), FRotator(0.0f, 180.0f, 4.0f), FVector(2.4f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Cathedral_GlyphPanel.BP_Cathedral_GlyphPanel_C"), FVector(17600.0f, -250.0f, 1250.0f), FRotator(0.0f, 0.0f, 0.0f), FVector(1.4f));
	SpawnPresentationProp(TEXT("/Game/Ashes/Blueprints/Environment/BP_Human_ExpeditionLight.BP_Human_ExpeditionLight_C"), FVector(16000.0f, -360.0f, 790.0f), FRotator::ZeroRotator, FVector(0.85f));

	// A controlled vocabulary of fins, frames and voids establishes the Cathedral language.
	for (const float X : {14200.0f, 15100.0f, 16000.0f, 17200.0f})
	{
		SpawnVisualShape(Cube, FVector(X, -1050.0f, 1300.0f), FVector(0.30f, 0.25f, 13.0f), FRotator(0.0f, 0.0f, 2.0f), VeilObsidianMaterial);
		SpawnVisualShape(Cube, FVector(X, 1050.0f, 1300.0f), FVector(0.30f, 0.25f, 13.0f), FRotator(0.0f, 0.0f, -2.0f), VeilObsidianMaterial);
	}
	SpawnVisualShape(Cube, FVector(15000.0f, 0.0f, 2300.0f), FVector(0.28f, 14.0f, 0.28f), FRotator::ZeroRotator, VeilObsidianMaterial);
	SpawnVisualShape(Cube, FVector(16400.0f, 0.0f, 1700.0f), FVector(0.18f, 9.0f, 0.18f), FRotator(0.0f, 0.0f, 8.0f), CathedralMaterial);
	SpawnVisualShape(Cube, FVector(17800.0f, 0.0f, 2100.0f), FVector(0.22f, 11.0f, 0.22f), FRotator(0.0f, 0.0f, -5.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, FVector(16800.0f, -700.0f, 2050.0f), FVector(1.4f, 3.8f, 0.38f), FRotator(0.0f, 0.0f, 7.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, FVector(17400.0f, 650.0f, 2550.0f), FVector(1.0f, 2.8f, 0.32f), FRotator(0.0f, 0.0f, -6.0f), VeilObsidianMaterial);
	SpawnVisualShape(Cube, FVector(18100.0f, 0.0f, 1500.0f), FVector(0.20f, 7.0f, 0.20f), FRotator::ZeroRotator, VeilObsidianMaterial);

	// Familiar human walkway and expedition equipment provide scale against the void.
	SpawnVisualShape(Cube, FVector(16000.0f, 0.0f, 790.0f), FVector(16.0f, 2.4f, 0.08f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(16000.0f, -245.0f, 900.0f), FVector(16.0f, 0.05f, 0.55f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(16000.0f, 245.0f, 900.0f), FVector(16.0f, 0.05f, 0.55f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(17400.0f, -280.0f, 900.0f), FVector(1.5f, 0.85f, 0.65f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cylinder, FVector(17400.0f, -280.0f, 1060.0f), FVector(0.5f, 0.5f, 1.2f), FRotator::ZeroRotator, HumanMetalMaterial);
	SpawnVisualShape(Cube, FVector(17650.0f, -280.0f, 920.0f), FVector(0.12f, 0.12f, 1.0f), FRotator(0.0f, 0.0f, 25.0f), EmissiveTechnologyMaterial);
	SpawnCathedralGlyph(FVector(16000.0f, -255.0f, 1420.0f), 150.0f, 1.0f);
	SpawnCathedralGlyph(FVector(17600.0f, -255.0f, 1710.0f), 95.0f, 0.72f);
	SpawnCathedralGlyph(FVector(18100.0f, -250.0f, 1490.0f), 210.0f, 1.35f);

	SpawnVisualLight(FVector(15800.0f, 0.0f, 1350.0f), FLinearColor(0.42f, 0.56f, 1.0f), 700.0f, 1000.0f);
	SpawnVisualLight(FVector(17900.0f, 0.0f, 1000.0f), FLinearColor(0.72f, 0.82f, 1.0f), 420.0f, 650.0f);
	SpawnVisualDust(FVector(15800.0f, 0.0f, 1600.0f), 1.4f);
	SpawnVisualDust(FVector(17700.0f, 260.0f, 1250.0f), 0.9f);
	SpawnVisualEffect(TEXT("/Game/Ashes/VFX/NS_CathedralMotes.NS_CathedralMotes"), FVector(16800.0f, 0.0f, 1500.0f), FVector(1.5f));
	SpawnLabel(FVector(15700.0f, -1320.0f, 2700.0f), TEXT("CATHEDRAL / INNER VOID"), FColor(190, 200, 232), 120.0f);
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
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Presentation] mesh failed to load path=%s"), MeshPath);
		#endif
		return nullptr;
	}
	AStaticMeshActor* Shape = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Location, Scale));
	if (Shape && Shape->GetStaticMeshComponent())
	{
		UStaticMeshComponent* Component = Shape->GetStaticMeshComponent();
		Component->SetStaticMesh(Mesh);
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Component->SetCanEverAffectNavigation(false);
		Shape->Tags.Add(FName(TEXT("Phase4Visual")));
		Shape->Tags.Add(FName(TEXT("Phase4Presentation")));
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

void AAHChapterOneDirector::SpawnVisualEffect(const TCHAR* SystemPath, const FVector& Location, const FVector& Scale)
{
	if (!GetWorld() || !SystemPath)
	{
		return;
	}
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, SystemPath);
	if (!System)
	{
		#if !UE_BUILD_SHIPPING
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Presentation] VFX failed to load path=%s"), SystemPath);
		#endif
		return;
	}
	if (UNiagaraComponent* Component = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, FRotator::ZeroRotator, Scale, true, true, ENCPoolMethod::AutoRelease))
	{
		Component->ComponentTags.Add(FName(TEXT("Phase4PresentationFX")));
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
	AAHChapterTrigger* Trigger = SpawnTrigger(FVector(-600.0f, 0.0f, 120.0f), FVector(280.0f, 1000.0f, 160.0f), FName(TEXT("ReachDefensiveLine")));
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	Trigger = SpawnTrigger(FVector(3500.0f, 0.0f, 120.0f), FVector(280.0f, 900.0f, 180.0f), FName(TEXT("ReachTransitStation")));
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	Trigger = SpawnTrigger(FVector(7200.0f, 0.0f, 120.0f), FVector(300.0f, 1100.0f, 180.0f), FName(TEXT("CrossBattlefield")));
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	Trigger = SpawnTrigger(FVector(14700.0f, 0.0f, 700.0f), FVector(400.0f, 1700.0f, 800.0f), FName(TEXT("EnterCathedral")));
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	Trigger = SpawnTrigger(FVector(17800.0f, 0.0f, 800.0f), FVector(300.0f, 1100.0f, 500.0f), FName(TEXT("ReachTerminal")));
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);
	Trigger = SpawnTrigger(FVector(24400.0f, 0.0f, 250.0f), FVector(450.0f, 1500.0f, 400.0f), FName(TEXT("EscapeCathedral")));
	if (Trigger) Trigger->OnTriggered.AddDynamic(this, &AAHChapterOneDirector::HandleTrigger);

	OpeningEncounter = SpawnEncounter(FName(TEXT("Ch01_OpeningBattle")), FVector(900.0f, 0.0f, 120.0f), 5, OpeningBattleObjective,
		{FVector(1200.0f, -520.0f, 120.0f), FVector(1500.0f, 520.0f, 120.0f), FVector(1900.0f, -600.0f, 120.0f), FVector(2250.0f, 620.0f, 120.0f), FVector(2600.0f, 0.0f, 120.0f)});
	BattlefieldEncounter = SpawnEncounter(FName(TEXT("Ch01_Battlefield")), FVector(7600.0f, 0.0f, 120.0f), 7, NAME_None,
		{FVector(7800.0f, -900.0f, 120.0f), FVector(8200.0f, 900.0f, 120.0f), FVector(8600.0f, -750.0f, 120.0f), FVector(9000.0f, 750.0f, 120.0f), FVector(9400.0f, -500.0f, 120.0f), FVector(9700.0f, 500.0f, 120.0f), FVector(10100.0f, 0.0f, 120.0f)});
	BattlefieldEncounter->AdditionalEnemyClasses.Add(AAHVeilWardenCharacter::StaticClass());
	EscapeEncounter = SpawnEncounter(FName(TEXT("Ch01_Escape")), FVector(20500.0f, 0.0f, 850.0f), 5, NAME_None,
		{FVector(20700.0f, -1000.0f, 850.0f), FVector(20900.0f, 1000.0f, 850.0f), FVector(21400.0f, -850.0f, 850.0f), FVector(21800.0f, 800.0f, 850.0f), FVector(22400.0f, 0.0f, 850.0f)});
	EscapeEncounter->AdditionalEnemyClasses.Add(AAHVeilWardenCharacter::StaticClass());

	for (int32 Index = 0; Index < 11; ++Index)
	{
		SpawnCheckpoint(FVector(-900.0f + Index * 2500.0f, 0.0f, 120.0f), FName(*FString::Printf(TEXT("Ch01_Checkpoint_%02d"), Index + 1)));
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
	TeleportPlayer(FVector(29200.0f, 0.0f, 150.0f), FRotator::ZeroRotator);
	SpawnBlock(FVector(30000.0f, 0.0f, -80.0f), FVector(18.0f, 10.0f, 0.8f));
	SpawnBlock(FVector(30200.0f, -850.0f, 250.0f), FVector(5.0f, 0.25f, 2.5f));
	SpawnBlock(FVector(30200.0f, 850.0f, 250.0f), FVector(5.0f, 0.25f, 2.5f));
	SpawnLabel(FVector(30000.0f, -1000.0f, 500.0f), TEXT("CAPTAIN MAYA SOL\nNYSA TRANSMISSION"), FColor(220, 220, 220), 105.0f);
}

void AAHChapterOneDirector::ActivateArtTargetView(FString TargetName)
{
	if (TargetName.Equals(TEXT("Erebus"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("Battlefield"), ESearchCase::IgnoreCase) || TargetName.Equals(TEXT("M91"), ESearchCase::IgnoreCase))
	{
		StartStage(EAHChapterStage::ErebusOpening);
		TeleportPlayer(FVector(600.0f, -250.0f, 150.0f), FRotator::ZeroRotator);
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
	FailsafeTerminal = GetWorld()->SpawnActor<AAHChapterTerminal>(AAHChapterTerminal::StaticClass(), FVector(18100.0f, 0.0f, 900.0f), FRotator::ZeroRotator);
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
	Manticore = GetWorld()->SpawnActor<AAHManticoreVehicle>(AAHManticoreVehicle::StaticClass(), FVector(8300.0f, -280.0f, 180.0f), FRotator::ZeroRotator);
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
		ADirectionalLight::StaticClass(), FVector(0.0f, 0.0f, 6000.0f), FRotator(-42.0f, -35.0f, 0.0f), SpawnParams))
	{
		SunLight->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			SunComponent->SetIntensity(1.45f);
			SunComponent->SetLightColor(FLinearColor(0.58f, 0.62f, 0.68f));
			SunComponent->SetAtmosphereSunLight(true);
		}
	}

	if (ASkyAtmosphere* SkyAtmosphere = GetWorld()->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		if (USkyAtmosphereComponent* Atmosphere = SkyAtmosphere->GetComponent())
		{
			Atmosphere->GroundAlbedo = FColor(14, 15, 17);
			Atmosphere->RayleighScattering = FLinearColor(0.012f, 0.016f, 0.022f);
			Atmosphere->MieScattering = FLinearColor(0.030f, 0.034f, 0.040f);
			Atmosphere->MieAbsorption = FLinearColor(0.016f, 0.018f, 0.022f);
			Atmosphere->MieAnisotropy = 0.62f;
			Atmosphere->MultiScatteringFactor = 0.32f;
			Atmosphere->MarkRenderStateDirty();
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
			SkyComponent->SetIntensity(0.22f);
		}
	}

	if (AExponentialHeightFog* Fog = GetWorld()->SpawnActor<AExponentialHeightFog>(AExponentialHeightFog::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams))
	{
		if (UExponentialHeightFogComponent* FogComponent = Fog->GetComponent())
		{
			FogComponent->SetMobility(EComponentMobility::Movable);
			FogComponent->SetFogDensity(0.020f);
			FogComponent->SetFogHeightFalloff(0.28f);
			FogComponent->SetFogInscatteringColor(FLinearColor(0.028f, 0.034f, 0.042f));
			FogComponent->SetStartDistance(420.0f);
		}
	}

	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Presentation] lighting profile=ErebusWar atmosphere=low-saturation fog=active"));
}

void AAHChapterOneDirector::SpawnBlock(const FVector& Location, const FVector& Scale, const FRotator& Rotation, UMaterialInterface* MaterialOverride)
{
	if (!GetWorld() || !BlockMesh)
	{
		return;
	}
	AStaticMeshActor* Block = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FTransform(Rotation, Location, Scale));
	if (Block && Block->GetStaticMeshComponent())
	{
		Block->GetStaticMeshComponent()->SetStaticMesh(BlockMesh);
		Block->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
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
	}
}

AAHChapterTrigger* AAHChapterOneDirector::SpawnTrigger(const FVector& Location, const FVector& Extent, FName Id)
{
	AAHChapterTrigger* Trigger = GetWorld()->SpawnActor<AAHChapterTrigger>(AAHChapterTrigger::StaticClass(), Location, FRotator::ZeroRotator);
	if (Trigger)
	{
		Trigger->TriggerId = Id;
		Trigger->Trigger->SetBoxExtent(Extent);
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
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Presentation] Stage=%s Profile=%s MaterialFamily=%s Atmosphere=%s Audio=%s PlacedActors=%d VFX=%d"),
		*UEnum::GetValueAsString(Stage),
		*Profile,
		EnvironmentStyle ? TEXT("authored") : TEXT("missing"),
		EnvironmentStyle ? TEXT("fog+sky+lighting") : TEXT("missing"),
		*EnvironmentAudio.ToString(),
		PresentationActorCount,
		PresentationVFXCount);
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
	TeleportPlayer(FVector(15000.0f, 0.0f, 850.0f));
}

void AAHChapterOneDirector::DebugTeleportToPresentDay()
{
	StartStage(EAHChapterStage::TenYearsLater);
}
