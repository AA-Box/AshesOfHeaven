#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AHCombatSliceGameMode.generated.h"

UCLASS()
class ASHESOFHEAVEN_API AAHCombatSliceGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAHCombatSliceGameMode();

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	void RestoreCheckpointAfterSpawn();
	FTimerHandle RestoreTimer;
};
