#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "DrawDebugHelpers.h"

AAHCombatAIController::AAHCombatAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	AIPerception = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
	UAISenseConfig_Sight* Sight = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	Sight->SightRadius = SightRange;
	Sight->LoseSightRadius = SightRange * 1.15f;
	Sight->PeripheralVisionAngleDegrees = 105.0f;
	Sight->SetMaxAge(4.0f);
	AIPerception->ConfigureSense(*Sight);
	AIPerception->SetDominantSense(Sight->GetSenseImplementation());
}

void AAHCombatAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SetActorTickEnabled(true);
	AIPerception->Activate();
	Combatant = Cast<AAHCombatantCharacter>(InPawn);
	if (Combatant.IsValid())
	{
		Combatant->OnCombatantDeath.AddDynamic(this, &AAHCombatAIController::HandlePawnDeath);
		Combatant->SetAimSpreadPenaltyDegrees((1.0f - FMath::Clamp(Accuracy, 0.0f, 1.0f)) * MaxAimErrorDegrees);
	}
}

void AAHCombatAIController::HandlePawnDeath()
{
	StopMovement();
	SetActorTickEnabled(false);
	AIPerception->Deactivate();
	if (Combatant.IsValid() && Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
	UnPossess();
}

void AAHCombatAIController::OnUnPossess()
{
	if (Combatant.IsValid() && Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
	StopMovement();
	Super::OnUnPossess();
}

void AAHCombatAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Combatant.IsValid() || Combatant->IsCombatantDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	if (Now >= NextDecisionTime)
	{
		NextDecisionTime = Now + 0.25f;
		UpdateTarget();
		UpdateAttackSlot();
	}
	UpdateCombatBehavior(DeltaSeconds);
}

void AAHCombatAIController::UpdateTarget()
{
	if (CurrentTarget.IsValid() && Cast<AAHCombatantCharacter>(CurrentTarget.Get()) && !Cast<AAHCombatantCharacter>(CurrentTarget.Get())->IsCombatantDead())
	{
		return;
	}
	AActor* const PreviousTarget = CurrentTarget.Get();
	CurrentTarget = FindBestTarget();
	if (CurrentTarget.Get() != PreviousTarget)
	{
		// A new target is a new contact, so it gets its own grace window.
		FirstContactTime = -BIG_NUMBER;
	}
	// AAHCombatantCharacter::GetWeaponTargetLocation reads this. Without it every AI rifle aimed
	// down the control rotation instead of at anyone, so shots trailed behind the turn.
	if (Combatant.IsValid())
	{
		Combatant->SetCombatTarget(CurrentTarget.Get());
	}
}

AActor* AAHCombatAIController::FindBestTarget() const
{
	AAHCombatantCharacter* Best = nullptr;
	float BestScore = TNumericLimits<float>::Max();
	for (TActorIterator<AAHCombatantCharacter> It(GetWorld()); It; ++It)
	{
		AAHCombatantCharacter* Candidate = *It;
		if (!Candidate || Candidate == Combatant.Get() || Candidate->IsCombatantDead() || !Combatant->IsHostileTo(Candidate))
		{
			continue;
		}

		const float Distance = FVector::DistSquared(Candidate->GetActorLocation(), Combatant->GetActorLocation());
		const float PlayerBias = Candidate->GetFaction() == EAHFaction::Player ? 0.35f : 1.0f;
		if (Distance * PlayerBias < BestScore)
		{
			Best = Candidate;
			BestScore = Distance * PlayerBias;
		}
	}
	return Best;
}

bool AAHCombatAIController::HasLineOfSightTo(AActor* Target) const
{
	if (!Target || !Combatant.IsValid())
	{
		return false;
	}

	const FVector Start = Combatant->GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	const FVector End = Target->GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHAILineOfSight), true, Combatant.Get());
	FHitResult Hit;
	return !GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) || Hit.GetActor() == Target;
}

