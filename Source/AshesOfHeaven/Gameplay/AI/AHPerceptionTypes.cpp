#include "Gameplay/AI/AHPerceptionTypes.h"

namespace AHPerception
{
	float RangeFalloff(const FAHAwarenessEvidence& Evidence)
	{
		if (Evidence.SightRange <= 0.0f)
		{
			return 0.0f;
		}
		if (Evidence.DistanceToTarget > Evidence.SightRange)
		{
			return 0.0f;
		}
		// Floored, not linear to zero. A pure 1-d/R ramp gains nothing at all at the edge of
		// sight range, so a target standing at exactly the limit is never noticed however long
		// it stands there - which reads as broken rather than as stealthy. The floor makes the
		// edge of vision slow (about seven seconds) instead of infinite.
		return FMath::Clamp(1.0f - Evidence.DistanceToTarget / Evidence.SightRange, 0.15f, 1.0f);
	}

	float AwarenessDelta(const FAHAwarenessEvidence& Evidence, const FAHAwarenessTuning& Tuning)
	{
		// The cone applies to noticing, not to keeping. An alerted AI turns to face its target,
		// and gating retention on the cone would make it forget whatever it is turning towards.
		if (!Evidence.bHasLineOfSight || !Evidence.bWithinViewCone)
		{
			return -Tuning.DecayRatePerSecond;
		}

		float Gain = Tuning.GainRatePerSecond * RangeFalloff(Evidence);
		if (Evidence.bTargetCrouched)
		{
			Gain *= Tuning.CrouchGainScale;
		}
		if (Evidence.bTargetSprinting)
		{
			Gain *= Tuning.SprintGainScale;
		}
		return Gain;
	}

	float StepAwareness(float CurrentAwareness, const FAHAwarenessEvidence& Evidence,
		const FAHAwarenessTuning& Tuning, float DeltaSeconds)
	{
		// A set, not a rate. Damage and gunfire bypass sight entirely - being shot from a
		// direction you cannot see is exactly the case where an AI must not stay unaware - and
		// ramping it would make the alert depend on how long the frame happened to be.
		if (Evidence.bForcedAlert)
		{
			return Tuning.AlertThreshold;
		}
		const float Delta = AwarenessDelta(Evidence, Tuning) * FMath::Max(0.0f, DeltaSeconds);
		return FMath::Clamp(CurrentAwareness + Delta, 0.0f, Tuning.AlertThreshold);
	}

	EAHAwarenessState ResolveState(float Awareness, bool bWasAlert, const FAHAwarenessTuning& Tuning)
	{
		if (bWasAlert)
		{
			// Hysteresis: an acquired target is only dropped once awareness falls well under the
			// threshold that acquired it, so stepping behind a pillar for half a second does not
			// reset the fight.
			return Awareness >= Tuning.LoseTargetThreshold ? EAHAwarenessState::Alert
				: (Awareness >= Tuning.SuspiciousThreshold * 0.5f ? EAHAwarenessState::Suspicious
					: EAHAwarenessState::Unaware);
		}
		if (Awareness >= Tuning.AlertThreshold)
		{
			return EAHAwarenessState::Alert;
		}
		return Awareness >= Tuning.SuspiciousThreshold ? EAHAwarenessState::Suspicious : EAHAwarenessState::Unaware;
	}
}
