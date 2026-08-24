#include "Performance/AHUpdateBudgetSubsystem.h"

#include "AshesOfHeaven.h"
#include "Performance/AHPerformanceStats.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "AIController.h"
#include "Components/ActorComponent.h"
#include "Components/AudioComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/MovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "HAL/IConsoleManager.h"
#include "NiagaraComponent.h"
#include "DrawDebugHelpers.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

namespace
{
	TAutoConsoleVariable<int32> CVarAHSignificanceEnabled(
		TEXT("ah.Perf.Significance.Enabled"), 1,
		TEXT("Enables significance-tier update budgets. Zero restores original tick state."), ECVF_Default);

	TAutoConsoleVariable<int32> CVarAHSignificanceDebug(
		TEXT("ah.Perf.Significance.Debug"), 0,
		TEXT("Draws combatant tiers and logs tier counts when non-zero."), ECVF_Cheat);

#if !UE_BUILD_SHIPPING
	// Command-line stress captures are process-scoped. A streaming world transition can
	// create a second subsystem, but should not silently start a second CSV capture.
	bool GStressCaptureClaimed = false;
#endif

	UAHUpdateBudgetSubsystem* FindActiveBudgetSubsystem()
	{
		if (!GEngine)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::GamePreview))
			{
				return World->GetSubsystem<UAHUpdateBudgetSubsystem>();
			}
		}
		return nullptr;
	}

	void DumpTickBudget(const TArray<FString>& Args)
	{
		if (UAHUpdateBudgetSubsystem* Subsystem = FindActiveBudgetSubsystem())
		{
			Subsystem->DumpToLog(Args.Contains(TEXT("reset")));
		}
		else
		{
			UE_LOG(LogAshesOfHeaven, Warning, TEXT("ah.Perf.TickBudget.Dump: no active game world"));
		}
	}

	FAutoConsoleCommand DumpTickBudgetCommand(
		TEXT("ah.Perf.TickBudget.Dump"),
		TEXT("Dumps active ticks, instrumented category cost, tier counts, and update rates. Pass reset to clear counters after dumping."),
		FConsoleCommandWithArgsDelegate::CreateStatic(&DumpTickBudget));

	EAHPerformanceCategory ClassifyTickObject(const UObject* Object)
	{
		const FString Name = Object ? Object->GetClass()->GetName() : FString();
		if (Object && (Object->IsA<UProjectileMovementComponent>() || Name.Contains(TEXT("Projectile")))) return EAHPerformanceCategory::Projectiles;
		if (Object && (Object->IsA<UMovementComponent>() || Name.Contains(TEXT("Movement")))) return EAHPerformanceCategory::Movement;
		if (Name.Contains(TEXT("Perception"))) return EAHPerformanceCategory::AIPerception;
		if (Object && (Object->IsA<AAIController>() || Name.Contains(TEXT("AI")) || Name.Contains(TEXT("Tactical")))) return EAHPerformanceCategory::AITacticalDecisions;
		if (Name.Contains(TEXT("Interaction"))) return EAHPerformanceCategory::Interaction;
		if (Object && (Object->IsA<UNiagaraComponent>() || Name.Contains(TEXT("Niagara")) || Name.Contains(TEXT("VFX")) || Name.Contains(TEXT("Particle")))) return EAHPerformanceCategory::VFXHelpers;
		if (Object && (Object->IsA<UWidgetComponent>() || Name.Contains(TEXT("Widget")) || Name.Contains(TEXT("HUD")))) return EAHPerformanceCategory::UI;
		if (Object && (Object->IsA<UAudioComponent>() || Name.Contains(TEXT("Audio")) || Name.Contains(TEXT("Sound")))) return EAHPerformanceCategory::Audio;
		if (Name.Contains(TEXT("Weapon")) || Name.Contains(TEXT("Combat")) || Name.Contains(TEXT("Health")) || Name.Contains(TEXT("Armor"))) return EAHPerformanceCategory::Combat;
		if (Name.Contains(TEXT("Presentation")) || Name.Contains(TEXT("Chapter")) || Name.Contains(TEXT("Vehicle")) || Name.Contains(TEXT("SkeletalMesh")) || Name.Contains(TEXT("Camera"))) return EAHPerformanceCategory::PresentationActors;
		return EAHPerformanceCategory::EnvironmentActors;
	}

	FColor TierColor(EAHSignificanceTier Tier)
	{
		switch (Tier)
		{
		case EAHSignificanceTier::Near: return FColor::Green;
		case EAHSignificanceTier::Mid: return FColor::Yellow;
		case EAHSignificanceTier::Far: return FColor(255, 128, 0);
		default: return FColor::Silver;
		}
	}
}

