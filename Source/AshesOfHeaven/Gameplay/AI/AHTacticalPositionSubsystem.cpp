#include "Gameplay/AI/AHTacticalPositionSubsystem.h"

#include "AshesOfHeaven.h"
#include "DrawDebugHelpers.h"
#include "EnvironmentQuery/Contexts/EnvQueryContext_Querier.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryOption.h"
#include "EnvironmentQuery/Tests/EnvQueryTest_Pathfinding.h"
#include "Gameplay/AI/AHEnvQueryGenerator_TacticalPositions.h"
#include "Gameplay/AI/AHEnvQueryTest_TacticalPosition.h"
#include "HAL/IConsoleManager.h"
#include "Platform/AHPlatformManagerSubsystem.h"

namespace
{
	TAutoConsoleVariable<int32> CVarAIEQSDebug(
		TEXT("ah.AI.EQS.Debug"),
		0,
		TEXT("Log tactical intent, score, rejection summary, and fallback use."),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarAIEQSDraw(
		TEXT("ah.AI.EQS.Draw"),
		0,
		TEXT("Draw selected tactical positions and owner-to-goal lines."),
		ECVF_Cheat);

	TAutoConsoleVariable<int32> CVarAIEQSDisable(
		TEXT("ah.AI.EQS.Disable"),
		0,
		TEXT("Disable tactical EQS execution while preserving safe fallback movement."),
		ECVF_Cheat);

	constexpr int32 QueryKindCount = static_cast<int32>(EAHTacticalQueryKind::Reinforcement) + 1;
	constexpr double ReservationLifetimeSeconds = 8.0;
}

void UAHTacticalPositionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	QueryTemplates.SetNum(QueryKindCount);
	for (int32 Index = 0; Index < QueryKindCount; ++Index)
	{
		QueryTemplates[Index] = BuildQueryTemplate(static_cast<EAHTacticalQueryKind>(Index));
	}
}

void UAHTacticalPositionSubsystem::Deinitialize()
{
	if (UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(GetWorld()))
	{
		TSet<TWeakObjectPtr<AActor>> Owners;
		for (const TPair<int32, FActiveQuery>& Pair : ActiveQueries)
		{
			Owners.Add(Pair.Value.Request.Querier);
		}
		for (const TWeakObjectPtr<AActor>& Owner : Owners)
		{
			if (Owner.IsValid())
			{
				QueryManager->SilentlyRemoveAllQueriesByQuerier(*Owner.Get());
			}
		}
	}

	for (const TPair<int32, FActiveQuery>& Pair : ActiveQueries)
	{
		if (Pair.Value.bPlatformSlotAcquired)
		{
			ReleasePlatformSlot();
		}
	}
	ActiveQueries.Reset();
	RequestsByOwner.Reset();
	Reservations.Reset();
	RecentPositions.Reset();
	RejectionCounts.Reset();
	TimedOutQueries.Reset();
	QueryTemplates.Reset();
	Super::Deinitialize();
}

bool UAHTacticalPositionSubsystem::DoesSupportWorldType(EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor || WorldType == EWorldType::GamePreview;
}

TStatId UAHTacticalPositionSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAHTacticalPositionSubsystem, STATGROUP_Tickables);
}

bool UAHTacticalPositionSubsystem::IsDebugEnabled()
{
	return CVarAIEQSDebug.GetValueOnGameThread() != 0;
}

bool UAHTacticalPositionSubsystem::IsDrawEnabled()
{
	return CVarAIEQSDraw.GetValueOnGameThread() != 0;
}

bool UAHTacticalPositionSubsystem::IsDisabled()
{
	return CVarAIEQSDisable.GetValueOnGameThread() != 0;
}

