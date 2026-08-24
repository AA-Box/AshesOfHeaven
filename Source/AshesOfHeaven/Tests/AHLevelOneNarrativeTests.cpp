#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Chapter/AHLevelOneNarrative.h"
#include "Materials/MaterialInterface.h"

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
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHLevelOneUnrealMaterialContractTest, "AshesOfHeaven.LevelOne.UnrealMaterialContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHLevelOneUnrealMaterialContractTest::RunTest(const FString& Parameters)
{
	// Level One presentation must remain inside Unreal's authored material/material-instance
	// pipeline. These are representative families required by Erebus, Transit and Cathedral.
	const TArray<FString> RequiredMaterials = {
		TEXT("/Game/Ashes/Materials/Instances/MI_Concrete_Wet.MI_Concrete_Wet"),
		TEXT("/Game/Ashes/Materials/Instances/MI_HumanMetal_Dark.MI_HumanMetal_Dark"),
		TEXT("/Game/Ashes/Materials/Instances/MI_CathedralMatter_Dark.MI_CathedralMatter_Dark"),
		TEXT("/Game/Ashes/Materials/Instances/MI_VeilObsidian_Black.MI_VeilObsidian_Black"),
		TEXT("/Game/Ashes/Materials/Instances/MI_EmissiveGlyph_Cyan.MI_EmissiveGlyph_Cyan"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_BannerCloth.MI_Erebus_BannerCloth"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_BannerEmblem.MI_Erebus_BannerEmblem"),
		TEXT("/Game/Ashes/Materials/Instances/MI_Erebus_CathedralSilhouette.MI_Erebus_CathedralSilhouette")
	};

	for (const FString& MaterialPath : RequiredMaterials)
	{
		TestNotNull(*FString::Printf(TEXT("Unreal material resolves: %s"), *MaterialPath), LoadObject<UMaterialInterface>(nullptr, *MaterialPath));
	}
	return true;
}

#endif
