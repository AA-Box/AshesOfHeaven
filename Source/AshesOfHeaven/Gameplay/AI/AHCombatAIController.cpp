#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Gameplay/AI/AHTacticalPositionSubsystem.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Performance/AHPerformanceStats.h"
#include "Performance/AHUpdateBudgetSubsystem.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "BehaviorTree/BehaviorTree.h"
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

void AAHCombatAIController::ApplyEnemySettings(const FAHEnemyAISettings& Settings)
{
	SightRange = Settings.SightRange;
	Accuracy = Settings.Accuracy;
	MaxAimErrorDegrees = Settings.MaxAimErrorDegrees;
	bPreferCover = Settings.bPreferCover;
	PreferredEngagementRange = Settings.PreferredEngagementRange;
	MinimumEngagementRange = Settings.MinimumEngagementRange;
	BurstRounds = Settings.BurstRounds;
	MinBurstPause = Settings.MinBurstPause;
	MaxBurstPause = Settings.MaxBurstPause;
	if (AIPerception)
	{
		if (UAISenseConfig_Sight* Sight = AIPerception->GetSenseConfig<UAISenseConfig_Sight>())
		{
			Sight->SightRadius = SightRange;
			Sight->LoseSightRadius = SightRange * 1.15f;
			AIPerception->RequestStimuliListenerUpdate();
		}
	}
	if (UBehaviorTree* BehaviorTree = Settings.BehaviorTree.Get())
	{
		RunBehaviorTree(BehaviorTree);
	}
	if (Combatant.IsValid())
	{
		Combatant->SetAimSpreadPenaltyDegrees((1.0f - FMath::Clamp(Accuracy, 0.0f, 1.0f)) * MaxAimErrorDegrees);
	}
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
		Combatant->SetAimSpreadPenaltyDegrees((1.0f - GetEffectiveAccuracy()) * MaxAimErrorDegrees);
		NextTacticalQueryTime = GetWorld()->GetTimeSeconds() + static_cast<float>(GetUniqueID() % 7) * 0.08f;
		const bool bActiveEncounterMember = InPawn->ActorHasTag(TEXT("ActiveEncounter"))
			|| InPawn->ActorHasTag(TEXT("AH.DirectedEncounter"))
			|| InPawn->GetOwner() != nullptr;
		if (UAHUpdateBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UAHUpdateBudgetSubsystem>())
		{
			Budget->RegisterCombatant(Combatant.Get(), this, bActiveEncounterMember);
		}
	}
}

void AAHCombatAIController::ApplyEncounterSophistication(float Sophistication)
{
	const float Pressure = FMath::Clamp(Sophistication, 0.65f, 1.50f);
	Accuracy = FMath::Clamp(Accuracy + (Pressure - 1.0f) * 0.16f, 0.45f, 0.90f);
	MinBurstPause = FMath::Max(0.25f, MinBurstPause / Pressure);
	MaxBurstPause = FMath::Max(MinBurstPause, MaxBurstPause / Pressure);
	FirstContactGraceSeconds = FMath::Clamp(FirstContactGraceSeconds / Pressure, 0.65f, 2.0f);
	MaxSimultaneousAttackers = FMath::Clamp(FMath::RoundToInt(MaxSimultaneousAttackers * Pressure), 1, 4);
	bPreferCover = bPreferCover || Pressure >= 1.0f;
	if (Combatant.IsValid())
	{
		Combatant->SetAimSpreadPenaltyDegrees((1.0f - Accuracy) * MaxAimErrorDegrees);
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
	if (Combatant.IsValid())
	{
		if (UAHUpdateBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UAHUpdateBudgetSubsystem>())
		{
			Budget->UnregisterCombatant(Combatant.Get());
		}
		if (UAHTacticalPositionSubsystem* Tactical = GetWorld()->GetSubsystem<UAHTacticalPositionSubsystem>())
		{
			Tactical->CancelForQuerier(Combatant.Get());
		}
	}
	if (Combatant.IsValid() && Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
	StopMovement();
	Super::OnUnPossess();
}

void AAHCombatAIController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Combatant.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			if (UAHUpdateBudgetSubsystem* Budget = World->GetSubsystem<UAHUpdateBudgetSubsystem>())
			{
				Budget->UnregisterCombatant(Combatant.Get());
			}
		}
	}
	Super::EndPlay(EndPlayReason);
}

void AAHCombatAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Combatant.IsValid() || Combatant->IsCombatantDead())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	UAHUpdateBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UAHUpdateBudgetSubsystem>();
	if (Budget && UAHUpdateBudgetSubsystem::IsEnabled())
	{
		const bool bCombatRelevant = CurrentTarget.IsValid() || (bHasSeenTarget && Now - LastSeenTime < 14.0f);
		Budget->SetCombatState(Combatant.Get(), bCombatRelevant, IsCurrentAttacker());
		const EAHSignificanceTier Tier = Budget->GetTier(Combatant.Get());
		if (Tier == EAHSignificanceTier::Dormant)
		{
			return;
		}

		float UpdateDelta = 0.0f;
		if (Tier == EAHSignificanceTier::Far)
		{
			if (Budget->IsUpdateDue(Combatant.Get(), EAHUpdateChannel::DistantBattlefieldSimulation, Now, UpdateDelta))
			{
				UpdateDistantBattlefieldSimulation(UpdateDelta);
			}
			return;
		}

		const bool bPerceptionDue = Budget->IsUpdateDue(Combatant.Get(), EAHUpdateChannel::Perception, Now, UpdateDelta);
		const bool bTacticalDue = Budget->IsUpdateDue(Combatant.Get(), EAHUpdateChannel::TacticalDecision, Now, UpdateDelta);
		if (bTacticalDue)
		{
			UpdateTarget();
			UpdateAttackSlot();
			const bool bUpdatedCombatRelevant = CurrentTarget.IsValid() || (bHasSeenTarget && Now - LastSeenTime < 14.0f);
			Budget->SetCombatState(Combatant.Get(), bUpdatedCombatRelevant, IsCurrentAttacker());
		}
		const bool bMovementDue = Budget->IsUpdateDue(Combatant.Get(), EAHUpdateChannel::Movement, Now, UpdateDelta);
		const bool bCombatDue = Budget->IsUpdateDue(Combatant.Get(), EAHUpdateChannel::Combat, Now, UpdateDelta);
		const bool bAimDue = Budget->IsUpdateDue(Combatant.Get(), EAHUpdateChannel::Aim, Now, UpdateDelta);
		UpdateCombatBehavior(DeltaSeconds, bPerceptionDue, bTacticalDue, bMovementDue, bCombatDue, bAimDue);
		return;
	}

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
	AH_SCOPE_PERFORMANCE(AIPerception, this);
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
		bCachedHasLineOfSight = false;
		bHasSeenTarget = false;
		bInvestigating = false;
		LastKnownLocation = FVector::ZeroVector;
		SearchLocation = FVector::ZeroVector;
		SetTacticalIntent(EAHTacticalIntent::Hold);
		if (Combatant.IsValid())
		{
			if (UAHTacticalPositionSubsystem* Tactical = GetWorld()->GetSubsystem<UAHTacticalPositionSubsystem>())
			{
				Tactical->CancelForQuerier(Combatant.Get());
			}
		}
		bTacticalQueryInFlight = false;
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
	AH_SCOPE_PERFORMANCE(AIPerception, this);
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

void AAHCombatAIController::FaceLocation(const FVector& Target, float DeltaSeconds)
{
	AH_SCOPE_PERFORMANCE(Combat, this);
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
		AH_SCOPE_PERFORMANCE(Movement, this);
		CurrentMoveGoal = Destination;
		MoveToLocation(Destination, 80.0f, true);
	}

	// Path following steers; this only keeps the body pointed where it is going.
	FaceLocation(Destination, DeltaSeconds);
}