float FAHUpdateRateSet::GetInterval(EAHUpdateChannel Channel) const
{
	switch (Channel)
	{
	case EAHUpdateChannel::Perception: return Perception;
	case EAHUpdateChannel::TacticalDecision: return TacticalDecision;
	case EAHUpdateChannel::Movement: return Movement;
	case EAHUpdateChannel::Combat: return Combat;
	case EAHUpdateChannel::Aim: return Aim;
	case EAHUpdateChannel::CosmeticEffects: return CosmeticEffects;
	case EAHUpdateChannel::DistantBattlefieldSimulation: return DistantBattlefieldSimulation;
	default: return -1.0f;
	}
}

const FAHUpdateRateSet& FAHUpdateBudgetPolicy::GetRates(EAHSignificanceTier Tier) const
{
	switch (Tier)
	{
	case EAHSignificanceTier::Near: return NearRates;
	case EAHSignificanceTier::Mid: return MidRates;
	case EAHSignificanceTier::Far: return FarRates;
	default: return DormantRates;
	}
}

void UAHUpdateBudgetSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RefreshPolicy();
	FAHPerformanceStats::Reset();
	int32 CommandLineSignificance = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("AHSignificance="), CommandLineSignificance))
	{
		CVarAHSignificanceEnabled->Set(CommandLineSignificance != 0 ? 1 : 0, ECVF_SetByCommandline);
	}
	FParse::Value(FCommandLine::Get(), TEXT("AHTickBudgetStress="), StressCombatantCount);
	FParse::Value(FCommandLine::Get(), TEXT("AHTickBudgetCaptureFrames="), StressCaptureFrameLimit);
	StressCaptureFrameLimit = FMath::Max(1, StressCaptureFrameLimit);
}

void UAHUpdateBudgetSubsystem::Deinitialize()
{
	if (bStressCaptureStarted && !bStressCaptureCompleted && GEngine && GetWorld())
	{
		GEngine->Exec(GetWorld(), TEXT("csvprofile stop"));
	}
	if (FParse::Param(FCommandLine::Get(), TEXT("AHTickBudgetCapture")) && GetWorld() && GetWorld()->HasBegunPlay())
	{
		DumpToLog(false);
	}
	for (TPair<TWeakObjectPtr<AActor>, FEntry>& Pair : Entries)
	{
		RestoreOriginalTickState(Pair.Value);
	}
	Entries.Reset();
	Super::Deinitialize();
}

bool UAHUpdateBudgetSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::GamePreview;
}

TStatId UAHUpdateBudgetSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAHUpdateBudgetSubsystem, STATGROUP_Tickables);
}

bool UAHUpdateBudgetSubsystem::IsEnabled()
{
	return CVarAHSignificanceEnabled.GetValueOnGameThread() != 0;
}

bool UAHUpdateBudgetSubsystem::IsDebugEnabled()
{
	return CVarAHSignificanceDebug.GetValueOnGameThread() != 0;
}

