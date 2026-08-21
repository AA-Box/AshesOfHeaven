#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AHChapterOneGameMode.generated.h"

UCLASS()
class ASHESOFHEAVEN_API AAHChapterOneGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAHChapterOneGameMode();

	virtual void BeginPlay() override;
	virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

private:
	void RestoreCheckpointAfterSpawn();
	FTimerHandle RestoreTimer;
};