UEnvQuery* UAHTacticalPositionSubsystem::BuildQueryTemplate(EAHTacticalQueryKind QueryKind)
{
	const FString KindName = StaticEnum<EAHTacticalQueryKind>()->GetNameStringByValue(static_cast<int64>(QueryKind));
	const FName QueryName(*FString::Printf(TEXT("AH_EQS_%s"), *KindName));
	UEnvQuery* Query = NewObject<UEnvQuery>(this, QueryName);
	UEnvQueryOption* Option = NewObject<UEnvQueryOption>(Query);
	UAHEnvQueryGenerator_TacticalPositions* Generator = NewObject<UAHEnvQueryGenerator_TacticalPositions>(Option);
	Generator->QueryKind = QueryKind;
	Option->Generator = Generator;

	UEnvQueryTest_Pathfinding* PathTest = NewObject<UEnvQueryTest_Pathfinding>(Option);
	PathTest->TestPurpose = EEnvTestPurpose::FilterAndScore;
	PathTest->FilterType = EEnvTestFilterType::Maximum;
	PathTest->TestMode = EEnvTestPathfinding::PathCost;
	PathTest->Context = UEnvQueryContext_Querier::StaticClass();
	PathTest->PathFromContext.DefaultValue = true;
	PathTest->SkipUnreachable.DefaultValue = true;
	PathTest->FloatValueMax.DefaultValue = 8000.0f;
	PathTest->ScoringEquation = EEnvTestScoreEquation::InverseLinear;
	PathTest->ScoringFactor.DefaultValue = 0.35f;
	PathTest->SetWorkOnFloatValues(true);
	Option->Tests.Add(PathTest);

	UAHEnvQueryTest_TacticalPosition* TacticalTest = NewObject<UAHEnvQueryTest_TacticalPosition>(Option);
	TacticalTest->QueryKind = QueryKind;
	Option->Tests.Add(TacticalTest);
	Query->GetOptionsMutable().Add(Option);
	return Query;
}

UEnvQuery* UAHTacticalPositionSubsystem::GetQueryTemplate(EAHTacticalQueryKind QueryKind) const
{
	const int32 Index = static_cast<int32>(QueryKind);
	return QueryTemplates.IsValidIndex(Index) ? QueryTemplates[Index] : nullptr;
}

int32 UAHTacticalPositionSubsystem::RequestPosition(const FAHTacticalPositionRequest& Request, FAHTacticalQueryFinished Completion)
{
	AActor* Querier = Request.Querier.Get();
	if (!Querier)
	{
		CompleteWithFallback(Request, Completion, TEXT("InvalidQuerier"));
		return INDEX_NONE;
	}
	if (RequestsByOwner.Contains(Querier))
	{
		CompleteWithFallback(Request, Completion, TEXT("QueryAlreadyRunning"));
		return INDEX_NONE;
	}
	if (IsDisabled())
	{
		CompleteWithFallback(Request, Completion, TEXT("Disabled"));
		return INDEX_NONE;
	}

	UEnvQuery* Query = GetQueryTemplate(Request.QueryKind);
	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(GetWorld());
	if (!Query || !QueryManager)
	{
		CompleteWithFallback(Request, Completion, TEXT("EQSUnavailable"));
		return INDEX_NONE;
	}

	UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	const bool bAcquiredSlot = !Platform || Platform->TryAcquireEQSQuerySlot();
	if (!bAcquiredSlot)
	{
		CompleteWithFallback(Request, Completion, TEXT("PlatformBudget"));
		return INDEX_NONE;
	}

	RequestsByOwner.Add(Querier, Request);
	FEnvQueryRequest QueryRequest(Query, Querier);
	const int32 QueryId = QueryRequest.Execute(EEnvQueryRunMode::AllMatching, this, &UAHTacticalPositionSubsystem::HandleQueryFinished);
	if (QueryId == INDEX_NONE)
	{
		RequestsByOwner.Remove(Querier);
		if (Platform)
		{
			Platform->ReleaseEQSQuerySlot();
		}
		CompleteWithFallback(Request, Completion, TEXT("FailedToStart"));
		return INDEX_NONE;
	}

	FActiveQuery Active;
	Active.Request = Request;
	Active.Completion = MoveTemp(Completion);
	Active.StartTime = FPlatformTime::Seconds();
	Active.bPlatformSlotAcquired = Platform != nullptr;
	ActiveQueries.Add(QueryId, MoveTemp(Active));
	++TotalStartedQueries;

	if (IsDebugEnabled())
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[AI.EQS] request id=%d owner=%s intent=%s kind=%s candidates=%d simplified=%s"),
			QueryId,
			*GetNameSafe(Querier),
			AHTacticalScoring::IntentToString(Request.Intent),
			*UEnum::GetValueAsString(Request.QueryKind),
			Request.MaxCandidatePoints,
			Request.bSimplifiedScoring ? TEXT("true") : TEXT("false"));
	}
	return QueryId;
}