FAHUpdateBudgetPolicy UAHUpdateBudgetSubsystem::BuildPolicy(const FAHPerformanceProfile& PerformanceProfile, bool bMobile)
{
	FAHUpdateBudgetPolicy Result;
	Result.MaxNearActors = FMath::Max(1, PerformanceProfile.MaxActiveCombatants);
	Result.MaxMidActors = FMath::Max(1, PerformanceProfile.MaxMidDistanceActors);
	Result.MaxFarActors = FMath::Max(1, PerformanceProfile.MaxDistantSimulationActors);
	Result.EvaluationInterval = FMath::Max(0.05f, PerformanceProfile.MidDistanceTickInterval);

	Result.NearDistance = bMobile ? 1800.0f : 2500.0f;
	Result.MidDistance = bMobile ? 4200.0f : 6000.0f;
	Result.FarDistance = bMobile ? 8000.0f : 12000.0f;

	Result.NearRates.Perception = 0.0f;
	Result.NearRates.TacticalDecision = 0.25f;
	Result.NearRates.Movement = 0.0f;
	Result.NearRates.Combat = 0.0f;
	Result.NearRates.Aim = 0.0f;
	Result.NearRates.CosmeticEffects = 0.0f;

	const float MidBase = FMath::Max(0.05f, PerformanceProfile.MidDistanceTickInterval);
	Result.MidRates.Perception = MidBase;
	Result.MidRates.TacticalDecision = MidBase * 2.5f;
	Result.MidRates.Movement = MidBase;
	Result.MidRates.Combat = MidBase;
	Result.MidRates.Aim = MidBase * 0.5f;
	Result.MidRates.CosmeticEffects = MidBase * (bMobile ? 2.0f : 1.0f);

	const float FarBase = MidBase * (bMobile ? 4.0f : 3.0f);
	Result.FarRates.Perception = FarBase * 2.0f;
	Result.FarRates.TacticalDecision = -1.0f;
	Result.FarRates.Movement = -1.0f;
	Result.FarRates.Combat = -1.0f;
	Result.FarRates.Aim = -1.0f;
	Result.FarRates.CosmeticEffects = FarBase;
	Result.FarRates.DistantBattlefieldSimulation = FarBase;

	Result.DormantRates.Perception = -1.0f;
	Result.DormantRates.TacticalDecision = -1.0f;
	Result.DormantRates.Movement = -1.0f;
	Result.DormantRates.Combat = -1.0f;
	Result.DormantRates.Aim = -1.0f;
	Result.DormantRates.CosmeticEffects = -1.0f;
	Result.DormantRates.DistantBattlefieldSimulation = -1.0f;
	return Result;
}

EAHSignificanceTier UAHUpdateBudgetSubsystem::EvaluateTier(const FAHSignificanceInput& Input, const FAHUpdateBudgetPolicy& InPolicy)
{
	if (Input.bCurrentAttacker || Input.bGrenadeThreat || Input.bObjectiveRelevant || Input.bNarrativeRelevant)
	{
		return EAHSignificanceTier::Near;
	}

	EAHSignificanceTier Tier = EAHSignificanceTier::Dormant;
	if (Input.DistanceSquared <= FMath::Square(InPolicy.NearDistance)) Tier = EAHSignificanceTier::Near;
	else if (Input.DistanceSquared <= FMath::Square(InPolicy.MidDistance)) Tier = EAHSignificanceTier::Mid;
	else if (Input.DistanceSquared <= FMath::Square(InPolicy.FarDistance)) Tier = EAHSignificanceTier::Far;

	if ((Input.bVisible || Input.bCombatRelevant) && Tier > EAHSignificanceTier::Mid)
	{
		Tier = EAHSignificanceTier::Mid;
	}
	if (Input.bActiveEncounterMember && Tier == EAHSignificanceTier::Dormant)
	{
		Tier = EAHSignificanceTier::Far;
	}
	return Tier;
}

bool UAHUpdateBudgetSubsystem::ConsumeInterval(double Now, float Interval, double& InOutNextDue, double& InOutLastUpdate, float& OutUpdateDelta)
{
	OutUpdateDelta = 0.0f;
	if (Interval < 0.0f)
	{
		return false;
	}
	if (Interval <= SMALL_NUMBER)
	{
		OutUpdateDelta = InOutLastUpdate > 0.0 ? static_cast<float>(Now - InOutLastUpdate) : 0.0f;
		InOutLastUpdate = Now;
		InOutNextDue = Now;
		return true;
	}
	if (InOutNextDue <= 0.0)
	{
		InOutNextDue = Now;
	}
	if (Now + UE_DOUBLE_SMALL_NUMBER < InOutNextDue)
	{
		return false;
	}

	OutUpdateDelta = InOutLastUpdate > 0.0 ? static_cast<float>(Now - InOutLastUpdate) : Interval;
	InOutLastUpdate = Now;
	do
	{
		InOutNextDue += Interval;
	}
	while (InOutNextDue <= Now + UE_DOUBLE_SMALL_NUMBER);
	return true;
}

const TCHAR* UAHUpdateBudgetSubsystem::TierName(EAHSignificanceTier Tier)
{
	switch (Tier)
	{
	case EAHSignificanceTier::Near: return TEXT("Near");
	case EAHSignificanceTier::Mid: return TEXT("Mid");
	case EAHSignificanceTier::Far: return TEXT("Far");
	default: return TEXT("Dormant");
	}
}

