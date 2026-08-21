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
#include "Components/BoxComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
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
	BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	BlockMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/M_PrototypeGrid.M_PrototypeGrid"));
	CathedralMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"));
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
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][ChapterDirector] start_stage=%s objective=%d"), *UEnum::GetValueAsString(Stage), Objectives ? Objectives->GetCurrentObjectiveIndex() : INDEX_NONE);
	#endif
	Chapter->SetStage(Stage);

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
		Chapter->SetObjectiveIndex(Objectives ? Objectives->GetCurrentObjectiveIndex() : Chapter->GetState().ObjectiveIndex + 1);
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
	else if (ObjectiveId == StarsObjective) StartStage(EAHChapterStage::ChapterComplete);
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
	SpawnBlock(FVector(14500.0f, 0.0f, -100.0f), FVector(190.0f, 32.0f, 1.0f));
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
	SpawnFriendly(FVector(30000.0f, 250.0f, 120.0f), FName(TEXT("MayaSol")));
	SpawnLabel(FVector(30000.0f, -1000.0f, 500.0f), TEXT("CAPTAIN MAYA SOL\nNYSA TRANSMISSION"), FColor(220, 220, 220));
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
		Manticore->VehicleMesh->SetMaterial(0, BlockMaterial);
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

	// The greybox map is generated empty and every actor here is spawned at runtime, so
	// nothing in the level provides illumination. Without these the scene renders black.
	// Neutral working light only - Phase 4 replaces this with the real lighting pass.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (ADirectionalLight* SunLight = GetWorld()->SpawnActor<ADirectionalLight>(
		ADirectionalLight::StaticClass(), FVector(0.0f, 0.0f, 6000.0f), FRotator(-42.0f, -35.0f, 0.0f), SpawnParams))
	{
		SunLight->SetMobility(EComponentMobility::Movable);
		if (UDirectionalLightComponent* SunComponent = Cast<UDirectionalLightComponent>(SunLight->GetLightComponent()))
		{
			SunComponent->SetIntensity(3.5f);
			SunComponent->SetAtmosphereSunLight(true);
		}
	}

	GetWorld()->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams);

	if (ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(
		ASkyLight::StaticClass(), FVector(0.0f, 0.0f, 4000.0f), FRotator::ZeroRotator, SpawnParams))
	{
		if (USkyLightComponent* SkyComponent = Sky->GetLightComponent())
		{
			SkyComponent->SetMobility(EComponentMobility::Movable);
			// Real-time capture sources ambient from the atmosphere; a static capture of an
			// unlit scene would just bake black.
			SkyComponent->SetRealTimeCapture(true);
			SkyComponent->SetIntensity(1.0f);
		}
	}

	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Greybox] lighting spawned sun+sky"));
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

void AAHChapterOneDirector::SpawnLabel(const FVector& Location, const FString& Text, const FColor& Color)
{
	if (ATextRenderActor* Label = GetWorld()->SpawnActor<ATextRenderActor>(ATextRenderActor::StaticClass(), Location, FRotator(0.0f, 90.0f, 0.0f)))
	{
		Label->GetTextRender()->SetText(FText::FromString(Text));
		Label->GetTextRender()->SetTextRenderColor(Color);
		Label->GetTextRender()->SetWorldSize(90.0f);
		Label->GetTextRender()->SetHorizontalAlignment(EHTA_Center);
		Label->GetTextRender()->SetVerticalAlignment(EVRTA_TextCenter);
	}
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
