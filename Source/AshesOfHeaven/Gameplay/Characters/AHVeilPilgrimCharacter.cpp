#include "Gameplay/Characters/AHVeilPilgrimCharacter.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"

AAHVeilPilgrimCharacter::AAHVeilPilgrimCharacter()
{
}

FPrimaryAssetId AAHVeilPilgrimCharacter::GetDefaultEnemyDefinitionId() const
{
	return AHEnemyAssets::EnemyId(TEXT("Pilgrim"));
}