void UAHTacticalPositionSubsystem::HandleQueryFinished(TSharedPtr<FEnvQueryResult> QueryResult)
{
	if (!QueryResult.IsValid())
	{
		return;
	}

	FActiveQuery Active;
	if (!ActiveQueries.RemoveAndCopyValue(QueryResult->QueryID, Active))
	{
		return;
	}
	RequestsByOwner.Remove(Active.Request.Querier.Get());
	if (Active.bPlatformSlotAcquired)
	{
		ReleasePlatformSlot();
	}

	const bool bTimedOut = TimedOutQueries.Remove(QueryResult->QueryID) > 0;
	if (bTimedOut || !QueryResult->IsSuccessful() || QueryResult->Items.IsEmpty())
	{
		CompleteWithFallback(
			Active.Request,
			Active.Completion,
			bTimedOut ? FName(TEXT("Timeout")) : (QueryResult->IsSuccessful() ? FName(TEXT("NoItems")) : FName(TEXT("QueryFailed"))),
			QueryResult->QueryID);
		return;
	}

	const float BestScore = QueryResult->GetItemScore(0);
	const float Threshold = BestScore * FMath::Clamp(Active.Request.QualityTolerance, 0.5f, 1.0f);
	int32 CandidateCount = 1;
	while (CandidateCount < QueryResult->Items.Num() && QueryResult->GetItemScore(CandidateCount) >= Threshold)
	{
		++CandidateCount;
	}
	FRandomStream Random(Active.Request.RandomSeed ^ QueryResult->QueryID);
	const int32 ChosenIndex = CandidateCount > 1 ? Random.RandRange(0, CandidateCount - 1) : 0;

	FAHTacticalPositionResult Result;
	Result.Intent = Active.Request.Intent;
	Result.Location = QueryResult->GetItemAsLocation(ChosenIndex);
	Result.Score = QueryResult->GetItemScore(ChosenIndex);
	Result.bSuccess = true;
	Result.QueryId = QueryResult->QueryID;
	ReserveLocation(Active.Request.Querier.Get(), Result.Location);
	RememberLocation(Active.Request.Querier.Get(), Result.Location);

	if (IsDebugEnabled())
	{
		FString Rejections;
		if (const TMap<FName, int32>* Counts = RejectionCounts.Find(Active.Request.Querier.Get()))
		{
			for (const TPair<FName, int32>& Pair : *Counts)
			{
				Rejections += FString::Printf(TEXT(" %s=%d"), *Pair.Key.ToString(), Pair.Value);
			}
		}
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[AI.EQS] selected id=%d intent=%s location=%s score=%.3f pool=%d rejected:%s"),
			Result.QueryId,
			AHTacticalScoring::IntentToString(Result.Intent),
			*Result.Location.ToCompactString(),
			Result.Score,
			CandidateCount,
			Rejections.IsEmpty() ? TEXT(" none") : *Rejections);
	}
	RejectionCounts.Remove(Active.Request.Querier.Get());
	DrawResult(Active.Request.Querier.Get(), Result);
	Active.Completion.ExecuteIfBound(Result);
}

