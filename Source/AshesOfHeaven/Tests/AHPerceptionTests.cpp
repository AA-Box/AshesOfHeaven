#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Gameplay/AI/AHPerceptionTypes.h"
#include "Misc/AutomationTest.h"

namespace
{
	/** Clear sight of a standing target at half the AI's sight range. */
	FAHAwarenessEvidence MakeSeenEvidence()
	{
		FAHAwarenessEvidence Evidence;
		Evidence.bHasLineOfSight = true;
		Evidence.bWithinViewCone = true;
		Evidence.DistanceToTarget = 1300.0f;
		Evidence.SightRange = 2600.0f;
		return Evidence;
	}

	/** Runs the ramp until it reaches Alert or the budget runs out; returns seconds taken. */
	float SecondsToAlert(const FAHAwarenessEvidence& Evidence, const FAHAwarenessTuning& Tuning, float Budget = 30.0f)
	{
		const float Step = 0.05f;
		float Awareness = 0.0f;
		for (float Elapsed = 0.0f; Elapsed < Budget; Elapsed += Step)
		{
			Awareness = AHPerception::StepAwareness(Awareness, Evidence, Tuning, Step);
			if (Awareness >= Tuning.AlertThreshold)
			{
				return Elapsed + Step;
			}
		}
		return Budget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHPerceptionAwarenessTest,
	"AshesOfHeaven.AI.Perception.AwarenessRamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHPerceptionAwarenessTest::RunTest(const FString& Parameters)
{
	const FAHAwarenessTuning Tuning;

	// Nothing seen means nothing learned, however long the AI stands there.
	FAHAwarenessEvidence Blind;
	Blind.SightRange = 2600.0f;
	TestEqual(TEXT("An AI with no line of sight never gains awareness"),
		AHPerception::StepAwareness(0.0f, Blind, Tuning, 10.0f), 0.0f);

	// The whole point of the feature: being seen is not instant.
	const float OpenSeconds = SecondsToAlert(MakeSeenEvidence(), Tuning);
	TestTrue(TEXT("A standing target is not detected instantly"), OpenSeconds > 0.5f);
	TestTrue(TEXT("A standing target in the open is detected within a few seconds"), OpenSeconds < 6.0f);

	// Crouching has to buy meaningfully more time or it is not a mechanic.
	FAHAwarenessEvidence Crouched = MakeSeenEvidence();
	Crouched.bTargetCrouched = true;
	const float CrouchSeconds = SecondsToAlert(Crouched, Tuning);
	TestTrue(TEXT("Crouching takes materially longer to be detected"), CrouchSeconds > OpenSeconds * 1.5f);

	// Sprinting past a guard should be the risky option.
	FAHAwarenessEvidence Sprinting = MakeSeenEvidence();
	Sprinting.bTargetSprinting = true;
	TestTrue(TEXT("Sprinting is detected faster than walking"),
		SecondsToAlert(Sprinting, Tuning) < OpenSeconds);

	// Distance matters, or there is no such thing as keeping your distance.
	FAHAwarenessEvidence Far = MakeSeenEvidence();
	Far.DistanceToTarget = 2400.0f;
	TestTrue(TEXT("A distant target is detected more slowly than a close one"),
		SecondsToAlert(Far, Tuning) > OpenSeconds);

	// Out of range is out of range, even with a clear trace.
	FAHAwarenessEvidence Beyond = MakeSeenEvidence();
	Beyond.DistanceToTarget = 3000.0f;
	TestEqual(TEXT("A target beyond sight range is never noticed"),
		AHPerception::StepAwareness(0.0f, Beyond, Tuning, 10.0f), 0.0f);

	// Breaking sight has to actually let go, or there is no escaping.
	const float Decayed = AHPerception::StepAwareness(Tuning.AlertThreshold, Blind, Tuning, 10.0f);
	TestEqual(TEXT("Awareness decays to nothing once sight is broken"), Decayed, 0.0f);
	TestEqual(TEXT("A fully decayed target is forgotten"),
		AHPerception::ResolveState(Decayed, true, Tuning), EAHAwarenessState::Unaware);

	// Being shot from an unseen angle must wake the AI up.
	FAHAwarenessEvidence Shot;
	Shot.bForcedAlert = true;
	TestEqual(TEXT("Damage or gunfire alerts regardless of sight"),
		AHPerception::ResolveState(AHPerception::StepAwareness(0.0f, Shot, Tuning, 0.1f), false, Tuning),
		EAHAwarenessState::Alert);

	// Hysteresis: a target that ducks briefly is not dropped.
	const float Ducked = AHPerception::StepAwareness(Tuning.AlertThreshold, Blind, Tuning, 0.5f);
	TestEqual(TEXT("A target that breaks sight briefly is still held"),
		AHPerception::ResolveState(Ducked, true, Tuning), EAHAwarenessState::Alert);
	TestEqual(TEXT("The same awareness would not have acquired the target from cold"),
		AHPerception::ResolveState(Ducked, false, Tuning), EAHAwarenessState::Suspicious);

	return true;
}

#endif
