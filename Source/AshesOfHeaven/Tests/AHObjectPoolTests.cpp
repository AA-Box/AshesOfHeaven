#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Components/SphereComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Gameplay/Pooling/AHObjectPoolSubsystem.h"
#include "Gameplay/Weapons/AHProjectileBase.h"
#include "HAL/PlatformMemory.h"
#include "Misc/AutomationTest.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Tests/AHPoolTestDamageActor.h"
#include "UObject/GarbageCollection.h"

namespace
{
	struct FAHPoolWorldFixture
	{
		UWorld* World = nullptr;

		bool Create(const TCHAR* Name)
		{
			const UWorld::InitializationValues Initialization = UWorld::InitializationValues()
				.InitializeScenes(true)
				.AllowAudioPlayback(false)
				.RequiresHitProxies(false)
				.CreatePhysicsScene(true)
				.CreateNavigation(false)
				.CreateAISystem(false)
				.ShouldSimulatePhysics(false)
				.EnableTraceCollision(true)
				.SetTransactional(false)
				.CreateFXSystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, FName(Name), nullptr, true, ERHIFeatureLevel::Num, &Initialization, false);
			if (!World)
			{
				return false;
			}
			if (GEngine)
			{
				FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
				WorldContext.SetCurrentWorld(World);
			}
			World->InitializeActorsForPlay(FURL());
			World->SetBegunPlay(true);
			World->BeginPlay();
			return true;
		}

		void Destroy()
		{
			if (World)
			{
				if (World->HasBegunPlay())
				{
					World->BeginTearingDown();
					World->EndPlay(EEndPlayReason::Quit);
				}
				if (GEngine)
				{
					GEngine->DestroyWorldContext(World);
				}
				World->DestroyWorld(false);
				World = nullptr;
			}
		}