void AAHCombatAIController::MaintainWeapon()
{
	AH_SCOPE_PERFORMANCE(Combat, this);
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

void AAHCombatAIController::UpdateCombatBehavior(float DeltaSeconds, bool bPerceptionDue, bool bTacticalDue,
	bool bMovementDue, bool bCombatDue, bool bAimDue)
{
	const float Now = GetWorld()->GetTimeSeconds();
	AAHCombatantCharacter* Target = Cast<AAHCombatantCharacter>(CurrentTarget.Get());
	if (Now < GrenadeThreatExpiryTime)
	{
		if (bCombatDue && Combatant->GetCombatComponent())
		{
			Combatant->GetCombatComponent()->StopFire();
		}
		if (bMovementDue && Now >= GrenadeReactionReadyTime)
		{
			SetTacticalIntent(EAHTacticalIntent::EscapeGrenade, GrenadeThreatExpiryTime - Now);
			// Grenade escape is explicitly protected from throttling, including its tactical query.
			ExecuteTacticalMovement(EAHTacticalIntent::EscapeGrenade, Target, DeltaSeconds, true);
		}
		return;
	}
	if (bTacticalDue && CurrentTacticalIntent == EAHTacticalIntent::EscapeGrenade)
	{
		SetTacticalIntent(EAHTacticalIntent::Hold);
	}
	GrenadeThreatRadius = 0.0f;

	if (bPerceptionDue)
	{
		bCachedHasLineOfSight = Target && HasLineOfSightTo(Target);
	}

	if (Target && bCachedHasLineOfSight)
	{
		if (bPerceptionDue)
		{
			LastKnownLocation = Target->GetActorLocation();
			LastSeenTime = Now;
			bHasSeenTarget = true;
			bInvestigating = false;
			NextSearchTime = 0.0f;
		}

		if (bTacticalDue)
		{
			AH_SCOPE_PERFORMANCE(AITacticalDecisions, this);
			const float Distance = FVector::Dist(Target->GetActorLocation(), Combatant->GetActorLocation());
			const bool bTooClose = Distance < MinimumEngagementRange;
			EAHTacticalIntent DesiredIntent = EAHTacticalIntent::Hold;
			if (bTooClose)
			{
				DesiredIntent = EAHTacticalIntent::Retreat;
				SetTacticalIntent(DesiredIntent, 3.0f);
			}
			else if (CurrentTacticalIntent == EAHTacticalIntent::Retreat && Now < TacticalIntentEndTime && Distance < PreferredEngagementRange * 0.9f)
			{
				DesiredIntent = EAHTacticalIntent::Retreat;
			}
			else if (Distance > PreferredEngagementRange * 1.35f)
			{
				DesiredIntent = EAHTacticalIntent::Advance;
				SetTacticalIntent(DesiredIntent, 4.0f);
			}
			else
			{
				const bool bRepositioning =
					(CurrentTacticalIntent == EAHTacticalIntent::FindCover
						|| CurrentTacticalIntent == EAHTacticalIntent::FlankLeft
						|| CurrentTacticalIntent == EAHTacticalIntent::FlankRight
						|| CurrentTacticalIntent == EAHTacticalIntent::Reposition)
					&& Now < TacticalIntentEndTime
					&& (bTacticalQueryInFlight
						|| (bHasCachedTacticalLocation && FVector::DistSquared2D(CachedTacticalLocation, Combatant->GetActorLocation()) > FMath::Square(130.0f)));
				if (bRepositioning)
				{
					DesiredIntent = CurrentTacticalIntent;
				}
				else if (Now >= NextRepositionTime)
				{
					float CadenceScale = 1.0f;
					switch (TacticalDifficulty)
					{
					case EAHAITacticalDifficulty::Recruit: CadenceScale = 1.25f; break;
					case EAHAITacticalDifficulty::Veteran: CadenceScale = 0.85f; break;
					case EAHAITacticalDifficulty::Damnation: CadenceScale = 0.72f; break;
					default: break;
					}
					NextRepositionTime = Now + FMath::FRandRange(4.5f, 8.0f) * CadenceScale;
					DesiredIntent = ChooseRepositionIntent();
					SetTacticalIntent(DesiredIntent, 5.0f);
				}
			}

			if (DesiredIntent == EAHTacticalIntent::Hold)
			{
				SetTacticalIntent(EAHTacticalIntent::Hold);
				if (GetMoveStatus() == EPathFollowingStatus::Idle)
				{
					CurrentMoveGoal = FVector::ZeroVector;
				}
			}
		}

		if (bMovementDue && CurrentTacticalIntent != EAHTacticalIntent::Hold)
		{
			ExecuteTacticalMovement(CurrentTacticalIntent, Target, DeltaSeconds, bTacticalDue);
		}
		// Aim at the target, not at the cover point: a combatant that can see you shoots at you
		// while it moves. The old code held fire for the whole approach and inside 900 units,
		// which is most of a firefight, so the enemies never returned any.
		if (bAimDue)
		{
			FaceLocation(Target->GetActorLocation() + FVector(0.0f, 0.0f, 55.0f), DeltaSeconds);
		}
		if (FirstContactTime < 0.0f)
		{
			FirstContactTime = Now;
		}
		if (bCombatDue)
		{
			MaintainWeapon();
			ApplyAimDiscipline(Now);
			UpdateBurstFire(Now);
		}
		return;
	}

	if (bCombatDue && Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
	// Out of contact is the free moment to reload, not mid-burst.
	if (bCombatDue)
	{
		MaintainWeapon();
	}
	if (bPerceptionDue && Now - LastSeenTime > 3.0f)
	{
		FirstContactTime = -BIG_NUMBER;
	}
	if (bHasSeenTarget && Now - LastSeenTime < 14.0f)
	{
		if (bTacticalDue)
		{
			bInvestigating = true;
			if (Now >= NextSearchTime)
			{
				NextSearchTime = Now + FMath::FRandRange(3.5f, 6.0f);
				bHasCachedTacticalLocation = false;
				bCachedTacticalFallback = false;
				NextTacticalQueryTime = FMath::Min(NextTacticalQueryTime, Now);
			}
			SetTacticalIntent(EAHTacticalIntent::SearchLastKnown, 14.0f - (Now - LastSeenTime));
		}
		if (bMovementDue && CurrentTacticalIntent == EAHTacticalIntent::SearchLastKnown)
		{
			ExecuteTacticalMovement(EAHTacticalIntent::SearchLastKnown, nullptr, DeltaSeconds, bTacticalDue);
		}
	}
	else if (bTacticalDue)
	{
		bInvestigating = false;
		SetTacticalIntent(EAHTacticalIntent::Hold);
	}
}

void AAHCombatAIController::UpdateDistantBattlefieldSimulation(float SimulatedDeltaSeconds)
{
	AH_SCOPE_PERFORMANCE(AITacticalDecisions, this);
	AAHCombatantCharacter* Target = Cast<AAHCombatantCharacter>(CurrentTarget.Get());
	if (Target && Target->IsCombatantDead())
	{
		CurrentTarget.Reset();
		bCachedHasLineOfSight = false;
		bHasSeenTarget = false;
		if (Combatant.IsValid())
		{
			Combatant->SetCombatTarget(nullptr);
		}
	}
	// The accumulated delta is intentionally consumed as a single deterministic simulation step.
	// Far actors never run navigation, LOS traces, tactical queries, weapon fire, or CharacterMovement.
	(void)SimulatedDeltaSeconds;
}

EAHTacticalIntent AAHCombatAIController::ChooseRepositionIntent() const
{
	if (!bAttackSlotHeld)
	{
		FRandomStream Random(GetUniqueID() ^ FMath::FloorToInt(GetWorld()->GetTimeSeconds() * 0.2f));
		if (Random.FRand() <= GetFlankWillingness())
		{
			return (GetUniqueID() & 1) == 0 ? EAHTacticalIntent::FlankLeft : EAHTacticalIntent::FlankRight;
		}
	}
	return bPreferCover ? EAHTacticalIntent::FindCover : EAHTacticalIntent::Reposition;
}

void AAHCombatAIController::SetTacticalIntent(EAHTacticalIntent NewIntent, float LifetimeSeconds)
{
	if (CurrentTacticalIntent == NewIntent)
	{
		return;
	}
	CurrentTacticalIntent = NewIntent;
	TacticalIntentEndTime = LifetimeSeconds > 0.0f ? GetWorld()->GetTimeSeconds() + LifetimeSeconds : -BIG_NUMBER;
	bHasCachedTacticalLocation = false;
	bCachedTacticalFallback = false;
	CachedTacticalIntent = EAHTacticalIntent::Hold;
	if (UAHTacticalPositionSubsystem::IsDebugEnabled())
	{
		UE_LOG(LogTemp, Display, TEXT("[AI.EQS] intent owner=%s requested=%s"), *GetNameSafe(Combatant.Get()), AHTacticalScoring::IntentToString(NewIntent));
	}
}

void AAHCombatAIController::ExecuteTacticalMovement(EAHTacticalIntent Intent, AActor* Target, float DeltaSeconds, bool bAllowTacticalQuery)
{
	if (!Combatant.IsValid() || Intent == EAHTacticalIntent::Hold)
	{
		return;
	}
	if (CurrentTacticalIntent != Intent)
	{
		SetTacticalIntent(Intent, 4.0f);
	}

	const float Now = GetWorld()->GetTimeSeconds();
	const bool bCanRunExpensiveQuery = Intent == EAHTacticalIntent::EscapeGrenade
		|| Intent == EAHTacticalIntent::SearchLastKnown
		|| IsExpensiveTacticalQueryAllowed(Target);
	if (bAllowTacticalQuery
		&& bCanRunExpensiveQuery
		&& AHTacticalScoring::CanStartQuery(
			bTacticalQueryInFlight,
			Now,
			NextTacticalQueryTime,
			bHasCachedTacticalLocation && !bCachedTacticalFallback))
	{
		RequestTacticalPosition(Intent, Target);
	}

	const FVector Destination = bHasCachedTacticalLocation && CachedTacticalIntent == Intent
		? CachedTacticalLocation
		: BuildTacticalFallback(Intent, Target);
	if (FVector::DistSquared2D(Destination, Combatant->GetActorLocation()) > FMath::Square(120.0f))
	{
		MoveWithFallback(Destination, DeltaSeconds);
	}
	else
	{
		CurrentMoveGoal = FVector::ZeroVector;
	}
}

void AAHCombatAIController::RequestTacticalPosition(EAHTacticalIntent Intent, AActor* Target)
{
	AH_SCOPE_PERFORMANCE(AITacticalDecisions, this);
	if (!Combatant.IsValid())
	{
		return;
	}
	UAHTacticalPositionSubsystem* Tactical = GetWorld()->GetSubsystem<UAHTacticalPositionSubsystem>();
	const FVector Fallback = BuildTacticalFallback(Intent, Target);
	if (!Tactical)
	{
		FAHTacticalPositionResult Result;
		Result.Intent = Intent;
		Result.Location = Fallback;
		Result.bUsedFallback = true;
		Result.FailureReason = TEXT("SubsystemUnavailable");
		HandleTacticalQueryFinished(Result);
		return;
	}

	FAHPerformanceProfile PerformanceProfile;
	if (const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		PerformanceProfile = Platform->GetPerformanceProfile();
	}

	FAHTacticalPositionRequest Request;
	Request.Querier = Combatant.Get();
	Request.CombatTarget = Intent == EAHTacticalIntent::SearchLastKnown ? nullptr : Target;
	Request.Intent = Intent;
	Request.QueryKind = AHTacticalScoring::QueryKindForIntent(Intent);
	Request.Origin = Combatant->GetActorLocation();
	Request.LastKnownTarget = LastKnownLocation.IsNearlyZero() ? Request.Origin : LastKnownLocation;
	Request.GrenadeThreat = GrenadeThreatLocation;
	Request.FallbackLocation = Fallback;
	Request.PreferredRange = PreferredEngagementRange;
	Request.MinimumRange = MinimumEngagementRange;
	Request.CurrentDistanceToTarget = Target ? FVector::Dist2D(Request.Origin, Target->GetActorLocation()) : 0.0f;
	Request.GrenadeDangerRadius = GetWorld()->GetTimeSeconds() < GrenadeThreatExpiryTime ? GrenadeThreatRadius : 0.0f;
	Request.QualityTolerance = GetTacticalQualityTolerance();
	Request.TimeoutSeconds = PerformanceProfile.EQSQueryTimeout;
	Request.MaxCandidatePoints = PerformanceProfile.EQSMaxCandidatePoints;
	Request.RandomSeed = GetUniqueID() ^ (++TacticalQueryRequestCount * 7919);
	Request.bAccurateAttacker = bAttackSlotHeld;
	Request.bSuppressionAttacker = !bAttackSlotHeld;
	Request.bSimplifiedScoring = PerformanceProfile.bUseSimplifiedEQSScoring;

	for (TActorIterator<AAHCombatAIController> It(GetWorld()); It; ++It)
	{
		const AAHCombatAIController* Other = *It;
		AAHCombatantCharacter* OtherPawn = Other && Other != this ? Other->Combatant.Get() : nullptr;
		if (OtherPawn && !OtherPawn->IsCombatantDead() && !Combatant->IsHostileTo(OtherPawn))
		{
			Request.Squadmates.Add(OtherPawn);
		}
	}

	if (const UGameInstance* GameInstance = GetWorld()->GetGameInstance())
	{
		if (const UAHChapterSubsystem* Chapter = GameInstance->GetSubsystem<UAHChapterSubsystem>())
		{
			const FAHStageSpatialDefinition& Stage = AHChapterSpatial::GetStageDefinition(Chapter->GetStage());
			Request.PlayableBounds = FBox(Stage.ExpectedBoundsMin, Stage.ExpectedBoundsMax);
		}
	}

	bTacticalQueryInFlight = true;
	bHasCachedTacticalLocation = false;
	bCachedTacticalFallback = false;
	NextTacticalQueryTime = GetWorld()->GetTimeSeconds() + GetTacticalQueryInterval();
	Tactical->RequestPosition(Request, FAHTacticalQueryFinished::CreateUObject(this, &AAHCombatAIController::HandleTacticalQueryFinished));
}

void AAHCombatAIController::HandleTacticalQueryFinished(const FAHTacticalPositionResult& Result)
{
	bTacticalQueryInFlight = false;
	if (!Combatant.IsValid() || Result.Intent != CurrentTacticalIntent)
	{
		NextTacticalQueryTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
		return;
	}

	CachedTacticalIntent = Result.Intent;
	CachedTacticalLocation = Result.Location;
	CachedTacticalScore = Result.Score;
	bHasCachedTacticalLocation = true;
	bCachedTacticalFallback = Result.bUsedFallback;
}

FVector AAHCombatAIController::BuildTacticalFallback(EAHTacticalIntent Intent, AActor* Target) const
{
	if (!Combatant.IsValid())
	{
		return FVector::ZeroVector;
	}
	const FVector Origin = Combatant->GetActorLocation();
	const FVector TargetLocation = Target ? Target->GetActorLocation() : LastKnownLocation;
	FVector Away = (Origin - TargetLocation).GetSafeNormal2D();
	if (Intent == EAHTacticalIntent::EscapeGrenade)
	{
		Away = (Origin - GrenadeThreatLocation).GetSafeNormal2D();
	}
	if (Away.IsNearlyZero())
	{
		Away = Combatant->GetActorForwardVector() * -1.0f;
	}

	FVector Destination = Origin;
	switch (Intent)
	{
	case EAHTacticalIntent::Advance:
		Destination = GetStandoffLocation(TargetLocation, PreferredEngagementRange);
		break;
	case EAHTacticalIntent::Retreat:
		Destination = Origin + Away * FMath::Max(700.0f, PreferredEngagementRange - MinimumEngagementRange + 350.0f);
		break;
	case EAHTacticalIntent::EscapeGrenade:
		Destination = Origin + Away * FMath::Max(800.0f, GrenadeThreatRadius + 350.0f);
		break;
	case EAHTacticalIntent::FlankLeft:
	case EAHTacticalIntent::FlankRight:
	{
		const float Side = Intent == EAHTacticalIntent::FlankLeft ? 1.0f : -1.0f;
		Destination = TargetLocation + Away.RotateAngleAxis(70.0f * Side, FVector::UpVector) * PreferredEngagementRange;
		break;
	}
	case EAHTacticalIntent::SearchLastKnown:
	{
		FRandomStream Random(GetUniqueID() ^ (TacticalQueryRequestCount + 1) * 3571);
		Destination = LastKnownLocation + FVector(Random.FRandRange(-550.0f, 550.0f), Random.FRandRange(-550.0f, 550.0f), 0.0f);
		break;
	}
	case EAHTacticalIntent::FindCover:
	case EAHTacticalIntent::Reposition:
	{
		const float Side = (GetUniqueID() & 1) == 0 ? 1.0f : -1.0f;
		const FVector Lateral = FVector::CrossProduct(FVector::UpVector, Away).GetSafeNormal2D();
		Destination = Origin + Away * 420.0f + Lateral * Side * 320.0f;
		break;
	}
	default:
		break;
	}

	if (const UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
	{
		FNavLocation Projected;
		if (Navigation->ProjectPointToNavigation(Destination, Projected, FVector(450.0f, 450.0f, 500.0f)))
		{
			Destination = Projected.Location;
		}
	}
	return Destination;
}

float AAHCombatAIController::GetTacticalQueryInterval() const
{
	float Interval = 0.75f;
	if (const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		Interval = Platform->GetPerformanceProfile().EQSQueryUpdateInterval;
	}
	switch (TacticalDifficulty)
	{
	case EAHAITacticalDifficulty::Recruit: return Interval * 1.3f;
	case EAHAITacticalDifficulty::Veteran: return Interval * 0.86f;
	case EAHAITacticalDifficulty::Damnation: return Interval * 0.74f;
	default: return Interval;
	}
}

float AAHCombatAIController::GetTacticalQualityTolerance() const
{
	switch (TacticalDifficulty)
	{
	case EAHAITacticalDifficulty::Recruit: return 0.72f;
	case EAHAITacticalDifficulty::Veteran: return 0.90f;
	case EAHAITacticalDifficulty::Damnation: return 0.94f;
	default: return 0.82f;
	}
}

float AAHCombatAIController::GetFlankWillingness() const
{
	float Scale = 1.0f;
	switch (TacticalDifficulty)
	{
	case EAHAITacticalDifficulty::Recruit: Scale = 0.4f; break;
	case EAHAITacticalDifficulty::Veteran: Scale = 1.55f; break;
	case EAHAITacticalDifficulty::Damnation: Scale = 2.0f; break;
	default: break;
	}
	return FMath::Clamp(BaseFlankWillingness * Scale, 0.0f, 0.65f);
}

float AAHCombatAIController::GetGrenadeReactionDelay() const
{
	switch (TacticalDifficulty)
	{
	case EAHAITacticalDifficulty::Recruit: return 0.45f;
	case EAHAITacticalDifficulty::Veteran: return 0.16f;
	case EAHAITacticalDifficulty::Damnation: return 0.10f;
	default: return 0.28f;
	}
}

float AAHCombatAIController::GetEffectiveAccuracy() const
{
	float Offset = 0.0f;
	switch (TacticalDifficulty)
	{
	case EAHAITacticalDifficulty::Recruit: Offset = -0.12f; break;
	case EAHAITacticalDifficulty::Veteran: Offset = 0.05f; break;
	case EAHAITacticalDifficulty::Damnation: Offset = 0.09f; break;
	default: break;
	}
	return FMath::Clamp(Accuracy + Offset, 0.35f, 0.90f);
}

float AAHCombatAIController::GetBurstPauseScale() const
{
	switch (TacticalDifficulty)
	{
	case EAHAITacticalDifficulty::Recruit: return 1.20f;
	case EAHAITacticalDifficulty::Veteran: return 0.90f;
	case EAHAITacticalDifficulty::Damnation: return 0.82f;
	default: return 1.0f;
	}
}

bool AAHCombatAIController::IsExpensiveTacticalQueryAllowed(AActor* Target) const
{
	if (!Target || !Combatant.IsValid())
	{
		return true;
	}
	float MaximumDistance = 6500.0f;
	if (const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		MaximumDistance = Platform->GetPerformanceProfile().EQSExpensiveRepositionDistance;
	}
	return FVector::DistSquared2D(Target->GetActorLocation(), Combatant->GetActorLocation()) <= FMath::Square(MaximumDistance);
}

void AAHCombatAIController::UpdateAttackSlot()
{
	AH_SCOPE_PERFORMANCE(AITacticalDecisions, this);
	bAttackSlotEvaluated = true;
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
	AH_SCOPE_PERFORMANCE(Combat, this);
	if (!Combatant.IsValid())
	{
		return;
	}

	const float AimedSpread = (1.0f - GetEffectiveAccuracy()) * MaxAimErrorDegrees;
	const bool bInGrace = FirstContactTime >= 0.0f && Now - FirstContactTime < FirstContactGraceSeconds;
	const bool bAiming = bAttackSlotHeld && !bInGrace;
	Combatant->SetAimSpreadPenaltyDegrees(bAiming ? AimedSpread : FMath::Max(AimedSpread, SuppressionSpreadDegrees));
}

void AAHCombatAIController::UpdateBurstFire(float Now)
{
	AH_SCOPE_PERFORMANCE(Combat, this);
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
			NextShotTime = Now + FMath::FRandRange(MinBurstPause, FMath::Max(MinBurstPause, MaxBurstPause)) * GetBurstPauseScale();
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
	const float Now = GetWorld()->GetTimeSeconds();
	GrenadeThreatLocation = GrenadeLocation;
	GrenadeThreatRadius = FMath::Max(100.0f, Radius);
	GrenadeThreatExpiryTime = Now + 2.5f;
	GrenadeReactionReadyTime = Now + GetGrenadeReactionDelay();
	SetTacticalIntent(EAHTacticalIntent::EscapeGrenade, 2.5f);
	NextTacticalQueryTime = FMath::Min(NextTacticalQueryTime, GrenadeReactionReadyTime);
	if (UAHUpdateBudgetSubsystem* Budget = GetWorld()->GetSubsystem<UAHUpdateBudgetSubsystem>())
	{
		Budget->ProtectFromGrenade(Combatant.Get(), 2.5f);
	}
	if (Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
}

bool AAHCombatAIController::IsCurrentAttacker() const
{
	return bAttackSlotEvaluated && bAttackSlotHeld && CurrentTarget.IsValid();
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
		if (UAHTacticalPositionSubsystem::IsDrawEnabled() && bHasCachedTacticalLocation)
		{
			DrawDebugLine(GetWorld(), Combatant->GetActorLocation(), CachedTacticalLocation, FColor::Green, false, 0.2f, 0, 2.5f);
			DrawDebugSphere(GetWorld(), CachedTacticalLocation, 55.0f, 12, FColor::Green, false, 0.2f, 0, 2.0f);
			DrawDebugString(GetWorld(), Combatant->GetActorLocation() + FVector(0.0f, 0.0f, 120.0f),
				FString::Printf(TEXT("%s %.2f"), AHTacticalScoring::IntentToString(CurrentTacticalIntent), CachedTacticalScore),
				nullptr, FColor::Green, 0.2f, true);
		}
	}
#endif
}