void UAHUpdateBudgetSubsystem::RegisterCombatant(AActor* Subject, AActor* TickOwner, bool bActiveEncounterMember)
{
	if (!IsValid(Subject) || !IsValid(TickOwner))
	{
		return;
	}

	FEntry& Entry = Entries.FindOrAdd(Subject);
	if (!Entry.Subject.IsValid())
	{
		Entry.Subject = Subject;
		Entry.TickOwner = TickOwner;
		Entry.bOriginalSubjectTickEnabled = Subject->IsActorTickEnabled();
		Entry.bOriginalTickOwnerTickEnabled = TickOwner->IsActorTickEnabled();
		Entry.OriginalTickOwnerInterval = TickOwner->PrimaryActorTick.TickInterval;
	}
	Entry.bActiveEncounterMember = bActiveEncounterMember;
	ResetSchedules(Entry, GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0);
}

void UAHUpdateBudgetSubsystem::UnregisterCombatant(AActor* Subject)
{
	if (FEntry* Entry = Entries.Find(Subject))
	{
		RestoreOriginalTickState(*Entry);
		Entries.Remove(Subject);
	}
}

void UAHUpdateBudgetSubsystem::SetCombatState(AActor* Subject, bool bCombatRelevant, bool bCurrentAttacker)
{
	if (FEntry* Entry = Entries.Find(Subject))
	{
		Entry->bCombatRelevant = bCombatRelevant;
		Entry->bCurrentAttacker = bCurrentAttacker;
		if (bCurrentAttacker && Entry->Tier != EAHSignificanceTier::Near)
		{
			ApplyTier(*Entry, EAHSignificanceTier::Near);
		}
	}
}

void UAHUpdateBudgetSubsystem::ProtectFromGrenade(AActor* Subject, float DurationSeconds)
{
	if (FEntry* Entry = Entries.Find(Subject))
	{
		Entry->GrenadeProtectionEndTime = FMath::Max(Entry->GrenadeProtectionEndTime,
			GetWorld()->GetTimeSeconds() + FMath::Max(0.0f, DurationSeconds));
		ApplyTier(*Entry, EAHSignificanceTier::Near);
	}
}

void UAHUpdateBudgetSubsystem::SetActiveEncounterMember(AActor* Subject, bool bActiveEncounterMember)
{
	if (FEntry* Entry = Entries.Find(Subject))
	{
		Entry->bActiveEncounterMember = bActiveEncounterMember;
	}
}

EAHSignificanceTier UAHUpdateBudgetSubsystem::GetTier(const AActor* Subject) const
{
	if (!IsEnabled())
	{
		return EAHSignificanceTier::Near;
	}
	if (const FEntry* Entry = Entries.Find(Subject))
	{
		return Entry->Tier;
	}
	return EAHSignificanceTier::Near;
}

bool UAHUpdateBudgetSubsystem::IsUpdateDue(AActor* Subject, EAHUpdateChannel Channel, float WorldTime, float& OutUpdateDelta)
{
	if (!IsEnabled())
	{
		OutUpdateDelta = 0.0f;
		return true;
	}
	FEntry* Entry = Entries.Find(Subject);
	if (!Entry)
	{
		OutUpdateDelta = 0.0f;
		return true;
	}
	const int32 ChannelIndex = static_cast<int32>(Channel);
	return ConsumeInterval(WorldTime, Policy.GetRates(Entry->Tier).GetInterval(Channel),
		Entry->NextUpdateTimes[ChannelIndex], Entry->LastUpdateTimes[ChannelIndex], OutUpdateDelta);
}

void UAHUpdateBudgetSubsystem::RefreshPolicy()
{
	if (const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		Policy = BuildPolicy(Platform->GetPerformanceProfile(), Platform->GetCapabilities().bIsMobile);
	}
	else
	{
		Policy = BuildPolicy(FAHPerformanceProfile(), false);
	}
}

FAHSignificanceInput UAHUpdateBudgetSubsystem::BuildInput(const FEntry& Entry, const APawn* PlayerPawn, double Now) const
{
	FAHSignificanceInput Input;
	const AActor* Subject = Entry.Subject.Get();
	Input.DistanceSquared = Subject && PlayerPawn ? FVector::DistSquared(Subject->GetActorLocation(), PlayerPawn->GetActorLocation()) : BIG_NUMBER;
	Input.bVisible = Subject && Subject->WasRecentlyRendered(0.25f);
	Input.bActiveEncounterMember = Entry.bActiveEncounterMember || (Subject
		&& (Subject->ActorHasTag(TEXT("ActiveEncounter")) || Subject->ActorHasTag(TEXT("AH.DirectedEncounter"))));
	Input.bCombatRelevant = Entry.bCombatRelevant;
	Input.bCurrentAttacker = Entry.bCurrentAttacker;
	Input.bGrenadeThreat = Now < Entry.GrenadeProtectionEndTime;
	Input.bObjectiveRelevant = Subject && (Subject->ActorHasTag(TEXT("Objective")) || Subject->ActorHasTag(TEXT("ObjectiveRelevant")));
	Input.bNarrativeRelevant = Subject && (Subject->ActorHasTag(TEXT("Narrative")) || Subject->ActorHasTag(TEXT("NarrativeRelevant")));
	return Input;
}

