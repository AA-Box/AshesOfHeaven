#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/Generators/EnvQueryGenerator_ProjectedPoints.h"
#include "Gameplay/AI/AHTacticalPositionTypes.h"
#include "AHEnvQueryGenerator_TacticalPositions.generated.h"

/** Generates a bounded, intent-shaped set of navigation candidates for one focused query. */
UCLASS(meta=(DisplayName="Ashes Tactical Positions"))
class ASHESOFHEAVEN_API UAHEnvQueryGenerator_TacticalPositions : public UEnvQueryGenerator_ProjectedPoints
{
	GENERATED_BODY()

public:
	UAHEnvQueryGenerator_TacticalPositions(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	UPROPERTY(EditDefaultsOnly, Category="Tactical")
	EAHTacticalQueryKind QueryKind = EAHTacticalQueryKind::RangedFiring;

	virtual void GenerateItems(FEnvQueryInstance& QueryInstance) const override;
};
