#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "GameFramework/CharacterMovementComponent.h"

AAHVeilPilgrimCharacter::AAHVeilPilgrimCharacter()
{
	Faction = EAHFaction::Veil;
	HealthComponent->MaxHealth = 120.0f;
	ArmorComponent->MaxArmor = ArmorValue;
	bDestroyOnDeath = true;
	AIControllerClass = AAHCombatAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	GetCharacterMovement()->MaxWalkSpeed = 300.0f;
	WeaponClass = AAHWeaponBase::StaticClass();
}

void AAHVeilPilgrimCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (WeaponClass && InventoryComponent && InventoryComponent->GetWeapons().IsEmpty())
	{
		InventoryComponent->AddWeaponClass(WeaponClass);
	}
}