void UAHUpdateBudgetSubsystem::Tick(float DeltaTime)
{
	AH_SCOPE_PERFORMANCE(AITacticalDecisions, this);
	FAHPerformanceStats::AdvanceFrame();
	TryStartStressCapture();
	if (bStressCaptureStarted && !bStressCaptureCompleted && ++StressCaptureFrameCount >= StressCaptureFrameLimit)
	{
		DumpToLog(false);
		bStressCaptureCompleted = true;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;

	if (!IsEnabled())
	{
		if (bAppliedEnabledPolicy)
		{
			for (TPair<TWeakObjectPtr<AActor>, FEntry>& Pair : Entries)
			{
				RestoreOriginalTickState(Pair.Value);
				Pair.Value.Tier = EAHSignificanceTier::Near;
			}
			bAppliedEnabledPolicy = false;
		}
		return;
	}

	bAppliedEnabledPolicy = true;
	if (Now >= NextEvaluationTime)
	{
		RefreshPolicy();
		RefreshTiers();
		NextEvaluationTime = Now + Policy.EvaluationInterval;
		if (IsDebugEnabled())
		{
			DrawDebugState();
		}
	}
}

void UAHUpdateBudgetSubsystem::TryStartStressCapture()
{
#if !UE_BUILD_SHIPPING
	if (bStressCaptureStarted || GStressCaptureClaimed || StressCombatantCount <= 0 || !GetWorld())
	{
		return;
	}
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	if (!PlayerPawn)
	{
		return;
	}

	const FVector Origin = PlayerPawn->GetActorLocation();
	for (int32 Index = 0; Index < StressCombatantCount; ++Index)
	{
		const int32 Band = (Index * 3) / FMath::Max(1, StressCombatantCount);
		const float Radius = Band == 0 ? 1400.0f : (Band == 1 ? 4200.0f : 9000.0f);
		const float AngleRadians = UE_TWO_PI * static_cast<float>(Index) / FMath::Max(1, StressCombatantCount);
		const FVector Location = Origin + FVector(FMath::Cos(AngleRadians), FMath::Sin(AngleRadians), 0.0f) * Radius + FVector(0.0f, 0.0f, 100.0f);
		FActorSpawnParameters Parameters;
		Parameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AAHVeilPilgrimCharacter* Combatant = GetWorld()->SpawnActor<AAHVeilPilgrimCharacter>(
			AAHVeilPilgrimCharacter::StaticClass(), Location, FRotator::ZeroRotator, Parameters))
		{
			Combatant->Tags.AddUnique(TEXT("ActiveEncounter"));
			if (!Combatant->GetController())
			{
				Combatant->AIControllerClass = AAHCombatAIController::StaticClass();
				Combatant->SpawnDefaultController();
			}
		}
	}

	FAHPerformanceStats::Reset();
	GStressCaptureClaimed = true;
	if (GEngine)
	{
		GEngine->Exec(GetWorld(), *FString::Printf(TEXT("csvprofile frames=%d"), StressCaptureFrameLimit));
	}
	bStressCaptureStarted = true;
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[TickBudget] stress capture started combatants=%d"), StressCombatantCount);
#endif
}

