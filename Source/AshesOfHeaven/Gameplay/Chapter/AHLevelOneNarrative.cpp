#include "Gameplay/Chapter/AHLevelOneNarrative.h"
#include "AshesOfHeaven.h"
#include "Misc/SecureHash.h"
#include "Sound/SoundBase.h"

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
		// The same UTF-8 speaker/text key is used by GenerateDialogueVoices.py. Text edits
		// cannot silently play an old take, and different speakers never share a recording.
		const FString VoiceKey = FString(Speaker) + TEXT("\n") + Text;
		const FTCHARToUTF8 VoiceKeyUtf8(*VoiceKey);
		const FString Digest = FMD5::HashBytes(reinterpret_cast<const uint8*>(VoiceKeyUtf8.Get()), VoiceKeyUtf8.Length()).ToLower();
		const FString AssetName = FString::Printf(TEXT("SW_%s_%s"), *FString(Speaker).Replace(TEXT(" "), TEXT("_")), *Digest);
		const FString AssetPath = FString::Printf(TEXT("/Game/Ashes/Audio/Dialogue/%s.%s"), *AssetName, *AssetName);
		Result.Voice = LoadObject<USoundBase>(nullptr, *AssetPath);
		if (Result.Voice)
		{
			// Stage holds and subtitle timers must agree on the performed line length.
			Result.Duration = FMath::Max(Duration, Result.Voice->GetDuration() + 0.15f);
		}
		else
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("[DialogueVoice] Missing take for %s: %s. Regenerate and import dialogue voices."), Speaker, Text);
		}
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
	FMissionBriefing GetMissionBriefing(EAHChapterStage Stage)
	{
		const TCHAR* Location = TEXT("EREBUS / DISTRICT NINE");
		const TCHAR* Situation = TEXT("Erebus was a human colony. Now its defense network is falling silent, district by district. You are Lucian Vale, callsign Mourner Actual. Seventeen minutes after an attack knocked you unconscious, your squadmate Maya Serrin finds you in the ruins.");
		const TCHAR* Orders = TEXT("Reach the defensive line. Keep the route to Transit North open for the survivors.");
		switch (Stage)
		{
		case EAHChapterStage::OpeningBattle:
			Situation = TEXT("District Nine is the last line between the Veil and the evacuation route. Maya is counting on you to buy the survivors time.");
			Orders = TEXT("Repel the Veil assault. Use the barricades for cover; move to Transit North when the line is secure.");
			break;
		case EAHChapterStage::TransitStation:
			Location = TEXT("TRANSIT NORTH / EVACUATION ROUTE");
			Situation = TEXT("The line is holding. The station beneath it should be carrying civilians out of District Nine, but only the evacuation announcement is answering.");
			Orders = TEXT("Follow the North Line through Transit North to Platform 02. Reach the survivor beside Maya; the conversation begins when you arrive.");
			break;
		case EAHChapterStage::VeilRevelation:
			Location = TEXT("TRANSIT NORTH / PLATFORM 02");
			Situation = TEXT("A survivor recognizes something inside the Veil. Maya suspects the colony is being converted. Admiral Sael orders you to the Cathedral without explaining why.");
			Orders = TEXT("Stay near Maya and listen. No interaction is required; continue to the surface when the conversation ends and the objective changes.");
			break;
		case EAHChapterStage::OpenBattlefield:
			Location = TEXT("EREBUS / SURFACE CORRIDOR");
			Situation = TEXT("The Cathedral rises beyond the fighting. Ivo Ren is bringing Manticore Four-Seven through the wreckage to give your squad a way across.");
			Orders = TEXT("Move between cover positions toward Ivo's rendezvous on the Cathedral route. You do not need to eliminate every enemy; reach the rendezvous to meet the Manticore.");
			break;
		case EAHChapterStage::ManticoreSection:
			Location = TEXT("MANTICORE FOUR-SEVEN / RENDEZVOUS");
			Situation = TEXT("Ivo made it. The Manticore is damaged, but its armor is your best chance of reaching the Cathedral.");
			Orders = TEXT("Approach the Manticore and use the board interaction. Boarding completes this objective; follow the next objective toward the Cathedral entrance.");
			break;
		case EAHChapterStage::CathedralApproach:
			Location = TEXT("THE CATHEDRAL / OUTER PERIMETER");
			Situation = TEXT("The Cathedral is responding to the vehicle. Ivo is losing control of its systems. The final approach must be made on foot.");
			Orders = TEXT("Drive to the ramp drop-off. The Manticore stops there and returns control on foot. Climb the ramp to the Cathedral entrance, then await Sael's transmission.");
			break;
		case EAHChapterStage::FailsafeOrder:
		case EAHChapterStage::CathedralInterior:
		case EAHChapterStage::SaelTransmission:
			Location = TEXT("THE CATHEDRAL / CONTAINMENT FAILURE");
			Situation = TEXT("The signal is creating the Veil. In eight minutes and forty-two seconds, Erebus can transmit it beyond the planet. Sael has ordered Planetary Failsafe. Maya wants another way.");
			Orders = Stage == EAHChapterStage::FailsafeOrder
				? TEXT("Enter the Cathedral before the carrier opens. Find the failsafe control chamber.")
				: TEXT("Follow the expedition walkway to the terminal. The transmission deadline is still running.");
			break;
		case EAHChapterStage::FailsafeTerminal:
			Location = TEXT("FAILSAFE CONTROL / AUTHORIZATION REQUIRED");
			Situation = TEXT("11,407,231 lives. The terminal offers containment at the cost of everyone left on Erebus. Evacuation ships are still launching; Sael believes they could carry the signal with them.");
			Orders = TEXT("Inspect the terminal. Read the casualty assessment. Interact again to authorize Planetary Failsafe, then escape.");
			break;
		case EAHChapterStage::Escape:
		case EAHChapterStage::OtherLucian:
			Location = TEXT("THE CATHEDRAL / EVACUATION");
			Situation = TEXT("The failsafe is armed. The Cathedral is coming apart. Maya is still with you; reaching shelter is all that remains within your control.");
			Orders = TEXT("Follow the illuminated escape route to shelter. Keep moving through the Veil attack; reaching shelter, not clearing every enemy, completes the escape.");
			break;
		case EAHChapterStage::ErebusDestruction:
			Location = TEXT("EREBUS / LAST LIGHT");
			Situation = TEXT("You reached shelter. The failsafe is beyond recall. Wait for the fleet to confirm containment.");
			Orders = TEXT("Stay with Maya. Listen to the final transmission.");
			break;
		case EAHChapterStage::ChapterComplete:
			Location = TEXT("FOR A WHILE / CHAPTER COMPLETE");
			Situation = TEXT("Erebus is gone. Sael reports the signal contained. Then a voice from Nysa speaks your name. You saved the other worlds. You do not yet know what followed you out.");
			Orders = TEXT("Chapter One complete. Your campaign completion is saved.");
			break;
		default:
			break;
		}
		return {FText::FromString(Location), FText::FromString(Situation), FText::FromString(Orders)};
	}

	FText GetSpeakerIdentity(FName Speaker)
	{
		if (Speaker == TEXT("LUCIAN")) return FText::FromString(TEXT("LUCIAN VALE / MOURNER ACTUAL"));
		if (Speaker == TEXT("MAYA")) return FText::FromString(TEXT("MAYA SERRIN / SQUADMATE"));
		if (Speaker == TEXT("SAEL")) return FText::FromString(TEXT("ADMIRAL SAEL VAREK / FLEET COMMS"));
		if (Speaker == TEXT("IVO")) return FText::FromString(TEXT("IVO REN / MANTICORE FOUR-SEVEN"));
		return FText::FromName(Speaker);
	}

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
