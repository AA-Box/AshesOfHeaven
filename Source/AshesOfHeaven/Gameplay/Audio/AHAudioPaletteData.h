#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Sound/SoundBase.h"
#include "AHAudioPaletteData.generated.h"

/** Semantic audio event palette. Designers assign SoundCue or MetaSound assets here. */
UCLASS(BlueprintType)
class ASHESOFHEAVEN_API UAHAudioPaletteData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Events")
	TMap<FName, TSoftObjectPtr<USoundBase>> Events;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Audio|Environments")
	TMap<FName, TSoftObjectPtr<USoundBase>> Environments;
};
