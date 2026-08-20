#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AHVeilPilgrimCharacter.generated.h"

class AAHWeaponBase;

UCLASS()
class ASHESOFHEAVEN_API AAHVeilPilgrimCharacter : public AAHCombatantCharacter
{
	GENERATED_BODY()

public:
	AAHVeilPilgrimCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veil Pilgrim")
	TSubclassOf<AAHWeaponBase> WeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veil Pilgrim")
	float ArmorValue = 35.0f;

	virtual void BeginPlay() override;
};
