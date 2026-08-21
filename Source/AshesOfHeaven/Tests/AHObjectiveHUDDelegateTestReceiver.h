#pragma once

#include "CoreMinimal.h"
#include "AHObjectiveHUDDelegateTestReceiver.generated.h"

class AAHCombatPlayerController;

UCLASS()
class ASHESOFHEAVEN_API UAHObjectiveHUDDelegateTestReceiver : public UObject
{
	GENERATED_BODY()

public:
	void Configure(AAHCombatPlayerController* InController);

	UFUNCTION()
	void HandleObjectiveChanged(FText Objective, int32 Index, int32 Count);

	UFUNCTION()
	void HandleMissionComplete();

	bool WasObjectiveCallbackInvoked() const { return bObjectiveCallbackInvoked; }
	bool WasMissionCompleteCallbackInvoked() const { return bMissionCompleteCallbackInvoked; }

private:
	TWeakObjectPtr<AAHCombatPlayerController> Controller;
	bool bObjectiveCallbackInvoked = false;
	bool bMissionCompleteCallbackInvoked = false;
};
