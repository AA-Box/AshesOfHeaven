#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "EngineUtils.h"
#include "NavigationSystem.h"
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
	Combatant = Cast<AAHCombatantCharacter>(InPawn);
	if (Combatant.IsValid())
	{
		Combatant->OnCombatantDeath.AddDynamic(this, &AAHCombatAIController::HandlePawnDeath);
	}
}

void AAHCombatAIController::HandlePawnDeath()
{
	StopMovement();
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
	}
	UpdateCombatBehavior(DeltaSeconds);
}

void AAHCombatAIController::UpdateTarget()
{
	if (CurrentTarget.IsValid() && Cast<AAHCombatantCharacter>(CurrentTarget.Get()) && !Cast<AAHCombatantCharacter>(CurrentTarget.Get())->IsCombatantDead())
	{
		return;
	}
	CurrentTarget = FindBestTarget();
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

void AAHCombatAIController::MoveWithFallback(const FVector& Destination, float DeltaSeconds)
{
	if (!Combatant.IsValid())
	{
		return;
	}

	const FVector Direction = (Destination - Combatant->GetActorLocation()).GetSafeNormal2D();
	if (!Direction.IsNearlyZero())
	{
		SetControlRotation(Direction.Rotation());
		Combatant->AddMovementInput(Direction, 1.0f);
		MoveToLocation(Destination, 80.0f, true);
	}
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
		const FVector ToTarget = (Target->GetActorLocation() - Combatant->GetActorLocation()).GetSafeNormal2D();
		SetControlRotation(ToTarget.Rotation());

		const float Distance = FVector::Dist(Target->GetActorLocation(), Combatant->GetActorLocation());
		if (Distance < 900.0f || (bPreferCover && Now >= NextRepositionTime))
		{
			if (Now >= NextRepositionTime)
			{
				NextRepositionTime = Now + FMath::FRandRange(2.5f, 5.0f);
				MoveWithFallback(ChooseCoverLocation(Target), DeltaSeconds);
			}
		}
		else if (Combatant->GetInventoryComponent() && Combatant->GetInventoryComponent()->GetCurrentWeapon())
		{
			if (!Combatant->GetInventoryComponent()->GetCurrentWeapon()->IsFiring())
			{
				Combatant->GetCombatComponent()->StartFire();
			}
		}
		return;
	}

	if (Combatant->GetCombatComponent())
	{
		Combatant->GetCombatComponent()->StopFire();
	}
	if (bHasSeenTarget && Now - LastSeenTime < 6.0f)
	{
		bInvestigating = true;
		MoveWithFallback(LastKnownLocation, DeltaSeconds);
	}
	else if (Target)
	{
		LastKnownLocation = Target->GetActorLocation();
		MoveWithFallback(LastKnownLocation + FVector(0.0f, FMath::FRandRange(-500.0f, 500.0f), 0.0f), DeltaSeconds);
	}
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
