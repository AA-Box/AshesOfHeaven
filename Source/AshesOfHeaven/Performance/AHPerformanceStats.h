#pragma once

#include "CoreMinimal.h"

class UObject;

enum class EAHPerformanceCategory : uint8
{
	AIPerception,
	AITacticalDecisions,
	Movement,
	Combat,
	Interaction,
	Projectiles,
	VFXHelpers,
	PresentationActors,
	UI,
	Audio,
	EnvironmentActors,
	Count
};

/** Lightweight game-thread counters used by ah.Perf.TickBudget.Dump. */
class ASHESOFHEAVEN_API FAHPerformanceStats
{
public:
	static void AddSample(EAHPerformanceCategory Category, const UObject* Object, uint64 Cycles);
	static void AdvanceFrame();
	static void Reset();
	static FString BuildReport();
	static const TCHAR* CategoryName(EAHPerformanceCategory Category);
};

class ASHESOFHEAVEN_API FAHScopedPerformanceStat
{
public:
	FAHScopedPerformanceStat(EAHPerformanceCategory InCategory, const UObject* InObject)
		: Category(InCategory)
		, Object(InObject)
		, StartCycles(FPlatformTime::Cycles64())
	{
	}

	~FAHScopedPerformanceStat()
	{
		FAHPerformanceStats::AddSample(Category, Object, FPlatformTime::Cycles64() - StartCycles);
	}

private:
	EAHPerformanceCategory Category;
	const UObject* Object;
	uint64 StartCycles;
};

#define AH_JOIN_PERF_IMPL(A, B) A##B
#define AH_JOIN_PERF(A, B) AH_JOIN_PERF_IMPL(A, B)
#define AH_SCOPE_PERFORMANCE(Category, Object) \
	FAHScopedPerformanceStat AH_JOIN_PERF(AHScopedPerformanceStat_, __LINE__)(EAHPerformanceCategory::Category, Object)