FVector AAHCombatAIController::ChooseCoverLocation(AActor* Target) const
{
	if (!Target || !GetWorld())
	{
		return Combatant.IsValid() ? Combatant->GetActorLocation() : FVector::ZeroVector;
	}

	const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	const FVector AwayFromTarget = (Combatant->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal();
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FVector Candidate = Combatant->GetActorLocation() + AwayFromTarget * FMath::FRandRange(350.0f, 850.0f) + FVector(0.0f, FMath::FRandRange(-700.0f, 700.0f), 0.0f);
		if (Nav)
		{
			FNavLocation NavLocation;
			if (Nav->ProjectPointToNavigation(Candidate, NavLocation))
			{
				Candidate = NavLocation.Location;
			}
		}

		FCollisionQueryParams Params(SCENE_QUERY_STAT(AHCoverTrace), true, Combatant.Get());
		FHitResult CoverHit;
		if (GetWorld()->LineTraceSingleByChannel(CoverHit, Target->GetActorLocation() + FVector(0.0f, 0.0f, 62.0f), Candidate + FVector(0.0f, 0.0f, 62.0f), ECC_Visibility, Params))
		{
			return Candidate;
		}
	}
	return Combatant->GetActorLocation() + AwayFromTarget * 500.0f;
}

void AAHCombatAIController::FaceLocation(const FVector& Target, float DeltaSeconds)
{
	if (!Combatant.IsValid())
	{
		return;
	}

	const FVector Direction = (Target - Combatant->GetActorLocation()).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}

	// Snapping control rotation made every combatant pivot like a turret, and the rifle aim
	// offset reads this rotation directly, so the pop was visible in the pose too.
	SetControlRotation(FMath::RInterpTo(GetControlRotation(), Direction.Rotation(), DeltaSeconds, 7.0f));
}

void AAHCombatAIController::MoveWithFallback(const FVector& Destination, float DeltaSeconds, bool bForceNewRequest)
{
	if (!Combatant.IsValid())
	{
		return;
	}

	// Re-issuing MoveToLocation every tick aborted and restarted path following before a single
	// step could finish, and AddMovementInput drove the pawn against the path follower on the
	// same frame. Together they walked a combatant back and forth over a metre - the short,
	// wrong loop. One request per goal, left alone until the goal actually moves or completes.
	const bool bGoalMoved = !CurrentMoveGoal.Equals(Destination, 150.0f);
	const bool bIdle = GetMoveStatus() == EPathFollowingStatus::Idle;
	if (bForceNewRequest || bGoalMoved || bIdle)
	{
		CurrentMoveGoal = Destination;
		MoveToLocation(Destination, 80.0f, true);
	}

	// Path following steers; this only keeps the body pointed where it is going.
	FaceLocation(Destination, DeltaSeconds);
}

void AAHCombatAIController::MaintainWeapon()
{
	if (!Combatant.IsValid() || !Combatant->GetInventoryComponent())
	{
		return;
	}

	AAHWeaponBase* Weapon = Combatant->GetInventoryComponent()->GetCurrentWeapon();
	if (!IsValid(Weapon) || Weapon->IsReloading())
	{
		return;
	}

	// Nothing in the AI ever reloaded, so a combatant emptied one magazine and then stood in the
	// open for the rest of the fight with a dry rifle - which is what "walks up and just stands
	// there" actually was. Top up before the magazine is gone so the pressure never lets up.
	const FAHAmmoState& Ammo = Weapon->GetAmmoState();
	if (Ammo.Magazine <= FMath::Max(1, Weapon->MagazineCapacity / 5) && Ammo.Reserve > 0)
	{
		if (Combatant->GetCombatComponent())
		{
			Combatant->GetCombatComponent()->StopFire();
			Combatant->GetCombatComponent()->Reload();
		}
	}
}

