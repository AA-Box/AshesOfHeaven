#include "Tests/AHObjectiveHUDDelegateTestReceiver.h"

#include "Gameplay/Game/AHCombatPlayerController.h"

void UAHObjectiveHUDDelegateTestReceiver::Configure(AAHCombatPlayerController* InController)
{
	Controller = InController;
}

void UAHObjectiveHUDDelegateTestReceiver::HandleObjectiveChanged(FText Objective, int32 Index, int32 Count)
{
	bObjectiveCallbackInvoked = true;
	if (AAHCombatPlayerController* BoundController = Controller.Get())
	{
		BoundController->HandleObjectiveChanged(Objective, Index, Count);
	}
}

void UAHObjectiveHUDDelegateTestReceiver::HandleMissionComplete()
{
	bMissionCompleteCallbackInvoked = true;
	if (AAHCombatPlayerController* BoundController = Controller.Get())
	{
		BoundController->HandleMissionComplete();
	}
}
