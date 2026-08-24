#include "Gameplay/Chapter/AHLevelOneNarrative.h"

namespace
{
	FAHDialogueLine Line(const TCHAR* Speaker, const TCHAR* Text, float Duration)
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
			Line(TEXT("CHILD"), TEXT("Did we win?"), 2.4f),
			Line(TEXT("LUCIAN"), TEXT("For a while."), 3.2f)
		};
	}

	TArray<FAHDialogueLine> VeilRevelation()
	{
		return {
			Line(TEXT("MAYA"), TEXT("Ma'am? Civil Defense. Can you hear me?"), 2.5f),
			Line(TEXT("CIVILIAN"), TEXT("It remembers us."), 2.2f),
			Line(TEXT("MAYA"), TEXT("They're not invading."), 2.1f),
			Line(TEXT("LUCIAN"), TEXT("Maya."), 1.2f),
			Line(TEXT("MAYA"), TEXT("They're converting us."), 2.3f),
			Line(TEXT("SAEL"), TEXT("You do not have enough information to make that conclusion."), 3.1f),
			Line(TEXT("MAYA"), TEXT("Then give us the information."), 2.2f),
			Line(TEXT("SAEL"), TEXT("Reach the Cathedral."), 2.2f)
		};
	}

	TArray<FAHDialogueLine> FailsafeOrder()
	{
		return {
			Line(TEXT("SAEL"), TEXT("Lucian."), 1.5f),
			Line(TEXT("LUCIAN"), TEXT("Admiral."), 1.3f),
			Line(TEXT("SAEL"), TEXT("The containment fleet has lost Array Three."), 2.8f),
			Line(TEXT("MAYA"), TEXT("What does that mean?"), 1.8f),
			Line(TEXT("SAEL"), TEXT("Eight minutes, forty-two seconds until Erebus establishes an outbound carrier."), 4.0f),
			Line(TEXT("LUCIAN"), TEXT("Carrier for what?"), 1.8f),
			Line(TEXT("SAEL"), TEXT("The signal."), 1.8f),
			Line(TEXT("MAYA"), TEXT("What signal?"), 1.8f),
			Line(TEXT("SAEL"), TEXT("The one creating the Veil."), 2.4f),
			Line(TEXT("SAEL"), TEXT("Anyone sufficiently exposed can become part of the transmission. Ships can carry it. Radio may carry it. We do not know how far it can propagate."), 5.6f),
			Line(TEXT("SAEL"), TEXT("There is one remaining option."), 2.2f),
			Line(TEXT("MAYA"), TEXT("No."), 1.4f),
			Line(TEXT("LUCIAN"), TEXT("What option?"), 1.8f),
			Line(TEXT("SAEL"), TEXT("Planetary Failsafe."), 2.4f)
		};
	}

	TArray<FAHDialogueLine> OtherLucianFirst()
	{
		return {
			Line(TEXT("OTHER LUCIAN"), TEXT("You invaded my world."), 2.7f),
			Line(TEXT("LUCIAN"), TEXT("I've never seen you."), 2.2f),
			Line(TEXT("OTHER LUCIAN"), TEXT("You have."), 1.8f),
			Line(TEXT("OTHER LUCIAN"), TEXT("You just haven't done it yet."), 3.1f),
			Line(TEXT("MAYA"), TEXT("Lucian?"), 1.6f)
		};
	}

	TArray<FAHDialogueLine> OtherLucianSecond()
	{
		return {
			Line(TEXT("MAYA"), TEXT("Lucian? Move!"), 1.7f)
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
				Line(TEXT("MAYA"), TEXT("You're late."), 1.7f),
				Line(TEXT("LUCIAN"), TEXT("I was unconscious."), 1.9f),
				Line(TEXT("MAYA"), TEXT("That's usually considered an excuse."), 2.3f),
				Line(TEXT("LUCIAN"), TEXT("How long?"), 1.5f),
				Line(TEXT("MAYA"), TEXT("Seventeen minutes."), 1.8f),
				Line(TEXT("MAYA"), TEXT("Apparently seventeen important minutes."), 2.6f),
				Line(TEXT("SAEL"), TEXT("Mourner Actual, defensive line is collapsing at District Nine. All surviving units converge on Transit North."), 4.4f),
				Line(TEXT("MAYA"), TEXT("They're inside Transit?"), 1.8f),
				Line(TEXT("SAEL"), TEXT("Confirmed."), 1.2f),
				Line(TEXT("LUCIAN"), TEXT("How?"), 1.2f),
				Line(TEXT("SAEL"), TEXT("Unknown."), 1.7f)
			};
			return true;

		case EAHChapterStage::TransitStation:
			OutSequenceId = FName(TEXT("Ch01_TransitArrival"));
			OutLines = {
				Line(TEXT("STATION ANNOUNCEMENT"), TEXT("North Line service has been suspended. Proceed calmly to Civil Defense evacuation point—"), 4.0f),
				Line(TEXT("STATION ANNOUNCEMENT"), TEXT("North Line service has been suspended—"), 2.6f),
				Line(TEXT("IVO"), TEXT("Mourner, tell me you're still alive."), 2.1f),
				Line(TEXT("LUCIAN"), TEXT("Unfortunately."), 1.5f),
				Line(TEXT("IVO"), TEXT("Good. I already owe you money."), 2.3f),
				Line(TEXT("MAYA"), TEXT("How much?"), 1.5f),
				Line(TEXT("IVO"), TEXT("Enough that his death would create administrative complications."), 3.2f)
			};
			return true;

		case EAHChapterStage::OpenBattlefield:
			OutSequenceId = FName(TEXT("Ch01_BattlefieldReveal"));
			OutLines = {
				Line(TEXT("IVO"), TEXT("Manticore Four-Seven is moving to your route. Keep the lane open."), 3.3f),
				Line(TEXT("MAYA"), TEXT("That's your definition of reassuring?"), 2.1f),
				Line(TEXT("IVO"), TEXT("It was either that or lie."), 1.9f)
			};
			return true;

		case EAHChapterStage::ManticoreSection:
			OutSequenceId = FName(TEXT("Ch01_ManticoreArrival"));
			OutLines = {
				Line(TEXT("IVO"), TEXT("Manticore Four-Seven. Slightly used."), 2.3f),
				Line(TEXT("MAYA"), TEXT("Half the armor is missing."), 2.0f),
				Line(TEXT("IVO"), TEXT("That's the used part."), 2.1f),
				Line(TEXT("IVO"), TEXT("There she is."), 1.8f),
				Line(TEXT("MAYA"), TEXT("Don't call it she."), 1.8f),
				Line(TEXT("IVO"), TEXT("It's older than every government humanity ever built. I'm allowed to be respectful."), 3.6f),
				Line(TEXT("LUCIAN"), TEXT("Eyes forward."), 1.5f)
			};
			return true;

		case EAHChapterStage::CathedralApproach:
			OutSequenceId = FName(TEXT("Ch01_CathedralShutdown"));
			OutLines = {
				Line(TEXT("IVO"), TEXT("Controls aren't responding."), 2.0f),
				Line(TEXT("MAYA"), TEXT("EMP?"), 1.4f),
				Line(TEXT("IVO"), TEXT("No."), 1.2f),
				Line(TEXT("IVO"), TEXT("Something's responding."), 2.5f)
			};
			return true;

		case EAHChapterStage::FailsafeTerminal:
			OutSequenceId = FName(TEXT("Ch01_TerminalDecision"));
			OutLines = {
				Line(TEXT("MAYA"), TEXT("There are eleven million people here."), 2.7f),
				Line(TEXT("LUCIAN"), TEXT("Eleven million, four hundred seven thousand, two hundred thirty-one."), 3.8f),
				Line(TEXT("SAEL"), TEXT("I know."), 1.4f),
				Line(TEXT("MAYA"), TEXT("There are evacuation ships still launching."), 2.7f),
				Line(TEXT("SAEL"), TEXT("Those ships are exactly why we're out of time."), 3.0f),
				Line(TEXT("MAYA"), TEXT("You don't know that they're infected."), 2.4f),
				Line(TEXT("SAEL"), TEXT("Correct."), 1.3f),
				Line(TEXT("SAEL"), TEXT("And I cannot gamble every inhabited system on the possibility that they aren't."), 4.0f),
				Line(TEXT("MAYA"), TEXT("Don't do this."), 2.0f),
				Line(TEXT("SAEL"), TEXT("If Erebus transmits, there will not be eleven million dead."), 3.4f),
				Line(TEXT("SAEL"), TEXT("There will eventually be everyone."), 2.7f)
			};
			return true;

		case EAHChapterStage::Escape:
			OutSequenceId = FName(TEXT("Ch01_EscapeStart"));
			OutLines = {
				Line(TEXT("MAYA"), TEXT("Oh God."), 1.5f),
				Line(TEXT("SAEL"), TEXT("Run."), 1.2f)
			};
			return true;

		case EAHChapterStage::ErebusDestruction:
			OutSequenceId = FName(TEXT("Ch01_ErebusFinale"));
			OutLines = {
				Line(TEXT("MAYA"), TEXT("They don't even know."), 1.2f),
				Line(TEXT("MAYA"), TEXT("Eleven million people."), 1.1f),
				Line(TEXT("LUCIAN"), TEXT("I know."), 1.0f),
				Line(TEXT("SAEL"), TEXT("Signal terminated. Erebus is contained."), 1.4f),
				Line(TEXT("NYSA"), TEXT("Lucian."), 1.5f)
			};
			return true;

		default:
			return false;
		}
	}

	bool IsLevelOneFinalObjective(FName ObjectiveId)
	{
		return ObjectiveId == FName(TEXT("Ch01_SurviveDestruction"));
	}
}
