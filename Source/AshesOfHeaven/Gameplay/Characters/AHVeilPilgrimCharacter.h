#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AHVeilPilgrimCharacter.generated.h"

UCLASS()
class ASHESOFHEAVEN_API AAHVeilPilgrimCharacter : public AAHCombatantCharacter
{
	GENERATED_BODY()

public:
	AAHVeilPilgrimCharacter();

protected:
	virtual FPrimaryAssetId GetDefaultEnemyDefinitionId() const override;
};