		~FAHPoolWorldFixture()
		{
			Destroy();
		}
	};

	FAHObjectPoolAcquireContext MakeContext(const FVector& Location, AActor* Owner = nullptr, APawn* Instigator = nullptr)
	{
		FAHObjectPoolAcquireContext Context;
		Context.Transform = FTransform(FRotator::ZeroRotator, Location);
		Context.Owner = Owner;
		Context.Instigator = Instigator;
		return Context;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHObjectPoolLifecycleStressTest,
	"AshesOfHeaven.Pooling.ProjectileLifecycle10000",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHObjectPoolLifecycleStressTest::RunTest(const FString& Parameters)
{
	FAHPoolWorldFixture Fixture;
	TestTrue(TEXT("pool test world is created"), Fixture.Create(TEXT("AHPoolLifecycleWorld")));
	if (!Fixture.World)
	{
		return false;
	}

	UAHObjectPoolSubsystem* Pool = Fixture.World->GetSubsystem<UAHObjectPoolSubsystem>();
	TestNotNull(TEXT("world owns an object pool subsystem"), Pool);
	if (!Pool)
	{
		return false;
	}
	TestTrue(TEXT("projectile pool primes"), Pool->PrimePool(AAHProjectileBase::StaticClass(), 32, 64));

	AActor* OwnerA = Fixture.World->SpawnActor<AActor>();
	AActor* OwnerB = Fixture.World->SpawnActor<AActor>();
	UNiagaraSystem* Trail = NewObject<UNiagaraSystem>(GetTransientPackage());
	const AAHProjectileBase* Defaults = AAHProjectileBase::StaticClass()->GetDefaultObject<AAHProjectileBase>();
	for (int32 Cycle = 0; Cycle < 10000; ++Cycle)
	{
		AActor* ExpectedOwner = (Cycle & 1) == 0 ? OwnerA : OwnerB;
		AAHProjectileBase* Projectile = Pool->AcquireActor<AAHProjectileBase>(
			AAHProjectileBase::StaticClass(), MakeContext(FVector(static_cast<double>(Cycle), 0.0, 100.0), ExpectedOwner));
		if (!TestNotNull(TEXT("every cycle acquires a projectile"), Projectile))
		{
			return false;
		}
		Projectile->TrailEffect = Trail;
		Projectile->InitializeProjectile(FVector::ForwardVector, 37.0f);
		if (Cycle == 0 || Cycle == 9999)
		{
			TestEqual(TEXT("owner is replaced on acquire"), Projectile->GetOwner(), ExpectedOwner);
			TestTrue(TEXT("collision is restored on acquire"), Projectile->GetActorEnableCollision());
			TestTrue(TEXT("velocity is restored without carry-over"), Projectile->ProjectileMovement->Velocity.Equals(FVector(3000.0f, 0.0f, 0.0f)));
			TestTrue(TEXT("hit delegate is bound"), Projectile->IsHitDelegateBoundForTest());
			TestTrue(TEXT("expiry timer is active"), Projectile->IsExpiryTimerActiveForTest());
			TestEqual(TEXT("trail asset is restored"), Projectile->TrailComponent->GetAsset(), Trail);
		}
		if (Cycle == 1)
		{
			TestEqual(TEXT("movement bounce default is restored"), Projectile->ProjectileMovement->bShouldBounce, Defaults->ProjectileMovement->bShouldBounce);
			TestEqual(TEXT("movement gravity default is restored"), Projectile->ProjectileMovement->ProjectileGravityScale, Defaults->ProjectileMovement->ProjectileGravityScale);
		}
		if (Cycle == 0)
		{
			Projectile->ProjectileMovement->bShouldBounce = !Defaults->ProjectileMovement->bShouldBounce;
			Projectile->ProjectileMovement->ProjectileGravityScale = Defaults->ProjectileMovement->ProjectileGravityScale + 1.0f;
		}
		TestTrue(TEXT("cycle releases the managed projectile"), Pool->ReleaseActor(Projectile));
		if (Cycle == 0 || Cycle == 9999)
		{
			TestNull(TEXT("owner is cleared on release"), Projectile->GetOwner());
			TestNull(TEXT("instigator is cleared on release"), Projectile->GetInstigator());
			TestFalse(TEXT("collision is disabled on release"), Projectile->GetActorEnableCollision());
			TestTrue(TEXT("velocity is zero on release"), Projectile->ProjectileMovement->Velocity.IsNearlyZero());
			TestFalse(TEXT("hit delegates are cleared on release"), Projectile->IsHitDelegateBoundForTest());
			TestFalse(TEXT("expiry timer is cleared on release"), Projectile->IsExpiryTimerActiveForTest());
			TestFalse(TEXT("trail is inactive on release"), Projectile->TrailComponent->IsActive());
			TestNull(TEXT("trail asset reference is cleared on release"), Projectile->TrailComponent->GetAsset());
			TestNull(TEXT("trail property reference is cleared on release"), Projectile->TrailEffect);
			TestEqual(TEXT("damage state is cleared on release"), Projectile->Damage, 0.0f);
			TestEqual(TEXT("lifetime state is cleared on release"), Projectile->LifeSeconds, 0.0f);
			TestFalse(TEXT("homing state is cleared on release"), Projectile->ProjectileMovement->HomingTargetComponent.IsValid());
			TestTrue(TEXT("release transform is deterministic"), Projectile->GetActorTransform().Equals(FTransform::Identity));
		}
	}

	const TArray<FAHObjectPoolMetrics> Metrics = Pool->GetMetrics();
	TestEqual(TEXT("one typed pool is registered"), Metrics.Num(), 1);
	if (!Metrics.IsEmpty())
	{
		TestEqual(TEXT("primed capacity stays constant"), Metrics[0].Capacity, 32);
		TestEqual(TEXT("all actors are available after stress"), Metrics[0].Available, 32);
		TestEqual(TEXT("no actors remain active"), Metrics[0].Active, 0);
		TestEqual(TEXT("sequential load causes no miss"), Metrics[0].Misses, 0);
		TestEqual(TEXT("sequential load causes no growth"), Metrics[0].Growth, 0);
		TestEqual(TEXT("peak concurrent use is one"), Metrics[0].PeakUse, 1);
	}
	OwnerA->Destroy();
	OwnerB->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHObjectPoolImpactAndExhaustionTest,
	"AshesOfHeaven.Pooling.ImpactExhaustionAndCheckpointReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHObjectPoolImpactAndExhaustionTest::RunTest(const FString& Parameters)
{
	FAHPoolWorldFixture Fixture;
	TestTrue(TEXT("impact test world is created"), Fixture.Create(TEXT("AHPoolImpactWorld")));
	if (!Fixture.World)
	{
		return false;
	}
	UAHObjectPoolSubsystem* Pool = Fixture.World->GetSubsystem<UAHObjectPoolSubsystem>();
	TestTrue(TEXT("small projectile pool primes"), Pool && Pool->PrimePool(AAHProjectileBase::StaticClass(), 4, 8));
	if (!Pool)
	{
		return false;
	}

	AActor* Owner = Fixture.World->SpawnActor<AActor>();
	APawn* Instigator = Fixture.World->SpawnActor<APawn>();
	AAHPoolTestDamageActor* Target = Fixture.World->SpawnActor<AAHPoolTestDamageActor>();
	TArray<AAHProjectileBase*> Projectiles;
	for (int32 Index = 0; Index < 9; ++Index)
	{
		AAHProjectileBase* Projectile = AAHProjectileBase::SpawnProjectile(
			Fixture.World,
			AAHProjectileBase::StaticClass(),
			FTransform(FRotator::ZeroRotator, FVector(0.0, Index * 20.0, 100.0)),
			Owner,
			Instigator,
			FVector::ForwardVector,
			41.0f);
		TestNotNull(TEXT("hard-max exhaustion still returns every critical projectile"), Projectile);
		if (Projectile)
		{
			Projectiles.Add(Projectile);
		}
	}
	TestEqual(TEXT("all simultaneous requests succeed"), Projectiles.Num(), 9);
	if (!Projectiles.IsEmpty())
	{
		TestEqual(TEXT("spawn integration sets the correct owner"), Projectiles[0]->GetOwner(), Owner);
		TestEqual(TEXT("spawn integration sets the correct instigator"), Projectiles[0]->GetInstigator(), Instigator);
	}
	const TArray<FAHObjectPoolMetrics> LoadedMetrics = Pool->GetMetrics();
	if (!LoadedMetrics.IsEmpty())
	{
		TestEqual(TEXT("pool grows only to hard max"), LoadedMetrics[0].Capacity, 8);
		TestEqual(TEXT("managed active count stops at hard max"), LoadedMetrics[0].Active, 8);
		TestEqual(TEXT("one fallback is recorded as a miss"), LoadedMetrics[0].Misses, 1);
		TestEqual(TEXT("growth is measured"), LoadedMetrics[0].Growth, 4);
		TestEqual(TEXT("peak managed use is measured"), LoadedMetrics[0].PeakUse, 8);
	}

	FHitResult Hit(Target, nullptr, Target->GetActorLocation(), -FVector::ForwardVector);
	for (AAHProjectileBase* Projectile : Projectiles)
	{
		Projectile->TestSimulateImpact(Target, Hit);
	}
	TestEqual(TEXT("every simultaneous projectile applies damage once"), Target->DamageEvents, 9);
	TestEqual(TEXT("per-shot damage survives pooling"), Target->LastDamage, 41.0f);
	Projectiles.Last()->TestSimulateImpact(Target, Hit);
	TestEqual(TEXT("a stale repeated hit cannot apply damage"), Target->DamageEvents, 9);

	AAHProjectileBase* CheckpointProjectile = AAHProjectileBase::SpawnProjectile(
		Fixture.World,
		AAHProjectileBase::StaticClass(),
		FTransform(FRotator::ZeroRotator, FVector(100.0, 0.0, 100.0)),
		Owner,
		Instigator,
		FVector::ForwardVector,
		41.0f);
	TestNotNull(TEXT("checkpoint transient projectile acquires"), CheckpointProjectile);
	TestEqual(TEXT("checkpoint reset releases active transient objects"), Pool->ReleaseAllActive(), 1);
	TestNull(TEXT("checkpoint reset clears owner"), CheckpointProjectile ? CheckpointProjectile->GetOwner() : nullptr);
	TestFalse(TEXT("checkpoint reset clears timers"), CheckpointProjectile && CheckpointProjectile->IsExpiryTimerActiveForTest());

	Owner->Destroy();
	Instigator->Destroy();
	Target->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHObjectPoolLevelTransitionTest,
	"AshesOfHeaven.Pooling.LevelTransitionIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHObjectPoolLevelTransitionTest::RunTest(const FString& Parameters)
{
	FAHPoolWorldFixture Fixture;
	TestTrue(TEXT("first transition world is created"), Fixture.Create(TEXT("AHPoolTransitionWorldA")));
	if (!Fixture.World)
	{
		return false;
	}
	UAHObjectPoolSubsystem* FirstPool = Fixture.World->GetSubsystem<UAHObjectPoolSubsystem>();
	TestTrue(TEXT("first world pool primes"), FirstPool && FirstPool->PrimePool(AAHProjectileBase::StaticClass(), 4, 8));
	AAHProjectileBase* FirstProjectile = FirstPool
		? FirstPool->AcquireActor<AAHProjectileBase>(AAHProjectileBase::StaticClass(), MakeContext(FVector(10.0, 20.0, 30.0)))
		: nullptr;
	TestNotNull(TEXT("first world has an active projectile"), FirstProjectile);
	const TWeakObjectPtr<AAHProjectileBase> FirstProjectileWeak = FirstProjectile;

	Fixture.Destroy();
	CollectGarbage(RF_NoFlags, true);
	TestFalse(TEXT("world teardown destroys active pooled actors"), FirstProjectileWeak.IsValid());
	TestTrue(TEXT("second transition world is created"), Fixture.Create(TEXT("AHPoolTransitionWorldB")));
	if (!Fixture.World)
	{
		return false;
	}
	UAHObjectPoolSubsystem* SecondPool = Fixture.World->GetSubsystem<UAHObjectPoolSubsystem>();
	TestNotNull(TEXT("second world owns a fresh pool subsystem"), SecondPool);
	TestTrue(TEXT("pool registrations do not leak across worlds"), SecondPool && SecondPool->GetMetrics().IsEmpty());
	TestTrue(TEXT("second world pool independently primes"), SecondPool && SecondPool->PrimePool(AAHProjectileBase::StaticClass(), 1, 2));
	AAHProjectileBase* SecondProjectile = SecondPool
		? SecondPool->AcquireActor<AAHProjectileBase>(AAHProjectileBase::StaticClass(), MakeContext(FVector::ZeroVector))
		: nullptr;
	TestNotNull(TEXT("second world independently acquires"), SecondProjectile);
	TestTrue(TEXT("second world independently releases"), SecondPool && SecondPool->ReleaseActor(SecondProjectile));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHObjectPoolSpawnChurnBenchmark,
	"AshesOfHeaven.Pooling.SpawnChurnBenchmark",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHObjectPoolSpawnChurnBenchmark::RunTest(const FString& Parameters)
{
	FAHPoolWorldFixture Fixture;
	TestTrue(TEXT("benchmark world is created"), Fixture.Create(TEXT("AHPoolBenchmarkWorld")));
	if (!Fixture.World)
	{
		return false;
	}
	constexpr int32 Cycles = 10000;
	const uint64 BaselineMemoryBefore = FPlatformMemory::GetStats().UsedPhysical;
	const double BaselineStart = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < Cycles; ++Index)
	{
		if (AAHProjectileBase* Projectile = Fixture.World->SpawnActor<AAHProjectileBase>(FVector::ZeroVector, FRotator::ZeroRotator))
		{
			Projectile->Destroy();
		}
	}
	const double BaselineMilliseconds = (FPlatformTime::Seconds() - BaselineStart) * 1000.0;
	const uint64 BaselineMemoryAfter = FPlatformMemory::GetStats().UsedPhysical;

	UAHObjectPoolSubsystem* Pool = Fixture.World->GetSubsystem<UAHObjectPoolSubsystem>();
	TestTrue(TEXT("benchmark pool primes"), Pool && Pool->PrimePool(AAHProjectileBase::StaticClass(), 32, 64));
	if (!Pool)
	{
		return false;
	}
	const uint64 PooledMemoryBefore = FPlatformMemory::GetStats().UsedPhysical;
	const double PooledStart = FPlatformTime::Seconds();
	for (int32 Index = 0; Index < Cycles; ++Index)
	{
		AAHProjectileBase* Projectile = Pool->AcquireActor<AAHProjectileBase>(
			AAHProjectileBase::StaticClass(), MakeContext(FVector::ZeroVector));
		if (Projectile)
		{
			Pool->ReleaseActor(Projectile);
		}
	}
	const double PooledMilliseconds = (FPlatformTime::Seconds() - PooledStart) * 1000.0;
	const uint64 PooledMemoryAfter = FPlatformMemory::GetStats().UsedPhysical;
	const TArray<FAHObjectPoolMetrics> Metrics = Pool->GetMetrics();

	AddInfo(FString::Printf(
		TEXT("AH_POOL_BENCH cycles=%d baseline_actor_spawns=%d pooled_actor_spawns=%d baseline_ms=%.3f pooled_ms=%.3f baseline_resident_delta=%lld pooled_resident_delta=%lld"),
		Cycles,
		Cycles,
		Metrics.IsEmpty() ? -1 : Metrics[0].Capacity,
		BaselineMilliseconds,
		PooledMilliseconds,
		static_cast<long long>(BaselineMemoryAfter) - static_cast<long long>(BaselineMemoryBefore),
		static_cast<long long>(PooledMemoryAfter) - static_cast<long long>(PooledMemoryBefore)));
	TestTrue(TEXT("benchmark completed all pooled cycles"), !Metrics.IsEmpty() && Metrics[0].Active == 0 && Metrics[0].Available == 32);
	return true;
}

#endif
