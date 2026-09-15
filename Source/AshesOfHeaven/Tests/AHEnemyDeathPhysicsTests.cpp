#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/DamageEvents.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Gameplay/Combat/AHCorpseManagerSubsystem.h"
#include "Animation/AnimSequenceBase.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAHEnemyDeathPhysicsTest, "AshesOfHeaven.Combat.EnemyDeathStaysGrounded",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)
bool FAHEnemyDeathPhysicsTest::RunTest(const FString& Parameters)
{
 const auto Init = UWorld::InitializationValues().InitializeScenes(true).AllowAudioPlayback(false)
  .CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(true);
 UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, TEXT("DeathPhysicsTest"), nullptr, true, ERHIFeatureLevel::Num, &Init, false);
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 World->SetBegunPlay(true);
 World->BeginPlay();
 AActor* Floor = World->SpawnActor<AActor>();
 UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
 Floor->SetRootComponent(Box);
 Box->SetBoxExtent(FVector(10000, 10000, 50));
 Box->SetCollisionProfileName(TEXT("BlockAll"));
 Box->RegisterComponentWithWorld(World);
 Floor->SetActorLocation(FVector(0,0,-50));
 int32 Index = 0;
 for (const TCHAR* Name : {TEXT("Pilgrim"), TEXT("Hound"), TEXT("Spider"), TEXT("Teuthisan")})
 {
  const FString Path = FString::Printf(TEXT("/Game/Ashes/Data/Enemies/DA_Enemy_%s.DA_Enemy_%s"), Name, Name);
  UAHEnemyDefinition* Def = LoadObject<UAHEnemyDefinition>(nullptr, *Path);
  if (!TestNotNull(Path, Def)) continue;
  Def->Visuals.SkeletalMesh.LoadSynchronous();
  auto& Clips = Def->Visuals.Locomotion;
  Clips.Idle.LoadSynchronous(); Clips.Walk.LoadSynchronous(); Clips.Run.LoadSynchronous();
  Clips.Attack.LoadSynchronous(); Clips.Death.LoadSynchronous();
  FActorSpawnParameters Spawn;
  Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Enemy = World->SpawnActor<AAHVeilPilgrimCharacter>(FVector(Index++ * 1500, 0, Def->CombatDefaults.CapsuleHalfHeight), FRotator::ZeroRotator, Spawn);
  Enemy->ApplyEnemyDefinition(Def);
  Enemy->bPersistentCorpse = true;
  auto* Body = Enemy->GetMesh();
  Body->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  for (int32 Frame = 0; Frame < 15; ++Frame) World->Tick(LEVELTICK_All, 1.f/60.f);
  const float InitialZ = Body->GetComponentLocation().Z;
  Enemy->TakeDamage(1000000.f, FDamageEvent(), nullptr, nullptr);
  float MaxRise = 0;
  bool bFinite = true;
  auto* Manager = World->GetSubsystem<UAHCorpseManagerSubsystem>();
  for (int32 Frame = 0; Frame < FMath::CeilToInt((Clips.Death.Get()->GetPlayLength() + 4.f) * 60.f); ++Frame)
  {
   ++GFrameCounter;
   World->Tick(LEVELTICK_All, 1.f/60.f);
   Manager->Tick(1.f/60.f);
   bFinite &= !Body->GetComponentLocation().ContainsNaN();
   MaxRise = FMath::Max(MaxRise, float(Body->GetComponentLocation().Z - InitialZ));
  }
  AddInfo(FString::Printf(TEXT("%s maximum corpse rise: %.1f cm, simulating=%d, final Z=%.1f"), Name, MaxRise, Body->IsSimulatingPhysics(), Body->GetComponentLocation().Z));
  TestTrue(FString::Printf(TEXT("%s corpse does not launch upward"), Name), bFinite && MaxRise < 100.f);
  TestFalse(TEXT("authored death stays out of ragdoll simulation"), Body->IsSimulatingPhysics());
  TestFalse(TEXT("death animation completes before cleanup"), Enemy->IsPlayingDeathAnimation());
  Enemy->Destroy();
 }
 GEngine->DestroyWorldContext(World);
 World->DestroyWorld(false);
 return true;
}
#endif
