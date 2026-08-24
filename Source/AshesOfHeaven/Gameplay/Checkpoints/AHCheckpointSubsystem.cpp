#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHCorpseManagerSubsystem.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

bool UAHCheckpointSubsystem::CaptureCheckpoint(FName CheckpointId)
{
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	UAHPlatformSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>();
	UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>();
	UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>();
	if (!Player || !Save || !Objectives || !Chapter)
	{
		return false;
	}
	const FAHCheckpointSpatialDefinition* CheckpointDefinition = AHChapterSpatial::FindCheckpointDefinition(CheckpointId);
	if (!CheckpointDefinition)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Spatial][Checkpoint] rejected unknown checkpoint id=%s"), *CheckpointId.ToString());
		return false;
	}
	const EAHChapterStage CurrentStage = Chapter->GetStage();
	const FAHStageSpatialDefinition& CurrentDefinition = AHChapterSpatial::GetStageDefinition(CurrentStage);
	const bool bStageCompatible = CheckpointDefinition->Stage == CurrentStage
		|| (UAHChapterSubsystem::ObjectiveIndexForStage(CheckpointDefinition->Stage) == UAHChapterSubsystem::ObjectiveIndexForStage(CurrentStage)
			&& CheckpointDefinition->ZoneId == CurrentDefinition.ZoneId);
	if (!bStageCompatible)
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Spatial][Checkpoint] rejected incompatible capture id=%s stage=%s currentStage=%s zone=%s currentZone=%s"), *CheckpointId.ToString(), *UEnum::GetValueAsString(CheckpointDefinition->Stage), *UEnum::GetValueAsString(CurrentStage), *CheckpointDefinition->ZoneId.ToString(), *CurrentDefinition.ZoneId.ToString());
		return false;
	}

	TArray<FName> CompletedEncounters = RuntimeState.CompletedEncounters;
	LoadState();
	for (const FName EncounterId : RuntimeState.CompletedEncounters)
	{
		CompletedEncounters.AddUnique(EncounterId);
	}
	RuntimeState = FAHCombatCheckpointState();
	RuntimeState.CompletedEncounters = CompletedEncounters;
	RuntimeState.bValid = true;
	RuntimeState.CheckpointId = CheckpointId;
	RuntimeState.Stage = CheckpointDefinition->Stage;
	RuntimeState.ZoneId = CheckpointDefinition->ZoneId;
	RuntimeState.SpatialSchemaVersion = AHChapterStateConstants::CurrentSpatialSchemaVersion;
	RuntimeState.MapName = GetWorld()->GetName();
	RuntimeState.PlayerLocation = Player->GetActorLocation();
	RuntimeState.PlayerRotation = Player->GetActorRotation();
	RuntimeState.Health = Player->GetHealthComponent() ? Player->GetHealthComponent()->GetHealth() : 100.0f;
	RuntimeState.Armor = Player->GetArmorComponent() ? Player->GetArmorComponent()->GetArmor() : 100.0f;
	RuntimeState.Ammo = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetSavedAmmo() : FAHAmmoState();
	RuntimeState.Grenades = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetGrenades() : 0;
	RuntimeState.ObjectiveIndex = Objectives->GetCurrentObjectiveIndex();
	RuntimeState.ChapterState = Chapter->GetState();
	RuntimeState.ChapterState.Stage = CheckpointDefinition->Stage;
	if (AAHManticoreVehicle* Manticore = Cast<AAHManticoreVehicle>(UGameplayStatics::GetActorOfClass(GetWorld(), AAHManticoreVehicle::StaticClass())))
	{
		RuntimeState.ChapterState.Vehicle = Manticore->GetVehicleState();
	}
	RuntimeState.ChapterState.CheckpointId = CheckpointId;
	RuntimeState.ChapterState.ObjectiveIndex = RuntimeState.ObjectiveIndex;
	RuntimeState.ChapterState.CompletedEncounters = CompletedEncounters;
	const bool bSaved = Save->SaveCombatCheckpoint(RuntimeState);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Checkpoint] capture id=%s objective=%d encounters=%d ammo=%d/%d grenades=%d result=%s"), *CheckpointId.ToString(), RuntimeState.ObjectiveIndex, RuntimeState.CompletedEncounters.Num(), RuntimeState.Ammo.Magazine, RuntimeState.Ammo.Reserve, RuntimeState.Grenades, bSaved ? TEXT("success") : TEXT("failure"));
	#endif
	return bSaved;
}

bool UAHCheckpointSubsystem::LoadState()
{
	if (UAHPlatformSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>())
	{
		return Save->LoadCombatCheckpoint(RuntimeState);
	}
	return false;
}

bool UAHCheckpointSubsystem::RestoreLatestCheckpoint()
{
	if (!LoadState())
	{
		return false;
	}
	return RestoreFromState(RuntimeState);
}

