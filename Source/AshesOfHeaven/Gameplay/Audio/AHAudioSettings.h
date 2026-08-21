#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AHAudioSettings.generated.h"

UCLASS(config=Game, defaultconfig, meta=(DisplayName="Ashes Audio"))
class ASHESOFHEAVEN_API UAHAudioSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere, Category="Palette")
	FSoftObjectPath DefaultPalette;
};
