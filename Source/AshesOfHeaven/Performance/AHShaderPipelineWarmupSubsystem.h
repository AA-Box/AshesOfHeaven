// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AHShaderPipelineWarmupSubsystem.generated.h"

struct FStreamableHandle;

/**
 * Exposes representative materials, Niagara systems, weapons, projectiles, and HUD
 * assets early enough for UE's automatic PSO precacher to work before combat.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHShaderPipelineWarmupSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Performance")
	bool IsStartupAssetPreloadComplete() const { return bStartupAssetPreloadComplete; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Performance")
	static int32 GetNumPipelinePrecompilesRemaining();

private:
	void HandleStartupAssetsLoaded();
	void SetBackgroundBatchMode();

	TSharedPtr<FStreamableHandle> StartupAssetHandle;
	bool bStartupAssetPreloadComplete = false;
	bool bChangedBatchMode = false;
};
