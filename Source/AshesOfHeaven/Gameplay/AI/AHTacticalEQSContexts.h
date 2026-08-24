#pragma once

#include "CoreMinimal.h"
#include "EnvironmentQuery/EnvQueryContext.h"
#include "AHTacticalEQSContexts.generated.h"

/** Supplies the perceived combat target captured when the query began. */
UCLASS()
class ASHESOFHEAVEN_API UAH_EQSContext_CombatTarget : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** Supplies friendly combatants used for separation and reservation scoring. */
UCLASS()
class ASHESOFHEAVEN_API UAH_EQSContext_Squadmates : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** Supplies the last location actually known by the controller, never live hidden target data. */
UCLASS()
class ASHESOFHEAVEN_API UAH_EQSContext_LastKnownTarget : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};

/** Supplies the grenade location captured by the existing reaction system. */
UCLASS()
class ASHESOFHEAVEN_API UAH_EQSContext_GrenadeThreat : public UEnvQueryContext
{
	GENERATED_BODY()

public:
	virtual void ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const override;
};