FVector AAHCombatAIController::GetStandoffLocation(const FVector& TargetLocation, float Range) const
{
	if (!Combatant.IsValid())
	{
		return TargetLocation;
	}

	// Pathing straight onto the target's own position is why combatants ended up shoulder to
	// shoulder with the player. Stop short of it and hold the range the rifle is good at.
	const FVector FromTarget = (Combatant->GetActorLocation() - TargetLocation).GetSafeNormal2D();
	if (FromTarget.IsNearlyZero())
	{
		return TargetLocation;
	}
	return TargetLocation + FromTarget * Range;
}

void AAHCombatAIController::UpdateCombatBehavior(float DeltaSeconds)
{
	const float Now = GetWorld()->GetTimeSeconds();
	AAHCombatantCharacter* Target = Cast<AAHCombatantCharacter>(CurrentTarget.Get());
	if (EscapeLocation != FVector::ZeroVector && FVector::DistSquared(EscapeLocation, Combatant->GetActorLocation()) > FMath::Square(120.0f))
	{
		MoveWithFallback(EscapeLocation, DeltaSeconds);
		return;
	}
	EscapeLocation = FVector::ZeroVector;

	if (Target && HasLineOfSightTo(Target))
	{
		LastKnownLocation = Target->GetActorLocation();
		LastSeenTime = Now;
		bHasSeenTarget = true;
		bInvestigating = false;
		NextSearchTime = 0.0f;

		const float Distance = FVector::Dist(Target->GetActorLocation(), Combatant->GetActorLocation());
		const bool bTooClose = Distance < MinimumEngagementRange;
		if (bTooClose)
		{
			// Break contact immediately rather than on the reposition cadence: standing inside a
			// rifle's minimum range is the one thing a combatant should never wait to fix.
			MoveWithFallback(GetStandoffLocation(Target->GetActorLocation(), PreferredEngagementRange), DeltaSeconds);
		}
		else if (Now >= NextRepositionTime && bPreferCover)
		{
			// A fresh cover point on a slow cadence. The old 2.5-5s reposition window landed on
			// top of a per-tick path restart, so a combatant never actually arrived anywhere.
			NextRepositionTime = Now + FMath::FRandRange(4.5f, 8.0f);
			MoveWithFallback(ChooseCoverLocation(Target), DeltaSeconds, true);
		}
		else if (GetMoveStatus() == EPathFollowingStatus::Idle)
		{
			// Holding position with a clear shot is correct; drifting off the goal is not.
			CurrentMoveGoal = FVector::ZeroVector;
		}
		// Aim at the target, not at the cover point: a combatant that can see you shoots at you
		// while it moves. The old code held fire for the whole approach and inside 900 units,
		// which is most of a firefight, so the enemies never returned any.
		FaceLocation(Target->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f), DeltaSeconds);
		if (FirstContactTime < 0.0f)
		{
			FirstContactTime = Now;
		}
		MaintainWeapon();
		ApplyAimDiscipline(Now);
		UpdateBurstFire(Now);
		return;
	}

	if (Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
	// Out of contact is the free moment to reload, not mid-burst.
	MaintainWeapon();
	if (Now - LastSeenTime > 3.0f)
	{
		FirstContactTime = -BIG_NUMBER;
	}
	if (bHasSeenTarget && Now - LastSeenTime < 6.0f)
	{
		bInvestigating = true;
		MoveWithFallback(GetStandoffLocation(LastKnownLocation, PreferredEngagementRange), DeltaSeconds);
	}
	else if (Target)
	{
		LastKnownLocation = Target->GetActorLocation();
		// Rolling a new random offset every frame meant the destination never stopped moving and
		// the combatant jittered on the spot. Hold one sweep point long enough to walk to it.
		if (Now >= NextSearchTime)
		{
			NextSearchTime = Now + FMath::FRandRange(3.5f, 6.0f);
			SearchLocation = LastKnownLocation + FVector(FMath::FRandRange(-400.0f, 400.0f), FMath::FRandRange(-500.0f, 500.0f), 0.0f);
		}
		MoveWithFallback(SearchLocation, DeltaSeconds);
	}
}

