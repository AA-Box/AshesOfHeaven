#include "Gameplay/Chapter/AHLevelOneNarrative.h"

namespace
{
	// Distinct from AAHChapterOneDirector's own anonymous-namespace Line(): both files land
	// in one translation unit under unity builds, and identical names there are a redefinition.
	FAHDialogueLine NarrativeLine(const TCHAR* Speaker, const TCHAR* Text, float Duration)
	{
		FAHDialogueLine Result;
		Result.Speaker = FName(Speaker);
		Result.Subtitle = FText::FromString(Text);
		Result.Duration = Duration;
		return Result;
	}

	TArray<FAHDialogueLine> Opening()
	{
		return {
			NarrativeLine(TEXT("CHILD"), TEXT("Did we win?"), 2.4f),
			NarrativeLine(TEXT("LUCIAN"), TEXT("For a while."), 3.2f)
		};
	}

	TArray<FAHDialogueLine> VeilRevelation()
	{
		return {
			NarrativeLine(TEXT("MAYA"), TEXT("Ma'am? Civil Defense. Can you hear me?"), 2.5f),
			NarrativeLine(TEXT("CIVILIAN"), TEXT("It remembers us."), 2.2f),
			NarrativeLine(TEXT("MAYA"), TEXT("They're not invading."), 2.1f),
			NarrativeLine(TEXT("LUCIAN"), TEXT("Maya."), 1.2f),
			NarrativeLine(TEXT("MAYA"), TEXT("They're converting us."), 2.3f),
			NarrativeLine(TEXT("SAEL"), TEXT("You do not have enough information to make that conclusion."), 3.1f),
			NarrativeLine(TEXT("MAYA"), TEXT("Then give us the information."), 2.2f),
			NarrativeLine(TEXT("SAEL"), TEXT("Reach the Cathedral."), 2.2f)
		};
	}

	TArray<FAHDialogueLine> FailsafeOrder()
	{
		return {
			NarrativeLine(TEXT("SAEL"), TEXT("Lucian."), 1.5f),
			NarrativeLine(TEXT("LUCIAN"), TEXT("Admiral."), 1.3f),
			NarrativeLine(TEXT("SAEL"), TEXT("The containment fleet has lost Array Three."), 2.8f),
			NarrativeLine(TEXT("MAYA"), TEXT("What does that mean?"), 1.8f),
			NarrativeLine(TEXT("SAEL"), TEXT("Eight minutes, forty-two seconds until Erebus establishes an outbound carrier."), 4.0f),
			NarrativeLine(TEXT("LUCIAN"), TEXT("Carrier for what?"), 1.8f),
			NarrativeLine(TEXT("SAEL"), TEXT("The signal."), 1.8f),
			NarrativeLine(TEXT("MAYA"), TEXT("What signal?"), 1.8f),
			NarrativeLine(TEXT("SAEL"), TEXT("The one creating the Veil."), 2.4f),
			NarrativeLine(TEXT("SAEL"), TEXT("Anyone sufficiently exposed can become part of the transmission. Ships can carry it. Radio may carry it. We do not know how far it can propagate."), 5.6f),
			NarrativeLine(TEXT("SAEL"), TEXT("There is one remaining option."), 2.2f),
			NarrativeLine(TEXT("MAYA"), TEXT("No."), 1.4f),
			NarrativeLine(TEXT("LUCIAN"), TEXT("What option?"), 1.8f),
			NarrativeLine(TEXT("SAEL"), TEXT("Planetary Failsafe."), 2.4f)
		};
	}

	TArray<FAHDialogueLine> OtherLucianFirst()
	{
		return {
			NarrativeLine(TEXT("OTHER LUCIAN"), TEXT("You invaded my world."), 2.7f),
			NarrativeLine(TEXT("LUCIAN"), TEXT("I've never seen you."), 2.2f),
			NarrativeLine(TEXT("OTHER LUCIAN"), TEXT("You have."), 1.8f),
			NarrativeLine(TEXT("OTHER LUCIAN"), TEXT("You just haven't done it yet."), 3.1f),
			NarrativeLine(TEXT("MAYA"), TEXT("Lucian?"), 1.6f)
		};
	}

	TArray<FAHDialogueLine> OtherLucianSecond()
	{
		return {
			NarrativeLine(TEXT("MAYA"), TEXT("Lucian? Move!"), 1.7f)
		};
	}
}

