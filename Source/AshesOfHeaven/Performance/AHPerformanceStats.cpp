#include "Performance/AHPerformanceStats.h"

#include "HAL/PlatformTime.h"
#include "UObject/Class.h"

namespace
{
	struct FAHPerformanceCounter
	{
		uint64 Cycles = 0;
		uint64 Calls = 0;
	};

	FAHPerformanceCounter GCategoryCounters[static_cast<int32>(EAHPerformanceCategory::Count)];
	TMap<FName, FAHPerformanceCounter> GClassCounters;
	uint64 GMeasuredFrames = 0;
	double GMeasurementStartTime = FPlatformTime::Seconds();
}

void FAHPerformanceStats::AddSample(EAHPerformanceCategory Category, const UObject* Object, uint64 Cycles)
{
	check(IsInGameThread());
	FAHPerformanceCounter& Counter = GCategoryCounters[static_cast<int32>(Category)];
	Counter.Cycles += Cycles;
	++Counter.Calls;

	if (Object)
	{
		FAHPerformanceCounter& ClassCounter = GClassCounters.FindOrAdd(Object->GetClass()->GetFName());
		ClassCounter.Cycles += Cycles;
		++ClassCounter.Calls;
	}
}

void FAHPerformanceStats::AdvanceFrame()
{
	check(IsInGameThread());
	++GMeasuredFrames;
}

void FAHPerformanceStats::Reset()
{
	check(IsInGameThread());
	for (FAHPerformanceCounter& Counter : GCategoryCounters)
	{
		Counter = {};
	}
	GClassCounters.Reset();
	GMeasuredFrames = 0;
	GMeasurementStartTime = FPlatformTime::Seconds();
}

const TCHAR* FAHPerformanceStats::CategoryName(EAHPerformanceCategory Category)
{
	switch (Category)
	{
	case EAHPerformanceCategory::AIPerception: return TEXT("AI perception");
	case EAHPerformanceCategory::AITacticalDecisions: return TEXT("AI tactical decisions");
	case EAHPerformanceCategory::Movement: return TEXT("movement");
	case EAHPerformanceCategory::Combat: return TEXT("combat");
	case EAHPerformanceCategory::Interaction: return TEXT("interaction");
	case EAHPerformanceCategory::Projectiles: return TEXT("projectiles");
	case EAHPerformanceCategory::VFXHelpers: return TEXT("VFX helpers");
	case EAHPerformanceCategory::PresentationActors: return TEXT("presentation actors");
	case EAHPerformanceCategory::UI: return TEXT("UI");
	case EAHPerformanceCategory::Audio: return TEXT("audio");
	case EAHPerformanceCategory::EnvironmentActors: return TEXT("environment actors");
	default: return TEXT("unknown");
	}
}

FString FAHPerformanceStats::BuildReport()
{
	check(IsInGameThread());
	const double ElapsedSeconds = FMath::Max(0.0, FPlatformTime::Seconds() - GMeasurementStartTime);
	const double FrameDivisor = static_cast<double>(FMath::Max<uint64>(1, GMeasuredFrames));
	FString Report = FString::Printf(
		TEXT("Measured frames=%llu elapsed=%.2fs\n"),
		static_cast<unsigned long long>(GMeasuredFrames), ElapsedSeconds);

	for (int32 Index = 0; Index < static_cast<int32>(EAHPerformanceCategory::Count); ++Index)
	{
		const EAHPerformanceCategory Category = static_cast<EAHPerformanceCategory>(Index);
		const FAHPerformanceCounter& Counter = GCategoryCounters[Index];
		const double TotalMs = FPlatformTime::ToMilliseconds64(Counter.Cycles);
		Report += FString::Printf(
			TEXT("  %-24s %8.4f ms/frame  %6.2f calls/frame  total=%8.2f ms\n"),
			CategoryName(Category), TotalMs / FrameDivisor,
			static_cast<double>(Counter.Calls) / FrameDivisor, TotalMs);
	}

	TArray<TPair<FName, FAHPerformanceCounter>> Classes;
	Classes.Reserve(GClassCounters.Num());
	for (const TPair<FName, FAHPerformanceCounter>& Pair : GClassCounters)
	{
		Classes.Add(Pair);
	}
	Classes.Sort([](const auto& A, const auto& B) { return A.Value.Cycles > B.Value.Cycles; });
	Report += TEXT("  Expensive instrumented classes:\n");
	for (int32 Index = 0; Index < FMath::Min(Classes.Num(), 10); ++Index)
	{
		const double TotalMs = FPlatformTime::ToMilliseconds64(Classes[Index].Value.Cycles);
		Report += FString::Printf(
			TEXT("    %-36s %8.4f ms/frame  calls=%llu\n"),
			*Classes[Index].Key.ToString(), TotalMs / FrameDivisor,
			static_cast<unsigned long long>(Classes[Index].Value.Calls));
	}
	return Report;
}
