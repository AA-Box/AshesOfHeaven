#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
#include "Gameplay/Chapter/AHLevelOneNarrative.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneNarrativeContractTest, "AshesOfHeaven.LevelOne.NarrativeContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneNarrativeContractTest::RunTest(const FString& Parameters)
{
	TArray<FAHDialogueLine> Lines;
	TestTrue(TEXT("Opening sequence resolves"), AHLevelOneNarrative::ResolveDirectorSequence(FName(TEXT("Ch01_Opening")), Lines));
	TestEqual(TEXT("Opening has two lines"), Lines.Num(), 2);
	if (Lines.Num() == 2)
	{
		TestEqual(TEXT("Opening question"), Lines[0].Subtitle.ToString(), FString(TEXT("Did we win?")));
		TestEqual(TEXT("Level title answer"), Lines[1].Subtitle.ToString(), FString(TEXT("For a while.")));
	}

	Lines.Reset();
	TestTrue(TEXT("Failsafe order resolves"), AHLevelOneNarrative::ResolveDirectorSequence(FName(TEXT("Ch01_Order")), Lines));
	TestFalse(TEXT("A sequence the director owns outright is not canonical data"), AHLevelOneNarrative::ResolveDirectorSequence(FName(TEXT("Ch01_NotALevelOneSequence")), Lines));
	TestTrue(TEXT("Failsafe explains outbound carrier"), Lines.ContainsByPredicate([](const FAHDialogueLine& Line)
	{
		return Line.Subtitle.ToString().Contains(TEXT("eight minutes, forty-two seconds"), ESearchCase::IgnoreCase);
	}));
	TestTrue(TEXT("Failsafe names the Veil signal"), Lines.ContainsByPredicate([](const FAHDialogueLine& Line)
	{
		return Line.Subtitle.ToString().Contains(TEXT("creating the Veil"), ESearchCase::IgnoreCase);
	}));

	FName StageSequence;
	Lines.Reset();
	TestTrue(TEXT("Terminal decision exists"), AHLevelOneNarrative::BuildStageEntrySequence(EAHChapterStage::FailsafeTerminal, StageSequence, Lines));
	TestEqual(TEXT("Terminal decision id"), StageSequence, FName(TEXT("Ch01_TerminalDecision")));
	TestTrue(TEXT("Exact casualty count is spoken"), Lines.ContainsByPredicate([](const FAHDialogueLine& Line)
	{
		return Line.Subtitle.ToString().Contains(TEXT("four hundred seven thousand, two hundred thirty-one"), ESearchCase::IgnoreCase);
	}));

	Lines.Reset();
	TestTrue(TEXT("Erebus finale exists"), AHLevelOneNarrative::BuildStageEntrySequence(EAHChapterStage::ErebusDestruction, StageSequence, Lines));
	TestTrue(TEXT("Nysa closes the level"), Lines.Num() > 0 && Lines.Last().Speaker == FName(TEXT("NYSA")) && Lines.Last().Subtitle.ToString() == TEXT("Lucian."));

	// The director holds the destruction stage on this value instead of a hand-fitted
	// constant, so the closing Nysa line survives any future line-length edit.
	const float FinaleSeconds = AHLevelOneNarrative::GetStageEntrySequenceDuration(EAHChapterStage::ErebusDestruction);
	const float HoldSeconds = AHLevelOneNarrative::GetErebusDestructionHoldSeconds();
	TestTrue(TEXT("Finale duration is measured"), FinaleSeconds > 0.0f);
	TestTrue(TEXT("Destruction hold covers the whole finale plus reading margin"), HoldSeconds >= FinaleSeconds + 1.0f);
	TestEqual(TEXT("Stage without a beat has no duration"), AHLevelOneNarrative::GetStageEntrySequenceDuration(EAHChapterStage::OpeningBlack), 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneProgressionContractTest, "AshesOfHeaven.LevelOne.ProgressionContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneProgressionContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Level One has twelve gameplay objectives"), AHChapterStateConstants::ObjectiveCount, 12);
	TestEqual(TEXT("Final objective stage is Erebus destruction"), UAHChapterSubsystem::StageForObjectiveIndex(11), EAHChapterStage::ErebusDestruction);
	TestEqual(TEXT("After final objective the level is complete"), UAHChapterSubsystem::StageForObjectiveIndex(12), EAHChapterStage::ChapterComplete);
	TestEqual(TEXT("Legacy TenYearsLater is outside Level One"), UAHChapterSubsystem::ObjectiveIndexForStage(EAHChapterStage::TenYearsLater), AHChapterStateConstants::ObjectiveCount);
	TestTrue(TEXT("Destruction is the final Level One objective"), AHLevelOneNarrative::IsLevelOneFinalObjective(FName(TEXT("Ch01_SurviveDestruction"))));

	FAHChapterState LegacyPostErebus;
	LegacyPostErebus.SaveVersion = 6;
	LegacyPostErebus.Stage = EAHChapterStage::TenYearsLater;
	LegacyPostErebus.ObjectiveIndex = 12;
	const FAHChapterState Migrated = UAHChapterSubsystem::NormalizeState(LegacyPostErebus);
	TestEqual(TEXT("v6 post-Erebus save migrates to complete"), Migrated.Stage, EAHChapterStage::ChapterComplete);
	TestTrue(TEXT("Migrated save is complete"), Migrated.bChapterComplete);

	// A retired post-Erebus stage is migrated whatever version wrote it. Version-gating this
	// let a v7 checkpoint recapture (which stamps Ch01_PresentDay's Stage=TenYearsLater onto the
	// saved chapter state) put the player back into the removed epilogue on the next boot.
	FAHChapterState CurrentVersionEpilogue;
	CurrentVersionEpilogue.SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
	CurrentVersionEpilogue.Stage = EAHChapterStage::StarsDisappearing;
	CurrentVersionEpilogue.ObjectiveIndex = 9999;
	const FAHChapterState Sanitized = UAHChapterSubsystem::NormalizeState(CurrentVersionEpilogue);
	TestEqual(TEXT("A current-version epilogue stage migrates to completion"), Sanitized.Stage, EAHChapterStage::ChapterComplete);
	TestTrue(TEXT("The migrated state is complete"), Sanitized.bChapterComplete);
	TestEqual(TEXT("Objective index lands on the terminal objective"), Sanitized.ObjectiveIndex, AHChapterStateConstants::ObjectiveCount);

	// Ordinary states still get their index bounded rather than passed through.
	FAHChapterState CorruptInLevel;
	CorruptInLevel.SaveVersion = AHChapterStateConstants::CurrentSaveVersion;
	CorruptInLevel.Stage = EAHChapterStage::Escape;
	CorruptInLevel.ObjectiveIndex = -5;
	const FAHChapterState SanitizedLow = UAHChapterSubsystem::NormalizeState(CorruptInLevel);
	TestEqual(TEXT("Negative objective index is clamped"), SanitizedLow.ObjectiveIndex, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneStageDialogueQueueTest, "AshesOfHeaven.LevelOne.StageDialogueQueue", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneStageDialogueQueueTest::RunTest(const FString& Parameters)
{
	// Boots the runtime order UWorld::BeginPlay uses: world subsystems bind before any
	// actor begins play, which is what makes stage-entry dialogue reachable at all.
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone();
	UWorld* World = GameInstance->GetWorld();
	if (!World)
	{
		AddError(TEXT("standalone game instance produced no world"));
		GameInstance->Shutdown();
		return false;
	}
	World->InitializeActorsForPlay(FURL());
	World->SetBegunPlay(true);
	World->BeginPlay();

	UAHDialogueSubsystem* Dialogue = World->GetSubsystem<UAHDialogueSubsystem>();
	UAHChapterSubsystem* Chapter = GameInstance->GetSubsystem<UAHChapterSubsystem>();
	if (!Dialogue || !Chapter)
	{
		AddError(TEXT("dialogue or chapter subsystem missing from the standalone world"));
		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);
		GameInstance->Shutdown();
		return false;
	}

	// A director-owned sequence occupies the dialogue channel, exactly as it does when a
	// stage change lands mid-sequence at runtime.
	FAHDialogueLine Holding;
	Holding.Speaker = FName(TEXT("SAEL"));
	Holding.Subtitle = FText::FromString(TEXT("Hold."));
	Holding.Duration = 60.0f;
	Dialogue->StartSequence(FName(TEXT("Test_DirectorBeat")), {Holding}, false);
	TestTrue(TEXT("Director sequence owns the channel"), Dialogue->HasActiveDialogue());

	// The canonical substitution is what actually reaches the player: a director calling
	// StartSequence with its own legacy lines must hear the authored Level One ones instead.
	FAHDialogueLine LegacyLine;
	LegacyLine.Speaker = FName(TEXT("LUCIAN"));
	LegacyLine.Subtitle = FText::FromString(TEXT("legacy director copy"));
	LegacyLine.Duration = 5.0f;
	Dialogue->StartSequence(FName(TEXT("Ch01_Opening")), {LegacyLine}, false);
	TestEqual(TEXT("Canonical Level One lines replace the director's inline copy"),
		Dialogue->GetCurrentSubtitle().ToString(), FString(TEXT("Did we win?")));
	Dialogue->SkipCurrentSequence();
	Dialogue->StartSequence(FName(TEXT("Test_DirectorBeat")), {Holding}, false);

	// A timer set outside FTimerManager::Tick is Pending and only becomes Active at the end
	// of the following Tick, and Tick early-outs once per frame. Two frames per pump is the
	// minimum that actually executes a 0.05s timer.
	const auto PumpTimers = [World]()
	{
		for (int32 Frame = 0; Frame < 5; ++Frame)
		{
			++GFrameCounter;
			World->GetTimerManager().Tick(0.06f);
		}
	};

	Chapter->SetStage(EAHChapterStage::ErebusOpening);
	PumpTimers();
	TestEqual(TEXT("Stage beat is queued instead of dropped"), Dialogue->GetPendingStageEntryCount(), 1);
	TestEqual(TEXT("Director sequence still playing"), Dialogue->GetCurrentSequence(), FName(TEXT("Test_DirectorBeat")));

	Dialogue->SkipCurrentSequence();
	TestEqual(TEXT("Queued stage beat plays when the channel frees"), Dialogue->GetCurrentSequence(), FName(TEXT("Ch01_ErebusOpeningBriefing")));
	TestEqual(TEXT("Queue is drained"), Dialogue->GetPendingStageEntryCount(), 0);

	// A director sequence still takes the channel immediately - progression depends on it - but
	// the stage beat it interrupts has to come back, resuming on the line it was cut on.
	Dialogue->Advance();
	Dialogue->Advance();
	const FString CutLine = Dialogue->GetCurrentSubtitle().ToString();
	TestEqual(TEXT("Advanced into the stage beat"), CutLine, FString(TEXT("That's usually considered an excuse.")));
	Dialogue->StartSequence(FName(TEXT("Test_DirectorPreempt")), {Holding}, false);
	TestEqual(TEXT("Preempting director sequence owns the channel"), Dialogue->GetCurrentSequence(), FName(TEXT("Test_DirectorPreempt")));
	TestEqual(TEXT("Preempted stage beat is requeued, not discarded"), Dialogue->GetPendingStageEntryCount(), 1);
	Dialogue->SkipCurrentSequence();
	TestEqual(TEXT("Preempted stage beat resumes"), Dialogue->GetCurrentSequence(), FName(TEXT("Ch01_ErebusOpeningBriefing")));
	TestEqual(TEXT("It resumes on the line it was cut on"), Dialogue->GetCurrentSubtitle().ToString(), CutLine);

	// A stage change with the channel free starts immediately, no queue involved.
	Dialogue->SkipCurrentSequence();
	Chapter->SetStage(EAHChapterStage::TransitStation);
	PumpTimers();
	TestEqual(TEXT("Free channel starts the stage beat directly"), Dialogue->GetCurrentSequence(), FName(TEXT("Ch01_TransitArrival")));
	Dialogue->SkipCurrentSequence();

	GEngine->DestroyWorldContext(World);
	World->DestroyWorld(false);
	GameInstance->Shutdown();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneUnrealMaterialContractTest, "AshesOfHeaven.LevelOne.UnrealMaterialContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneUnrealMaterialContractTest::RunTest(const FString& Parameters)
{
	// Level One presentation must remain inside Unreal's authored material/material-instance
	// pipeline. This is every material the runtime loads BY PATH: a by-path load that resolves
	// to null does not crash, it silently leaves the engine default material on the surface,
	// which is exactly how the Erebus sky ended up on the stock volumetric cloud shader. Keep
	// this list equal to the "/Game/Ashes/Materials/..." string literals under Source.
	const TArray<FString> RequiredMaterials = {
		TEXT("/Game/Ashes/Materials/Instances/MI_CathedralMatter_Dark.MI_CathedralMatter_Dark"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Concrete_Wet.MI_Concrete_Wet"),
		TEXT("/Game/Ashes/Materials/Instances/MI_EmissiveGlyph_Cyan.MI_EmissiveGlyph_Cyan"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_BannerCloth.MI_Erebus_BannerCloth"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_BannerEmblem.MI_Erebus_BannerEmblem"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_CathedralSilhouette.MI_Erebus_CathedralSilhouette"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_StormCloud.MI_Erebus_StormCloud"),
		TEXT("/Game/Ashes/Materials/Instances/MI_FireGlow_Orange.MI_FireGlow_Orange"),
		TEXT("/Game/Ashes/Materials/Instances/MI_HumanMetal_Dark.MI_HumanMetal_Dark"),
		TEXT("/Game/Ashes/Materials/Instances/MI_VeilObsidian_Black.MI_VeilObsidian_Black"),
		TEXT("/Game/Ashes/Materials/M_AH_FireSprite.M_AH_FireSprite"),
		TEXT("/Game/Ashes/Materials/M_AH_SmokeSoft.M_AH_SmokeSoft"),
		TEXT("/Game/Ashes/Materials/M_CathedralMatter.M_CathedralMatter"),
		TEXT("/Game/Ashes/Materials/M_Concrete.M_Concrete"),
		TEXT("/Game/Ashes/Materials/M_EmissiveGlyph.M_EmissiveGlyph"),
		TEXT("/Game/Ashes/Materials/M_Glass.M_Glass"),
		TEXT("/Game/Ashes/Materials/M_Grime.M_Grime"),
		TEXT("/Game/Ashes/Materials/M_HumanArmor.M_HumanArmor"),
		TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"),
		TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian")
	};

	for (const FString& MaterialPath : RequiredMaterials)
	{
		TestNotNull(*FString::Printf(TEXT("Unreal material resolves: %s"), *MaterialPath), LoadObject<UMaterialInterface>(nullptr, *MaterialPath));
	}
	return true;
}

#endif