namespace AHLevelOneNarrative
{
	bool ResolveDirectorSequence(FName SequenceId, TArray<FAHDialogueLine>& OutLines)
	{
		if (SequenceId == FName(TEXT("Ch01_Opening")))
		{
			OutLines = Opening();
			return true;
		}
		if (SequenceId == FName(TEXT("Ch01_VeilRevelation")))
		{
			OutLines = VeilRevelation();
			return true;
		}
		if (SequenceId == FName(TEXT("Ch01_Order")))
		{
			OutLines = FailsafeOrder();
			return true;
		}
		if (SequenceId == FName(TEXT("Ch01_Sael")))
		{
			OutLines = OtherLucianFirst();
			return true;
		}
		if (SequenceId == FName(TEXT("Ch01_OtherLucian")))
		{
			OutLines = OtherLucianSecond();
			return true;
		}
		return false;
	}

	bool BuildStageEntrySequence(EAHChapterStage Stage, FName& OutSequenceId, TArray<FAHDialogueLine>& OutLines)
	{
		OutSequenceId = NAME_None;
		OutLines.Reset();

		switch (Stage)
		{
		case EAHChapterStage::ErebusOpening:
			OutSequenceId = FName(TEXT("Ch01_ErebusOpeningBriefing"));
			OutLines = {
				NarrativeLine(TEXT("MAYA"), TEXT("You're late."), 1.7f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("I was unconscious."), 1.9f),
				NarrativeLine(TEXT("MAYA"), TEXT("That's usually considered an excuse."), 2.3f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("How long?"), 1.5f),
				NarrativeLine(TEXT("MAYA"), TEXT("Seventeen minutes."), 1.8f),
				NarrativeLine(TEXT("MAYA"), TEXT("Apparently seventeen important minutes."), 2.6f),
				NarrativeLine(TEXT("SAEL"), TEXT("Mourner Actual, defensive line is collapsing at District Nine. All surviving units converge on Transit North."), 4.4f),
				NarrativeLine(TEXT("MAYA"), TEXT("They're inside Transit?"), 1.8f),
				NarrativeLine(TEXT("SAEL"), TEXT("Confirmed."), 1.2f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("How?"), 1.2f),
				NarrativeLine(TEXT("SAEL"), TEXT("Unknown."), 1.7f)
			};
			return true;

		case EAHChapterStage::TransitStation:
			OutSequenceId = FName(TEXT("Ch01_TransitArrival"));
			OutLines = {
				NarrativeLine(TEXT("STATION ANNOUNCEMENT"), TEXT("North Line service has been suspended. Proceed calmly to Civil Defense evacuation point—"), 4.0f),
				NarrativeLine(TEXT("STATION ANNOUNCEMENT"), TEXT("North Line service has been suspended—"), 2.6f),
				NarrativeLine(TEXT("IVO"), TEXT("Mourner, tell me you're still alive."), 2.1f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("Unfortunately."), 1.5f),
				NarrativeLine(TEXT("IVO"), TEXT("Good. I already owe you money."), 2.3f),
				NarrativeLine(TEXT("MAYA"), TEXT("How much?"), 1.5f),
				NarrativeLine(TEXT("IVO"), TEXT("Enough that his death would create administrative complications."), 3.2f)
			};
			return true;

		case EAHChapterStage::OpenBattlefield:
			OutSequenceId = FName(TEXT("Ch01_BattlefieldReveal"));
			OutLines = {
				NarrativeLine(TEXT("IVO"), TEXT("Manticore Four-Seven is moving to your route. Keep the lane open."), 3.3f),
				NarrativeLine(TEXT("MAYA"), TEXT("That's your definition of reassuring?"), 2.1f),
				NarrativeLine(TEXT("IVO"), TEXT("It was either that or lie."), 1.9f)
			};
			return true;

		case EAHChapterStage::ManticoreSection:
			OutSequenceId = FName(TEXT("Ch01_ManticoreArrival"));
			OutLines = {
				NarrativeLine(TEXT("IVO"), TEXT("Manticore Four-Seven. Slightly used."), 2.3f),
				NarrativeLine(TEXT("MAYA"), TEXT("Half the armor is missing."), 2.0f),
				NarrativeLine(TEXT("IVO"), TEXT("That's the used part."), 2.1f),
				NarrativeLine(TEXT("IVO"), TEXT("There she is."), 1.8f),
				NarrativeLine(TEXT("MAYA"), TEXT("Don't call it she."), 1.8f),
				NarrativeLine(TEXT("IVO"), TEXT("It's older than every government humanity ever built. I'm allowed to be respectful."), 3.6f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("Eyes forward."), 1.5f)
			};
			return true;

		case EAHChapterStage::CathedralApproach:
			OutSequenceId = FName(TEXT("Ch01_CathedralShutdown"));
			OutLines = {
				NarrativeLine(TEXT("IVO"), TEXT("Controls aren't responding."), 2.0f),
				NarrativeLine(TEXT("MAYA"), TEXT("EMP?"), 1.4f),
				NarrativeLine(TEXT("IVO"), TEXT("No."), 1.2f),
				NarrativeLine(TEXT("IVO"), TEXT("Something's responding."), 2.5f)
			};
			return true;

		case EAHChapterStage::FailsafeTerminal:
			OutSequenceId = FName(TEXT("Ch01_TerminalDecision"));
			OutLines = {
				NarrativeLine(TEXT("MAYA"), TEXT("There are eleven million people here."), 2.7f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("Eleven million, four hundred seven thousand, two hundred thirty-one."), 3.8f),
				NarrativeLine(TEXT("SAEL"), TEXT("I know."), 1.4f),
				NarrativeLine(TEXT("MAYA"), TEXT("There are evacuation ships still launching."), 2.7f),
				NarrativeLine(TEXT("SAEL"), TEXT("Those ships are exactly why we're out of time."), 3.0f),
				NarrativeLine(TEXT("MAYA"), TEXT("You don't know that they're infected."), 2.4f),
				NarrativeLine(TEXT("SAEL"), TEXT("Correct."), 1.3f),
				NarrativeLine(TEXT("SAEL"), TEXT("And I cannot gamble every inhabited system on the possibility that they aren't."), 4.0f),
				NarrativeLine(TEXT("MAYA"), TEXT("Don't do this."), 2.0f),
				NarrativeLine(TEXT("SAEL"), TEXT("If Erebus transmits, there will not be eleven million dead."), 3.4f),
				NarrativeLine(TEXT("SAEL"), TEXT("There will eventually be everyone."), 2.7f)
			};
			return true;

		case EAHChapterStage::Escape:
			OutSequenceId = FName(TEXT("Ch01_EscapeStart"));
			OutLines = {
				NarrativeLine(TEXT("MAYA"), TEXT("Oh God."), 1.5f),
				NarrativeLine(TEXT("SAEL"), TEXT("Run."), 1.2f)
			};
			return true;

		case EAHChapterStage::ErebusDestruction:
			OutSequenceId = FName(TEXT("Ch01_ErebusFinale"));
			OutLines = {
				NarrativeLine(TEXT("MAYA"), TEXT("They don't even know."), 2.2f),
				NarrativeLine(TEXT("MAYA"), TEXT("Eleven million people."), 2.4f),
				NarrativeLine(TEXT("LUCIAN"), TEXT("I know."), 1.6f),
				NarrativeLine(TEXT("SAEL"), TEXT("Signal terminated. Erebus is contained."), 3.0f),
				NarrativeLine(TEXT("NYSA"), TEXT("Lucian."), 2.0f)
			};
			return true;

		default:
			return false;
		}
	}

	float GetStageEntrySequenceDuration(EAHChapterStage Stage)
	{
		FName SequenceId = NAME_None;
		TArray<FAHDialogueLine> Lines;
		if (!BuildStageEntrySequence(Stage, SequenceId, Lines))
		{
			return 0.0f;
		}

		float TotalSeconds = 0.0f;
		for (const FAHDialogueLine& DialogueLine : Lines)
		{
			// Matches the floor UAHDialogueSubsystem applies to every line timer.
			TotalSeconds += FMath::Max(0.1f, DialogueLine.Duration);
		}
		return TotalSeconds;
	}

	float GetErebusDestructionHoldSeconds()
	{
		// The finale plays as a stage-entry beat while the director holds the stage. Keep the
		// old 7s floor for the presentation fade, and always leave the last line room to read.
		constexpr float ClosingLineMargin = 1.5f;
		return FMath::Max(7.0f, GetStageEntrySequenceDuration(EAHChapterStage::ErebusDestruction) + ClosingLineMargin);
	}

	bool IsLevelOneFinalObjective(FName ObjectiveId)
	{
		return ObjectiveId == FName(TEXT("Ch01_SurviveDestruction"));
	}
}