bool UAHCheckpointSubsystem::IsCheckpointTransformValid(UWorld* World, const FVector& Location)
{
	if (!World || Location.ContainsNaN())
	{
		return false;
	}
	// The chapter is authored inside a bounded strip; anything outside it is a stale or
	// corrupt transform, not a place the game can put a player.
	if (FMath::Abs(Location.X) > 40000.0f || FMath::Abs(Location.Y) > 6000.0f || Location.Z < -400.0f || Location.Z > 6000.0f)
	{
		return false;
	}
	FHitResult Hit;
	const FVector Start = Location + FVector(0.0f, 0.0f, 300.0f);
	const FVector End = Location - FVector(0.0f, 0.0f, 1500.0f);
	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility))
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHCheckpointTransform), false);
	return !World->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeCapsule(34.0f, 88.0f), Params);
}

bool UAHCheckpointSubsystem::IsCheckpointTransformValid(UWorld* World, EAHChapterStage Stage, const FVector& Location)
{
	if (!IsCheckpointTransformValid(World, Location))
	{
		return false;
	}
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(Stage);
	const bool bInBounds = Location.X >= Definition.ExpectedBoundsMin.X && Location.X <= Definition.ExpectedBoundsMax.X
		&& Location.Y >= Definition.ExpectedBoundsMin.Y && Location.Y <= Definition.ExpectedBoundsMax.Y
		&& Location.Z >= Definition.ExpectedBoundsMin.Z && Location.Z <= Definition.ExpectedBoundsMax.Z;
	if (!bInBounds || FVector::Dist2D(Location, Definition.StageAnchor) > Definition.MaxDistanceFromAnchor)
	{
		return false;
	}
	FHitResult Ground;
	if (!World->LineTraceSingleByChannel(Ground, Location + FVector(0.0f, 0.0f, 300.0f), Location - FVector(0.0f, 0.0f, 1500.0f), ECC_Visibility))
	{
		return false;
	}
	return FMath::Abs(Ground.ImpactPoint.Z - Definition.GameplayFloorZ) <= 180.0f;
}

bool UAHCheckpointSubsystem::RestoreFromState(const FAHCombatCheckpointState& State)
{
	RuntimeState = State;
	if (!RuntimeState.MapName.IsEmpty() && !RuntimeState.MapName.Contains(TEXT("ChapterOne"), ESearchCase::IgnoreCase))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.4][Checkpoint] ignored checkpoint from different map=%s"), *RuntimeState.MapName);
		return false;
	}

	RuntimeState.ChapterState = UAHChapterSubsystem::NormalizeState(RuntimeState.ChapterState);
	RuntimeState.ObjectiveIndex = RuntimeState.ChapterState.ObjectiveIndex;
	RuntimeState.Stage = RuntimeState.Stage == EAHChapterStage::OpeningBlack && RuntimeState.ChapterState.Stage != EAHChapterStage::OpeningBlack
		? RuntimeState.ChapterState.Stage
		: RuntimeState.Stage;
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(RuntimeState.ChapterState.Stage);
	if (State.SpatialSchemaVersion != AHChapterStateConstants::CurrentSpatialSchemaVersion
		|| State.Stage != RuntimeState.ChapterState.Stage
		|| State.ZoneId != Definition.ZoneId
		|| !AHChapterSpatial::FindCheckpointDefinition(State.CheckpointId))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.5][Checkpoint] rejected incompatible spatial schema=%d stage=%s savedStage=%s zone=%s expectedZone=%s id=%s"), State.SpatialSchemaVersion, *UEnum::GetValueAsString(RuntimeState.ChapterState.Stage), *UEnum::GetValueAsString(State.Stage), *State.ZoneId.ToString(), *Definition.ZoneId.ToString(), *State.CheckpointId.ToString());
		return false;
	}

	// Validity gates run before the player lookup: a stale or void checkpoint must be
	// rejected regardless of possession timing.
	//
	// The chapter-opening capture carries no progress worth restoring; replaying its stored
	// transform only leaks a previous session's position/look direction (e.g. facing away
	// from the world) into what the player experiences as a fresh start. Rejecting it makes
	// the caller re-capture a clean opening checkpoint at the actual spawn.
	if (RuntimeState.ObjectiveIndex <= 0)
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4.2][Checkpoint] opening checkpoint id=%s has no progress; using fresh spawn transform"), *RuntimeState.CheckpointId.ToString());
		return false;
	}
	if (!IsCheckpointTransformValid(GetWorld(), RuntimeState.ChapterState.Stage, RuntimeState.PlayerLocation))
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.4.2][Checkpoint] rejected stale checkpoint id=%s transform=%s: no valid ground at saved location"), *RuntimeState.CheckpointId.ToString(), *RuntimeState.PlayerLocation.ToCompactString());
		return false;
	}

	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>();
	UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>();
	if (!Player || !Objectives || !Chapter)
	{
		return false;
	}
	if (UAHCorpseManagerSubsystem* CorpseManager = GetWorld()->GetSubsystem<UAHCorpseManagerSubsystem>())
	{
		CorpseManager->ResetOrdinaryCorpsesForCheckpoint();
	}

	Player->SetActorLocationAndRotation(RuntimeState.PlayerLocation, RuntimeState.PlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
	if (Player->GetHealthComponent())
	{
		Player->GetHealthComponent()->SetHealth(RuntimeState.Health);
	}
	if (Player->GetArmorComponent())
	{
		Player->GetArmorComponent()->SetArmor(RuntimeState.Armor);
	}
	if (Player->GetInventoryComponent())
	{
		Player->GetInventoryComponent()->SetSavedAmmo(RuntimeState.Ammo);
		const int32 Delta = RuntimeState.Grenades - Player->GetInventoryComponent()->GetGrenades();
		Player->GetInventoryComponent()->AddGrenades(Delta);
	}
	Objectives->RestoreState(RuntimeState.ObjectiveIndex);
	Chapter->RestoreState(RuntimeState.ChapterState);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Checkpoint] restore id=%s objective=%d encounters=%d ammo=%d/%d grenades=%d"), *RuntimeState.CheckpointId.ToString(), RuntimeState.ObjectiveIndex, RuntimeState.CompletedEncounters.Num(), RuntimeState.Ammo.Magazine, RuntimeState.Ammo.Reserve, RuntimeState.Grenades);
	#endif
	return true;
}

