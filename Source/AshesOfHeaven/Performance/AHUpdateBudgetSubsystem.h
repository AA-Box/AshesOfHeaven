#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "Platform/AHPlatformTypes.h"
#include "AHUpdateBudgetSubsystem.generated.h"

class UActorComponent;

UENUM(BlueprintType)
enum class EAHSignificanceTier : uint8
{
	Near,
	Mid,
	Far,
	Dormant
};

UENUM()
enum class EAHUpdateChannel : uint8
{
	Perception,
	TacticalDecision,
	Movement,
	Combat,
	Aim,
	CosmeticEffects,
	DistantBattlefieldSimulation,
	Count UMETA(Hidden)
};

struct FAHUpdateRateSet
{
	float Perception = 0.0f;
	float TacticalDecision = 0.0f;
	float Movement = 0.0f;
	float Combat = 0.0f;
	float Aim = 0.0f;
	float CosmeticEffects = 0.0f;
	float DistantBattlefieldSimulation = -1.0f;

	float GetInterval(EAHUpdateChannel Channel) const;
};

struct FAHUpdateBudgetPolicy
{
	float NearDistance = 2500.0f;
	float MidDistance = 6000.0f;
	float FarDistance = 12000.0f;
	float EvaluationInterval = 0.10f;
	int32 MaxNearActors = 24;
	int32 MaxMidActors = 48;
	int32 MaxFarActors = 96;
	FAHUpdateRateSet NearRates;
	FAHUpdateRateSet MidRates;
	FAHUpdateRateSet FarRates;
	FAHUpdateRateSet DormantRates;

	const FAHUpdateRateSet& GetRates(EAHSignificanceTier Tier) const;
};

struct FAHSignificanceInput
{
	float DistanceSquared = BIG_NUMBER;
	bool bVisible = false;
	bool bActiveEncounterMember = false;
	bool bCombatRelevant = false;
	bool bCurrentAttacker = false;
	bool bGrenadeThreat = false;
	bool bObjectiveRelevant = false;
	bool bNarrativeRelevant = false;
};

/** Central per-world significance policy for combat simulation update channels. */
UCLASS()
class ASHESOFHEAVEN_API UAHUpdateBudgetSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(EWorldType::Type WorldType) const override;

	void RegisterCombatant(AActor* Subject, AActor* TickOwner, bool bActiveEncounterMember = true);
	void UnregisterCombatant(AActor* Subject);
	void SetCombatState(AActor* Subject, bool bCombatRelevant, bool bCurrentAttacker);
	void ProtectFromGrenade(AActor* Subject, float DurationSeconds);
	void SetActiveEncounterMember(AActor* Subject, bool bActiveEncounterMember);

	EAHSignificanceTier GetTier(const AActor* Subject) const;
	bool IsUpdateDue(AActor* Subject, EAHUpdateChannel Channel, float WorldTime, float& OutUpdateDelta);
	int32 GetRegisteredCombatantCount() const { return Entries.Num(); }
	void DumpToLog(bool bResetCountersAfterDump = false) const;

	static bool IsEnabled();
	static bool IsDebugEnabled();
	static FAHUpdateBudgetPolicy BuildPolicy(const FAHPerformanceProfile& PerformanceProfile, bool bMobile);
	static EAHSignificanceTier EvaluateTier(const FAHSignificanceInput& Input, const FAHUpdateBudgetPolicy& Policy);
	static bool ConsumeInterval(double Now, float Interval, double& InOutNextDue, double& InOutLastUpdate, float& OutUpdateDelta);
	static const TCHAR* TierName(EAHSignificanceTier Tier);

private:
	struct FEntry
	{
		struct FComponentTickState
		{
			TWeakObjectPtr<UActorComponent> Component;
			bool bWasTickEnabled = false;
		};

		TWeakObjectPtr<AActor> Subject;
		TWeakObjectPtr<AActor> TickOwner;
		EAHSignificanceTier Tier = EAHSignificanceTier::Near;
		bool bActiveEncounterMember = true;
		bool bCombatRelevant = false;
		bool bCurrentAttacker = false;
		double GrenadeProtectionEndTime = -BIG_NUMBER;
		bool bOriginalSubjectTickEnabled = true;
		bool bOriginalTickOwnerTickEnabled = true;
		float OriginalTickOwnerInterval = 0.0f;
		double NextUpdateTimes[static_cast<int32>(EAHUpdateChannel::Count)] = {};
		double LastUpdateTimes[static_cast<int32>(EAHUpdateChannel::Count)] = {};
		TArray<FComponentTickState> SuspendedComponentTicks;
		bool bSubjectComponentsSuspended = false;
	};

	void RefreshPolicy();
	void RefreshTiers();
	void RestoreOriginalTickState(FEntry& Entry) const;
	void SuspendSubjectComponentTicks(FEntry& Entry) const;
	void RestoreSubjectComponentTicks(FEntry& Entry) const;
	void ApplyTier(FEntry& Entry, EAHSignificanceTier NewTier);
	void ResetSchedules(FEntry& Entry, double Now);
	FAHSignificanceInput BuildInput(const FEntry& Entry, const APawn* PlayerPawn, double Now) const;
	FString BuildTickAudit() const;
	void DrawDebugState() const;
	void TryStartStressCapture();

	TMap<TWeakObjectPtr<AActor>, FEntry> Entries;
	FAHUpdateBudgetPolicy Policy;
	double NextEvaluationTime = 0.0;
	bool bAppliedEnabledPolicy = false;
	int32 StressCombatantCount = 0;
	int32 StressCaptureFrameLimit = 180;
	int32 StressCaptureFrameCount = 0;
	bool bStressCaptureStarted = false;
	bool bStressCaptureCompleted = false;
};
