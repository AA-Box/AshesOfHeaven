#pragma once

#include "CoreMinimal.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AHVeilWardenCharacter.generated.h"

class UAHEnemyDefinition;

UCLASS()
class ASHESOFHEAVEN_API AAHVeilWardenCharacter : public AAHCombatantCharacter
{
	GENERATED_BODY()

public:
	AAHVeilWardenCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;
	virtual void ApplyEnemyDefinition(UAHEnemyDefinition* Definition) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veil Warden", meta=(ClampMin=0.0))
	float ShieldCycleSeconds = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veil Warden", meta=(ClampMin=0.0, ClampMax=1.0))
	float ShieldDamageMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veil Warden", meta=(ClampMin=1.0))
	float TeleportCycleSeconds = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Veil Warden", meta=(ClampMin=100.0))
	float TeleportDistance = 650.0f;

	UFUNCTION(BlueprintPure, Category="Veil Warden")
	bool IsShieldActive() const { return bShieldActive; }

private:
	virtual FPrimaryAssetId GetDefaultEnemyDefinitionId() const override;

	float ShieldTimer = 0.0f;
	float TeleportTimer = 0.0f;
	bool bShieldActive = true;
};