void AAHCombatAIController::UpdateAttackSlot()
{
	AActor* const Target = CurrentTarget.Get();
	if (!Target || !Combatant.IsValid())
	{
		bAttackSlotHeld = true;
		return;
	}

	// Closest N on the same target get to aim. ponytail: O(n^2) over AI controllers on the 0.25s
	// decision cadence, and n is capped by FAHPerformanceProfile::MaxActiveCombatants (24). Swap
	// in a per-target registry only if that cap ever rises.
	const float MyDistanceSquared = FVector::DistSquared(Combatant->GetActorLocation(), Target->GetActorLocation());
	int32 CloserAttackers = 0;
	for (TActorIterator<AAHCombatAIController> It(GetWorld()); It; ++It)
	{
		const AAHCombatAIController* Other = *It;
		if (!Other || Other == this || Other->CurrentTarget.Get() != Target)
		{
			continue;
		}
		const AAHCombatantCharacter* OtherPawn = Other->Combatant.Get();
		if (!OtherPawn || OtherPawn->IsCombatantDead())
		{
			continue;
		}
		if (FVector::DistSquared(OtherPawn->GetActorLocation(), Target->GetActorLocation()) < MyDistanceSquared)
		{
			++CloserAttackers;
		}
	}
	bAttackSlotHeld = CloserAttackers < FMath::Max(1, MaxSimultaneousAttackers);
}

void AAHCombatAIController::ApplyAimDiscipline(float Now)
{
	if (!Combatant.IsValid())
	{
		return;
	}

	const float AimedSpread = (1.0f - FMath::Clamp(Accuracy, 0.0f, 1.0f)) * MaxAimErrorDegrees;
	const bool bInGrace = FirstContactTime >= 0.0f && Now - FirstContactTime < FirstContactGraceSeconds;
	const bool bAiming = bAttackSlotHeld && !bInGrace;
	Combatant->SetAimSpreadPenaltyDegrees(bAiming ? AimedSpread : FMath::Max(AimedSpread, SuppressionSpreadDegrees));
}

void AAHCombatAIController::UpdateBurstFire(float Now)
{
	UAHCombatComponent* Combat = Combatant.IsValid() ? Combatant->GetCombatComponent() : nullptr;
	AAHWeaponBase* Weapon = Combatant.IsValid() && Combatant->GetInventoryComponent() ? Combatant->GetInventoryComponent()->GetCurrentWeapon() : nullptr;
	if (!Combat || !Weapon || Weapon->IsReloading())
	{
		return;
	}

	if (Weapon->IsFiring())
	{
		if (Now >= BurstEndTime)
		{
			Combat->StopFire();
			NextShotTime = Now + FMath::FRandRange(MinBurstPause, FMath::Max(MinBurstPause, MaxBurstPause));
		}
		return;
	}

	if (Now < NextShotTime)
	{
		return;
	}

	const float SecondsPerRound = Weapon->RoundsPerMinute > 0.0f ? 60.0f / Weapon->RoundsPerMinute : 0.1f;
	BurstEndTime = Now + SecondsPerRound * FMath::Max(1, BurstRounds);
	Combat->StartFire();
}

void AAHCombatAIController::ReactToGrenade(const FVector& GrenadeLocation, float Radius)
{
	if (!Combatant.IsValid())
	{
		return;
	}
	EscapeLocation = Combatant->GetActorLocation() + (Combatant->GetActorLocation() - GrenadeLocation).GetSafeNormal2D() * (Radius + 300.0f);
	if (Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
}

void AAHCombatAIController::DebugDrawAI() const
{
#if ENABLE_DRAW_DEBUG
	if (Combatant.IsValid())
	{
		DrawDebugSphere(GetWorld(), Combatant->GetActorLocation(), 90.0f, 12, FColor::Purple, false, 0.2f);
		if (CurrentTarget.IsValid())
		{
			DrawDebugLine(GetWorld(), Combatant->GetActorLocation(), CurrentTarget->GetActorLocation(), FColor::Red, false, 0.2f, 0, 2.0f);
		}
	}
#endif
}