void UAHTacticalPositionSubsystem::CompleteWithFallback(
	const FAHTacticalPositionRequest& Request,
	const FAHTacticalQueryFinished& Completion,
	FName Reason,
	int32 QueryId)
{
	FAHTacticalPositionResult Result;
	Result.Intent = Request.Intent;
	Result.Location = Request.FallbackLocation;
	Result.bUsedFallback = true;
	Result.FailureReason = Reason;
	Result.QueryId = QueryId;
	if (Request.Querier.IsValid())
	{
		ReserveLocation(Request.Querier.Get(), Result.Location);
		RememberLocation(Request.Querier.Get(), Result.Location);
	}
	if (IsDebugEnabled())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[AI.EQS] fallback id=%d owner=%s intent=%s reason=%s location=%s"),
			QueryId,
			*GetNameSafe(Request.Querier.Get()),
			AHTacticalScoring::IntentToString(Request.Intent),
			*Reason.ToString(),
			*Result.Location.ToCompactString());
	}
	DrawResult(Request.Querier.Get(), Result);
	Completion.ExecuteIfBound(Result);
}

void UAHTacticalPositionSubsystem::Tick(float DeltaTime)
{
	const double Now = FPlatformTime::Seconds();
	Reservations.RemoveAll([Now](const FPositionReservation& Reservation)
	{
		return !Reservation.Owner.IsValid() || Reservation.ExpiryTime <= Now;
	});

	TArray<int32> ExpiredQueries;
	for (const TPair<int32, FActiveQuery>& Pair : ActiveQueries)
	{
		if (Now - Pair.Value.StartTime > FMath::Max(0.05f, Pair.Value.Request.TimeoutSeconds))
		{
			ExpiredQueries.Add(Pair.Key);
		}
	}
	for (const int32 QueryId : ExpiredQueries)
	{
		TimedOutQueries.Add(QueryId);
		UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld());
		if (!Manager || !Manager->AbortQuery(QueryId))
		{
			FActiveQuery Active;
			if (ActiveQueries.RemoveAndCopyValue(QueryId, Active))
			{
				RequestsByOwner.Remove(Active.Request.Querier.Get());
				if (Active.bPlatformSlotAcquired)
				{
					ReleasePlatformSlot();
				}
				CompleteWithFallback(Active.Request, Active.Completion, TEXT("Timeout"), QueryId);
			}
			TimedOutQueries.Remove(QueryId);
		}
	}
}

void UAHTacticalPositionSubsystem::CancelForQuerier(const UObject* Querier)
{
	if (!Querier)
	{
		return;
	}
	if (UEnvQueryManager* Manager = UEnvQueryManager::GetCurrent(GetWorld()))
	{
		Manager->SilentlyRemoveAllQueriesByQuerier(*Querier);
	}

	TArray<int32> QueryIds;
	for (const TPair<int32, FActiveQuery>& Pair : ActiveQueries)
	{
		if (Pair.Value.Request.Querier.Get() == Querier)
		{
			QueryIds.Add(Pair.Key);
		}
	}
	for (const int32 QueryId : QueryIds)
	{
		FActiveQuery Active;
		if (ActiveQueries.RemoveAndCopyValue(QueryId, Active) && Active.bPlatformSlotAcquired)
		{
			ReleasePlatformSlot();
		}
		TimedOutQueries.Remove(QueryId);
	}

	RequestsByOwner.Remove(Querier);
	Reservations.RemoveAll([Querier](const FPositionReservation& Reservation) { return Reservation.Owner.Get() == Querier; });
	RecentPositions.Remove(Querier);
	RejectionCounts.Remove(Querier);
}

const FAHTacticalPositionRequest* UAHTacticalPositionSubsystem::FindRequest(const UObject* Querier) const
{
	return Querier ? RequestsByOwner.Find(Querier) : nullptr;
}

