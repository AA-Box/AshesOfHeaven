#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AHAudioSettings.generated.h"

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Ashes Audio"))
class ASHESOFHEAVEN_API UAHAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Emergency fallback for commandlet diagnostics only. Keep disabled for normal presentation. */
	UPROPERTY(config, EditAnywhere, Category="Fallback")
	bool bAllowGeneratedAudioFallback = false;

	UPROPERTY(config, EditAnywhere, Category="Palette")
	FSoftObjectPath DefaultPalette;
};