void UAHUpdateBudgetSubsystem::RefreshTiers()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
	const APawn* PlayerPawn = PlayerController ? PlayerController->GetPawn() : nullptr;
	TArray<FEntry*> RankedEntries;

	for (auto It = Entries.CreateIterator(); It; ++It)
	{
		FEntry& Entry = It.Value();
		if (!Entry.Subject.IsValid() || !Entry.TickOwner.IsValid())
		{
			It.RemoveCurrent();
			continue;
		}
		RankedEntries.Add(&Entry);
	}

	RankedEntries.Sort([this, PlayerPawn, Now](const FEntry& A, const FEntry& B)
	{
		const FAHSignificanceInput AInput = BuildInput(A, PlayerPawn, Now);
		const FAHSignificanceInput BInput = BuildInput(B, PlayerPawn, Now);
		const auto Score = [](const FAHSignificanceInput& Input)
		{
			const int32 Protected = (Input.bCurrentAttacker || Input.bGrenadeThreat || Input.bObjectiveRelevant || Input.bNarrativeRelevant) ? 1000000 : 0;
			const int32 Relevance = (Input.bCombatRelevant ? 100000 : 0) + (Input.bActiveEncounterMember ? 10000 : 0) + (Input.bVisible ? 1000 : 0);
			return static_cast<double>(Protected + Relevance) - static_cast<double>(Input.DistanceSquared) * 0.000001;
		};
		const double AScore = Score(AInput);
		const double BScore = Score(BInput);
		if (!FMath::IsNearlyEqual(AScore, BScore)) return AScore > BScore;
		return A.Subject->GetPathName() < B.Subject->GetPathName();
	});

	int32 NearCount = 0;
	int32 MidCount = 0;
	int32 FarCount = 0;
	for (FEntry* Entry : RankedEntries)
	{
		const FAHSignificanceInput Input = BuildInput(*Entry, PlayerPawn, Now);
		EAHSignificanceTier Tier = EvaluateTier(Input, Policy);
		const bool bNeverThrottle = Input.bCurrentAttacker || Input.bGrenadeThreat || Input.bObjectiveRelevant || Input.bNarrativeRelevant;
		if (Tier == EAHSignificanceTier::Near && !bNeverThrottle && NearCount >= Policy.MaxNearActors) Tier = EAHSignificanceTier::Mid;
		if (Tier == EAHSignificanceTier::Mid && MidCount >= Policy.MaxMidActors) Tier = EAHSignificanceTier::Far;
		if (Tier == EAHSignificanceTier::Far && FarCount >= Policy.MaxFarActors && !Input.bActiveEncounterMember && !Input.bCombatRelevant) Tier = EAHSignificanceTier::Dormant;

		switch (Tier)
		{
		case EAHSignificanceTier::Near: ++NearCount; break;
		case EAHSignificanceTier::Mid: ++MidCount; break;
		case EAHSignificanceTier::Far: ++FarCount; break;
		default: break;
		}
		ApplyTier(*Entry, Tier);
	}
}

void UAHUpdateBudgetSubsystem::RestoreOriginalTickState(FEntry& Entry) const
{
	RestoreSubjectComponentTicks(Entry);
	if (AActor* Subject = Entry.Subject.Get())
	{
		Subject->SetActorTickEnabled(Entry.bOriginalSubjectTickEnabled);
	}
	if (AActor* TickOwner = Entry.TickOwner.Get())
	{
		TickOwner->PrimaryActorTick.TickInterval = Entry.OriginalTickOwnerInterval;
		TickOwner->SetActorTickEnabled(Entry.bOriginalTickOwnerTickEnabled);
	}
}

void UAHUpdateBudgetSubsystem::SuspendSubjectComponentTicks(FEntry& Entry) const
{
	if (Entry.bSubjectComponentsSuspended)
	{
		return;
	}
	AActor* Subject = Entry.Subject.Get();
	if (!Subject)
	{
		return;
	}

	Entry.SuspendedComponentTicks.Reset();
	TInlineComponentArray<UActorComponent*> Components(Subject);
	for (UActorComponent* Component : Components)
	{
		if (!Component || !Component->PrimaryComponentTick.bCanEverTick)
		{
			continue;
		}
		FEntry::FComponentTickState& State = Entry.SuspendedComponentTicks.AddDefaulted_GetRef();
		State.Component = Component;
		State.bWasTickEnabled = Component->IsComponentTickEnabled();
		Component->SetComponentTickEnabled(false);
	}
	Entry.bSubjectComponentsSuspended = true;
}

void UAHUpdateBudgetSubsystem::RestoreSubjectComponentTicks(FEntry& Entry) const
{
	if (!Entry.bSubjectComponentsSuspended)
	{
		return;
	}
	for (const FEntry::FComponentTickState& State : Entry.SuspendedComponentTicks)
	{
		if (UActorComponent* Component = State.Component.Get())
		{
			Component->SetComponentTickEnabled(State.bWasTickEnabled);
		}
	}
	Entry.SuspendedComponentTicks.Reset();
	Entry.bSubjectComponentsSuspended = false;
}

