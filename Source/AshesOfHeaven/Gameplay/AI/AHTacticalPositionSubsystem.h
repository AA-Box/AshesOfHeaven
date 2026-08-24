#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Gameplay/AI/AHTacticalPositionTypes.h"
#include "AHTacticalPositionSubsystem.generated.h"

class UEnvQuery;
struct FEnvQueryResult;

DECLARE_DELEGATE_OneParam(FAHTacticalQueryFinished, const FAHTacticalPositionResult&);

/**
 * Owns focused tactical EQS templates, asynchronous query budgets, position reservations,
 * and recent-position memory for every combatant in a world.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHTacticalPositionSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	/** Starts a time-sliced EQS request or completes immediately through its safe fallback. */
	int32 RequestPosition(const FAHTacticalPositionRequest& Request, FAHTacticalQueryFinished Completion);

	/** Cancels query state and releases reservations owned by a departing combatant. */
	void CancelForQuerier(const UObject* Querier);

	/** Returns the immutable request used by contexts and tests while a query is active. */
	const FAHTacticalPositionRequest* FindRequest(const UObject* Querier) const;

	float GetReservationPenalty(const UObject* Querier, const FVector& Candidate) const;
	float GetRecentPositionPenalty(const UObject* Querier, const FVector& Candidate) const;
	void RecordRejection(const UObject* Querier, FName Reason);

	int32 GetActiveQueryCount() const { return ActiveQueries.Num(); }
	int32 GetTotalStartedQueryCount() const { return TotalStartedQueries; }

	static bool IsDebugEnabled();
	static bool IsDrawEnabled();
	static bool IsDisabled();

private:
	struct FActiveQuery
	{
		FAHTacticalPositionRequest Request;
		FAHTacticalQueryFinished Completion;
		double StartTime = 0.0;
		bool bPlatformSlotAcquired = false;
	};

	struct FPositionReservation
	{
		TWeakObjectPtr<const UObject> Owner;
		FVector Location = FVector::ZeroVector;
		double ExpiryTime = 0.0;
	};

	UEnvQuery* BuildQueryTemplate(EAHTacticalQueryKind QueryKind);
	UEnvQuery* GetQueryTemplate(EAHTacticalQueryKind QueryKind) const;
	void HandleQueryFinished(TSharedPtr<FEnvQueryResult> QueryResult);
	void CompleteWithFallback(const FAHTacticalPositionRequest& Request, const FAHTacticalQueryFinished& Completion, FName Reason, int32 QueryId = INDEX_NONE);
	void ReleasePlatformSlot();
	void ReserveLocation(const UObject* Owner, const FVector& Location);
	void RememberLocation(const UObject* Owner, const FVector& Location);
	void DrawResult(const UObject* Owner, const FAHTacticalPositionResult& Result) const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UEnvQuery>> QueryTemplates;

	TMap<int32, FActiveQuery> ActiveQueries;
	TMap<TWeakObjectPtr<const UObject>, FAHTacticalPositionRequest> RequestsByOwner;
	TArray<FPositionReservation> Reservations;
	TMap<TWeakObjectPtr<const UObject>, TArray<FVector>> RecentPositions;
	TMap<TWeakObjectPtr<const UObject>, TMap<FName, int32>> RejectionCounts;
	TSet<int32> TimedOutQueries;
	int32 TotalStartedQueries = 0;
};