float UAHTacticalPositionSubsystem::GetReservationPenalty(const UObject* Querier, const FVector& Candidate) const
{
	float Penalty = 0.0f;
	for (const FPositionReservation& Reservation : Reservations)
	{
		if (!Reservation.Owner.IsValid() || Reservation.Owner.Get() == Querier)
		{
			continue;
		}
		const float Distance = FVector::Dist2D(Candidate, Reservation.Location);
		Penalty = FMath::Max(Penalty, 1.0f - FMath::Clamp((Distance - 180.0f) / 420.0f, 0.0f, 1.0f));
	}
	return Penalty;
}

float UAHTacticalPositionSubsystem::GetRecentPositionPenalty(const UObject* Querier, const FVector& Candidate) const
{
	const TArray<FVector>* Positions = Querier ? RecentPositions.Find(Querier) : nullptr;
	if (!Positions)
	{
		return 0.0f;
	}
	float Penalty = 0.0f;
	for (const FVector& Position : *Positions)
	{
		const float Distance = FVector::Dist2D(Candidate, Position);
		Penalty = FMath::Max(Penalty, 1.0f - FMath::Clamp((Distance - 120.0f) / 480.0f, 0.0f, 1.0f));
	}
	return Penalty;
}

void UAHTacticalPositionSubsystem::RecordRejection(const UObject* Querier, FName Reason)
{
	if (IsDebugEnabled() && Querier && !Reason.IsNone())
	{
		RejectionCounts.FindOrAdd(Querier).FindOrAdd(Reason)++;
	}
}

void UAHTacticalPositionSubsystem::ReleasePlatformSlot()
{
	if (UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this))
	{
		Platform->ReleaseEQSQuerySlot();
	}
}

void UAHTacticalPositionSubsystem::ReserveLocation(const UObject* Owner, const FVector& Location)
{
	if (!Owner)
	{
		return;
	}
	Reservations.RemoveAll([Owner](const FPositionReservation& Reservation) { return Reservation.Owner.Get() == Owner; });
	FPositionReservation& Reservation = Reservations.AddDefaulted_GetRef();
	Reservation.Owner = Owner;
	Reservation.Location = Location;
	Reservation.ExpiryTime = FPlatformTime::Seconds() + ReservationLifetimeSeconds;
}

void UAHTacticalPositionSubsystem::RememberLocation(const UObject* Owner, const FVector& Location)
{
	if (!Owner)
	{
		return;
	}
	TArray<FVector>& Positions = RecentPositions.FindOrAdd(Owner);
	Positions.Insert(Location, 0);
	Positions.SetNum(FMath::Min(Positions.Num(), 3));
}

void UAHTacticalPositionSubsystem::DrawResult(const UObject* Owner, const FAHTacticalPositionResult& Result) const
{
#if ENABLE_DRAW_DEBUG
	if (!IsDrawEnabled() || !GetWorld())
	{
		return;
	}
	const AActor* OwnerActor = Cast<AActor>(Owner);
	const FColor Color = Result.bUsedFallback ? FColor::Orange : FColor::Green;
	DrawDebugSphere(GetWorld(), Result.Location, 70.0f, 16, Color, false, 8.0f, 0, 3.0f);
	if (OwnerActor)
	{
		DrawDebugDirectionalArrow(GetWorld(), OwnerActor->GetActorLocation(), Result.Location, 120.0f, Color, false, 8.0f, 0, 2.0f);
	}
	DrawDebugString(GetWorld(), Result.Location + FVector(0.0f, 0.0f, 100.0f),
		FString::Printf(TEXT("%s %.2f%s"), AHTacticalScoring::IntentToString(Result.Intent), Result.Score, Result.bUsedFallback ? TEXT(" fallback") : TEXT("")),
		nullptr, Color, 8.0f, true);
#endif
}
