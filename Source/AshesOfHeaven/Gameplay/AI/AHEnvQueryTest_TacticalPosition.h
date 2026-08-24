#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryTest.h"
#include "Gameplay/AI/AHTacticalPositionTypes.h"
#include "AHEnvQueryTest_TacticalPosition.generated.h"

/** Scores one focused tactical query using combat geometry and the captured controller state. */
UCLASS(meta=(DisplayName="Ashes Tactical Position"))
class ASHESOFHEAVEN_API UAHEnvQueryTest_TacticalPosition : public UEnvQueryTest
{
	GENERATED_BODY()

public:
	UAHEnvQueryTest_TacticalPosition(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, Category="Tactical")
	EAHTacticalQueryKind QueryKind = EAHTacticalQueryKind::RangedFiring;

	virtual void RunTest(FEnvQueryInstance& QueryInstance) const override;
};
