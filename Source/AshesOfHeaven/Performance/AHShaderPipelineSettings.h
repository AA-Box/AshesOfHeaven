// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "AHShaderPipelineSettings.generated.h"

/** Project-owned inputs for the bounded shader/PSO startup warmup. */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Ashes Shader Pipeline"))
class ASHESOFHEAVEN_API UAHShaderPipelineSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	static const UAHShaderPipelineSettings* Get();

	/** Asynchronously load the small, representative asset set during game-instance startup. */
	UPROPERTY(Config, EditAnywhere, Category="Startup Warmup")
	bool bEnableStartupAssetPreload = true;

	/** Temporarily use the pipeline cache Fast batch while the startup assets are loading. */
	UPROPERTY(Config, EditAnywhere, Category="Startup Warmup")
	bool bUseFastPipelineBatchDuringPreload = true;

	/** Soft paths are intentionally configurable and must be covered by the packaging rules. */
	UPROPERTY(Config, EditAnywhere, Category="Startup Warmup", meta=(AllowedClasses="/Script/CoreUObject.Object"))
	TArray<FSoftObjectPath> StartupAssets;
};
