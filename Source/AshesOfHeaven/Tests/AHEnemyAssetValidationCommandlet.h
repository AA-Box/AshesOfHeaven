#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "AHEnemyAssetValidationCommandlet.generated.h"

/** CI/cook gate for AHEnemy and AHEncounter manifests. Safe to run before or after cooking. */
UCLASS()
class ASHESOFHEAVEN_API UAHEnemyAssetValidationCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAHEnemyAssetValidationCommandlet();
	virtual int32 Main(const FString& Params) override;

	/** Shared with automation tests. Returns false after logging every discovered problem. */
	static bool ValidateEnemyAssetManifest(FOutputDevice& Output, int32& OutValidatedAssets);
};
