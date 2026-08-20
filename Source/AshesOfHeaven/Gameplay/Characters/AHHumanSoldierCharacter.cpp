#include "Gameplay/Characters/AHHumanSoldierCharacter.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"

AAHHumanSoldierCharacter::AAHHumanSoldierCharacter()
{
	Faction = EAHFaction::Human;
	HealthComponent->MaxHealth = 100.0f;
	ArmorComponent->MaxArmor = 50.0f;
	bDestroyOnDeath = true;
	AIControllerClass = AAHCombatAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	WeaponClass = AAHWeaponBase::StaticClass();
}

void AAHHumanSoldierCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (WeaponClass && InventoryComponent && InventoryComponent->GetWeapons().IsEmpty())
	{
		InventoryComponent->AddWeaponClass(WeaponClass);
	}
}