void UAHUpdateBudgetSubsystem::ApplyTier(FEntry& Entry, EAHSignificanceTier NewTier)
{
	AActor* Subject = Entry.Subject.Get();
	AActor* TickOwner = Entry.TickOwner.Get();
	if (!Subject || !TickOwner)
	{
		return;
	}
	const bool bChanged = Entry.Tier != NewTier;
	Entry.Tier = NewTier;

	if (NewTier == EAHSignificanceTier::Far || NewTier == EAHSignificanceTier::Dormant)
	{
		if (AAIController* AIController = Cast<AAIController>(TickOwner))
		{
			AIController->StopMovement();
		}
		if (AAHCombatantCharacter* Combatant = Cast<AAHCombatantCharacter>(Subject))
		{
			if (UAHCombatComponent* Combat = Combatant->GetCombatComponent())
			{
				Combat->StopFire();
			}
		}
		SuspendSubjectComponentTicks(Entry);
		if (NewTier == EAHSignificanceTier::Dormant)
		{
			TickOwner->SetActorTickEnabled(false);
			Subject->SetActorTickEnabled(false);
		}
		else
		{
			Subject->SetActorTickEnabled(false);
			TickOwner->SetActorTickEnabled(Entry.bOriginalTickOwnerTickEnabled);
			TickOwner->PrimaryActorTick.TickInterval = Policy.FarRates.DistantBattlefieldSimulation;
		}
	}
	else
	{
		RestoreSubjectComponentTicks(Entry);
		Subject->SetActorTickEnabled(Entry.bOriginalSubjectTickEnabled);
		TickOwner->SetActorTickEnabled(Entry.bOriginalTickOwnerTickEnabled);
		const FAHUpdateRateSet& Rates = Policy.GetRates(NewTier);
		float MinimumInterval = BIG_NUMBER;
		for (int32 Index = 0; Index < static_cast<int32>(EAHUpdateChannel::Count); ++Index)
		{
			const float Interval = Rates.GetInterval(static_cast<EAHUpdateChannel>(Index));
			if (Interval >= 0.0f) MinimumInterval = FMath::Min(MinimumInterval, Interval);
		}
		TickOwner->PrimaryActorTick.TickInterval = MinimumInterval == BIG_NUMBER ? Entry.OriginalTickOwnerInterval : MinimumInterval;
	}

	if (bChanged)
	{
		ResetSchedules(Entry, GetWorld()->GetTimeSeconds());
	}
}

void UAHUpdateBudgetSubsystem::ResetSchedules(FEntry& Entry, double Now)
{
	const uint32 Hash = GetTypeHash(Entry.Subject.IsValid() ? Entry.Subject->GetPathName() : FString());
	const double Phase = static_cast<double>(Hash % 997) / 997.0;
	const FAHUpdateRateSet& Rates = Policy.GetRates(Entry.Tier);
	for (int32 Index = 0; Index < static_cast<int32>(EAHUpdateChannel::Count); ++Index)
	{
		const float Interval = Rates.GetInterval(static_cast<EAHUpdateChannel>(Index));
		Entry.NextUpdateTimes[Index] = Interval > 0.0f && Entry.Tier != EAHSignificanceTier::Near ? Now + Interval * Phase : Now;
		Entry.LastUpdateTimes[Index] = Now;
	}
}

FString UAHUpdateBudgetSubsystem::BuildTickAudit() const
{
	int32 ActorCounts[static_cast<int32>(EAHPerformanceCategory::Count)] = {};
	int32 ComponentCounts[static_cast<int32>(EAHPerformanceCategory::Count)] = {};
	TMap<FName, int32> ClassCounts;
	int32 TotalActors = 0;
	int32 TotalComponents = 0;

	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Actor = *It;
		if (Actor->PrimaryActorTick.IsTickFunctionEnabled())
		{
			++ActorCounts[static_cast<int32>(ClassifyTickObject(Actor))];
			++ClassCounts.FindOrAdd(Actor->GetClass()->GetFName());
			++TotalActors;
		}
		TInlineComponentArray<UActorComponent*> Components(Actor);
		for (UActorComponent* Component : Components)
		{
			if (Component && Component->PrimaryComponentTick.IsTickFunctionEnabled())
			{
				++ComponentCounts[static_cast<int32>(ClassifyTickObject(Component))];
				++ClassCounts.FindOrAdd(Component->GetClass()->GetFName());
				++TotalComponents;
			}
		}
	}

	FString Result = FString::Printf(TEXT("Active primary ticks: actors=%d components=%d total=%d\n"), TotalActors, TotalComponents, TotalActors + TotalComponents);
	for (int32 Index = 0; Index < static_cast<int32>(EAHPerformanceCategory::Count); ++Index)
	{
		Result += FString::Printf(TEXT("  %-24s actors=%4d components=%4d\n"),
			FAHPerformanceStats::CategoryName(static_cast<EAHPerformanceCategory>(Index)), ActorCounts[Index], ComponentCounts[Index]);
	}

	TArray<TPair<FName, int32>> Classes;
	for (const TPair<FName, int32>& Pair : ClassCounts) Classes.Add(Pair);
	Classes.Sort([](const auto& A, const auto& B) { return A.Value > B.Value; });
	Result += TEXT("  Most numerous ticking classes:\n");
	for (int32 Index = 0; Index < FMath::Min(Classes.Num(), 12); ++Index)
	{
		Result += FString::Printf(TEXT("    %-36s %d\n"), *Classes[Index].Key.ToString(), Classes[Index].Value);
	}
	return Result;
}

