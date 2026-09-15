#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Gameplay/Chapter/AHLevelOneNarrative.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundSubmix.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHDialogueVoiceCoverageTest, "AshesOfHeaven.LevelOne.DialogueVoiceCoverage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHDialogueVoiceCoverageTest::RunTest(const FString& Parameters)
{
	TArray<FAHDialogueLine> AllLines;
	for (const TCHAR* Id : {TEXT("Ch01_Opening"), TEXT("Ch01_VeilRevelation"), TEXT("Ch01_Order"), TEXT("Ch01_Sael"), TEXT("Ch01_OtherLucian")})
	{
		TArray<FAHDialogueLine> Lines;
		TestTrue(TEXT("Director dialogue resolves"), AHLevelOneNarrative::ResolveDirectorSequence(FName(Id), Lines));
		AllLines.Append(Lines);
	}
	for (int32 Stage = 0; Stage <= static_cast<int32>(EAHChapterStage::ChapterComplete); ++Stage)
	{
		FName SequenceId;
		TArray<FAHDialogueLine> Lines;
		if (AHLevelOneNarrative::BuildStageEntrySequence(static_cast<EAHChapterStage>(Stage), SequenceId, Lines))
		{
			AllLines.Append(Lines);
		}
	}
	TSet<FName> Speakers;
	TSet<USoundBase*> Recordings;
	USoundSubmix* DialogueSubmix = LoadObject<USoundSubmix>(nullptr, TEXT("/Game/Ashes/Audio/Submixes/SM_Dialogue.SM_Dialogue"));
	TestNotNull(TEXT("Dialogue bus exists"), DialogueSubmix);
	for (const FAHDialogueLine& Line : AllLines)
	{
		Speakers.Add(Line.Speaker);
		USoundWave* Voice = Cast<USoundWave>(Line.Voice);
		const FString Context = Line.Speaker.ToString() + TEXT(": ") + Line.Subtitle.ToString();
		if (!TestNotNull(Context + TEXT(" has a recording"), Voice)) continue;
		Recordings.Add(Voice);
		TestTrue(Context + TEXT(" has audible duration"), Voice->GetDuration() > 0.1f);
		TestTrue(Context + TEXT(" leaves time for the whole performance"), Line.Duration >= Voice->GetDuration() + 0.14f);
		TestEqual(Context + TEXT(" uses the dialogue bus"), Voice->GetSoundSubmix(), static_cast<USoundSubmixBase*>(DialogueSubmix));
		TestEqual(Context + TEXT(" is mono"), Voice->NumChannels, 1);
		TestFalse(Context + TEXT(" does not loop"), Voice->bLooping);
	}
	TestEqual(TEXT("Every speaking role is covered"), Speakers.Num(), 9);
	TestEqual(TEXT("Every unique speaker/text pair has a recording"), Recordings.Num(), 80);
	return true;
}

#endif
