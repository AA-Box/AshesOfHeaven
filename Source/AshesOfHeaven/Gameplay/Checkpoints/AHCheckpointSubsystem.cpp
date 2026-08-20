#include "Gameplay/Checkpoints/AHCheckpointSubsystem.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Objectives/AHObjectiveSubsystem.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

bool UAHCheckpointSubsystem::CaptureCheckpoint(FName CheckpointId)
{
	AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(UGameplayStatics::GetPlayerCharacter(GetWorld(), 0));
	UAHPlatformSaveSubsystem* Save = GetWorld()->GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>();
	UAHObjectiveSubsystem* Objectives = GetWorld()->GetSubsystem<UAHObjectiveSubsystem>();
	if (!Player || !Save || !Objectives)
	{
		return false;
	}

	TArray<FName> CompletedEncounters = RuntimeState.CompletedEncounters;
	LoadState();
	CompletedEncounters.Append(RuntimeState.CompletedEncounters);
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
	return Save->SaveCombatCheckpoint(RuntimeState);
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
	if (!Player || !Objectives)
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
	return true;
}

void UAHCheckpointSubsystem::ReloadLatestCheckpoint()
{
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
}