void UAHUpdateBudgetSubsystem::DumpToLog(bool bResetCountersAfterDump) const
{
	int32 TierCounts[4] = {};
	for (const TPair<TWeakObjectPtr<AActor>, FEntry>& Pair : Entries)
	{
		if (Pair.Value.Subject.IsValid()) ++TierCounts[static_cast<int32>(Pair.Value.Tier)];
	}
	const FString TierReport = FString::Printf(
		TEXT("Significance enabled=%s registered=%d Near=%d Mid=%d Far=%d Dormant=%d\n")
		TEXT("Rates seconds: Near[P=%.3f T=%.3f M=%.3f C=%.3f A=%.3f FX=%.3f] ")
		TEXT("Mid[P=%.3f T=%.3f M=%.3f C=%.3f A=%.3f FX=%.3f] ")
		TEXT("Far[P=%.3f T=off M=off C=off A=off FX=%.3f Sim=%.3f]\n"),
		IsEnabled() ? TEXT("true") : TEXT("false"), Entries.Num(), TierCounts[0], TierCounts[1], TierCounts[2], TierCounts[3],
		Policy.NearRates.Perception, Policy.NearRates.TacticalDecision, Policy.NearRates.Movement, Policy.NearRates.Combat, Policy.NearRates.Aim, Policy.NearRates.CosmeticEffects,
		Policy.MidRates.Perception, Policy.MidRates.TacticalDecision, Policy.MidRates.Movement, Policy.MidRates.Combat, Policy.MidRates.Aim, Policy.MidRates.CosmeticEffects,
		Policy.FarRates.Perception, Policy.FarRates.CosmeticEffects, Policy.FarRates.DistantBattlefieldSimulation);
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[TickBudget]\n%s%s%s"), *TierReport, *BuildTickAudit(), *FAHPerformanceStats::BuildReport());
	if (bResetCountersAfterDump)
	{
		FAHPerformanceStats::Reset();
	}
}

void UAHUpdateBudgetSubsystem::DrawDebugState() const
{
	int32 TierCounts[4] = {};
	for (const TPair<TWeakObjectPtr<AActor>, FEntry>& Pair : Entries)
	{
		const FEntry& Entry = Pair.Value;
		if (AActor* Subject = Entry.Subject.Get())
		{
			++TierCounts[static_cast<int32>(Entry.Tier)];
			DrawDebugString(GetWorld(), Subject->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f),
				TierName(Entry.Tier), nullptr, TierColor(Entry.Tier), Policy.EvaluationInterval * 1.5f, false);
		}
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(0xA45161, Policy.EvaluationInterval * 1.5f, FColor::White,
			FString::Printf(
				TEXT("Tick budget  N:%d  M:%d  F:%d  D:%d\n")
				TEXT("Near T:%.2f P:%.2f | Mid T:%.2f P:%.2f FX:%.2f | Far Sim:%.2f FX:%.2f"),
				TierCounts[0], TierCounts[1], TierCounts[2], TierCounts[3],
				Policy.NearRates.TacticalDecision, Policy.NearRates.Perception,
				Policy.MidRates.TacticalDecision, Policy.MidRates.Perception, Policy.MidRates.CosmeticEffects,
				Policy.FarRates.DistantBattlefieldSimulation, Policy.FarRates.CosmeticEffects));
	}
}
