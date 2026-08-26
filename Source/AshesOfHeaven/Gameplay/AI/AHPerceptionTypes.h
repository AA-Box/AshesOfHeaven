#pragma once

#include "CoreMinimal.h"
#include "AHPerceptionTypes.generated.h"

/** How much an AI currently knows about one target. Drives acquisition, not damage. */
UENUM(BlueprintType)
enum class EAHAwarenessState : uint8
{
	/** Has not noticed the target. Will not advance on it and will not fire. */
	Unaware,
	/** Saw something. Moves to the last known point but does not treat it as a target. */
	Suspicious,
	/** Acquired. This is the only state that engages. */
	Alert
};

/** One frame of evidence about one target, gathered by the controller.
 *
 * Deliberately plain data with no engine types: the decision is a pure function so it can be
 * tested without a world, a pawn or a perception component - the same shape AHTacticalScoring
 * uses. The controller does the tracing; this decides what the tracing means.
 */
struct ASHESOFHEAVEN_API FAHAwarenessEvidence
{
	/** Clear trace from the AI's eye to the target. */
	bool bHasLineOfSight = false;
	/** Target is inside the AI's sight cone. Acquisition only - an alerted AI turns to face. */
	bool bWithinViewCone = false;
	float DistanceToTarget = 0.0f;
	/** From the archetype's FAHEnemyAISettings::SightRange. */
	float SightRange = 2600.0f;
	/** Crouching halves the rate the AI gains on you; sprinting raises it. */
	bool bTargetCrouched = false;
	bool bTargetSprinting = false;
	/** Set by damage or a nearby gunshot. Forces Alert regardless of sight. */
	bool bForcedAlert = false;
};

/** Tuning for the awareness ramp. Seconds-to-detect at point blank is 1/GainRate. */
struct ASHESOFHEAVEN_API FAHAwarenessTuning
{
	/** Awareness gained per second with clear sight at zero range. */
	float GainRatePerSecond = 1.35f;
	/** Awareness lost per second with no sight. 0.28 gives ~3.5s of memory from full alert. */
	float DecayRatePerSecond = 0.28f;
	/** Multiplies the gain rate while the target is crouched. */
	float CrouchGainScale = 0.45f;
	/** Multiplies the gain rate while the target is sprinting. */
	float SprintGainScale = 1.7f;
	/** Awareness at or above this starts investigating. */
	float SuspiciousThreshold = 0.45f;
	/** Awareness at or above this engages. */
	float AlertThreshold = 1.0f;
	/** Once alert, awareness must fall below this before the target is dropped, so a target
	 *  that ducks behind a pillar for half a second is not instantly forgotten. */
	float LoseTargetThreshold = 0.35f;
};

namespace AHPerception
{
	/** Fraction of the gain rate that applies at this distance: 1 at zero range, 0 at SightRange.
	 *  Linear on purpose - a designer setting SightRange 2600 should get "about half rate at
	 *  1300", not a curve they have to model. */
	ASHESOFHEAVEN_API float RangeFalloff(const FAHAwarenessEvidence& Evidence);

	/** Signed rate of change of awareness for this evidence, per second. Does not handle
	 *  bForcedAlert - that is a set rather than a rate, applied in StepAwareness. */
	ASHESOFHEAVEN_API float AwarenessDelta(const FAHAwarenessEvidence& Evidence, const FAHAwarenessTuning& Tuning);

	/** Advances awareness by DeltaSeconds and returns the new value, clamped to [0, 1]. */
	ASHESOFHEAVEN_API float StepAwareness(float CurrentAwareness, const FAHAwarenessEvidence& Evidence,
		const FAHAwarenessTuning& Tuning, float DeltaSeconds);

	/** State for an awareness value, given whether the AI already held this target.
	 *  The hysteresis is what stops a target flickering between Alert and Suspicious every time
	 *  it crosses a doorway. */
	ASHESOFHEAVEN_API EAHAwarenessState ResolveState(float Awareness, bool bWasAlert, const FAHAwarenessTuning& Tuning);
}
