#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AHHumanSoldierCharacter.generated.h"

class AAHWeaponBase;

UCLASS()
class ASHESOFHEAVEN_API AAHHumanSoldierCharacter : public AAHCombatantCharacter
{
	GENERATED_BODY()

public:
	AAHHumanSoldierCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Human Soldier")
	TSubclassOf<AAHWeaponBase> WeaponClass;

	virtual void BeginPlay() override;
};