bool UAHCheckpointSubsystem::RecoverToCanonicalStage()
{
	UAHChapterSubsystem* Chapter = GetWorld() && GetWorld()->GetGameInstance()
		? GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>()
		: nullptr;
	AAHCombatPlayerCharacter* Player = GetWorld() ? Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0)) : nullptr;
	if (!Chapter || !Player)
	{
		return false;
	}
	const FAHStageSpatialDefinition& Definition = AHChapterSpatial::GetStageDefinition(Chapter->GetStage());
	Player->SetActorLocationAndRotation(Definition.SafePlayerLocation, Definition.SafePlayerRotation, false, nullptr, ETeleportType::TeleportPhysics);
	RuntimeState.PlayerLocation = Definition.SafePlayerLocation;
	RuntimeState.PlayerRotation = Definition.SafePlayerRotation;
	RuntimeState.Stage = Chapter->GetStage();
	RuntimeState.ZoneId = Definition.ZoneId;
	RuntimeState.SpatialSchemaVersion = AHChapterStateConstants::CurrentSpatialSchemaVersion;
	UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.5][Checkpoint] recovered stage=%s zone=%s location=%s"), *UEnum::GetValueAsString(Chapter->GetStage()), *Definition.ZoneId.ToString(), *Definition.SafePlayerLocation.ToCompactString());
	return true;
}

void UAHCheckpointSubsystem::ReloadLatestCheckpoint()
{
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Checkpoint] reload_requested id=%s valid=%s"), *RuntimeState.CheckpointId.ToString(), HasCheckpoint() ? TEXT("true") : TEXT("false"));
	#endif
	if (!HasCheckpoint())
	{
		UGameplayStatics::OpenLevel(GetWorld(), FName(*GetWorld()->GetName()));
		return;
	}
	UGameplayStatics::OpenLevel(GetWorld(), FName(*GetWorld()->GetName()));
}

bool UAHCheckpointSubsystem::HasCheckpoint() const
{
	if (RuntimeState.bValid)
	{
		return true;
	}
	if (const UAHPlatformSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>())
	{
		FAHCombatCheckpointState Loaded;
		return Save->LoadCombatCheckpoint(Loaded);
	}
	return false;
}

bool UAHCheckpointSubsystem::IsEncounterCompleted(FName EncounterId) const
{
	if (RuntimeState.CompletedEncounters.Contains(EncounterId))
	{
		return true;
	}
	// MarkEncounterCompleted already guards this; without the same guard a world with no game
	// instance dereferences null on the very first encounter's BeginPlay.
	if (!GetWorld() || !GetWorld()->GetGameInstance())
	{
		return false;
	}
	if (const UAHPlatformSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>())
	{
		FAHCombatCheckpointState Loaded;
		return Save->LoadCombatCheckpoint(Loaded) && Loaded.CompletedEncounters.Contains(EncounterId);
	}
	return false;
}

void UAHCheckpointSubsystem::MarkEncounterCompleted(FName EncounterId)
{
	RuntimeState.CompletedEncounters.AddUnique(EncounterId);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Checkpoint] encounter_complete id=%s total=%d"), *EncounterId.ToString(), RuntimeState.CompletedEncounters.Num());
	#endif
	if (GetWorld() && GetWorld()->GetGameInstance())
	{
		if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
		{
			Chapter->MarkEncounterComplete(EncounterId);
		}
	}
}
