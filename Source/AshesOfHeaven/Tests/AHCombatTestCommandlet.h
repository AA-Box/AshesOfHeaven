#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AHCombatTestCommandlet.generated.h"

UCLASS()
class ASHESOFHEAVEN_API UAHCombatVerificationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAHCombatVerificationCommandlet();
	virtual int32 Main(const FString& Params) override;
};
