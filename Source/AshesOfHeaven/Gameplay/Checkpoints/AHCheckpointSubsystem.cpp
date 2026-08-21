#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
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
	RuntimeState.MapName = GetWorld()->GetName();
	RuntimeState.PlayerLocation = Player->GetActorLocation();
	RuntimeState.PlayerRotation = Player->GetActorRotation();
	RuntimeState.Health = Player->GetHealthComponent() ? Player->GetHealthComponent()->GetHealth() : 100.0f;
	RuntimeState.Armor = Player->GetArmorComponent() ? Player->GetArmorComponent()->GetArmor() : 100.0f;
	RuntimeState.Ammo = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetSavedAmmo() : FAHAmmoState();
	RuntimeState.Grenades = Player->GetInventoryComponent() ? Player->GetInventoryComponent()->GetGrenades() : 0;
	RuntimeState.ObjectiveIndex = Objectives->GetCurrentObjectiveIndex();
	RuntimeState.ChapterState = Chapter->GetState();
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

	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>();
	UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>();
	if (!Player || !Objectives || !Chapter)
	{
		return false;
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
