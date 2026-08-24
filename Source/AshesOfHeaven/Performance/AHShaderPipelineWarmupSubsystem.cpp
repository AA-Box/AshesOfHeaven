// Copyright Epic Games, Inc. All Rights Reserved.

#include "Performance/AHShaderPipelineWarmupSubsystem.h"

#include "AshesOfHeaven.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"
#include "Performance/AHShaderPipelineSettings.h"
#include "ShaderPipelineCache.h"

namespace
{
	void LogShaderPipelineStatus()
	{
		UE_LOG(LogAshesOfHeaven, Display, TEXT("Shader pipeline status: startup precompiles remaining=%u"),
			FShaderPipelineCache::NumPrecompilesRemaining());
	}

	FAutoConsoleCommand ShaderPipelineStatusCommand(
		TEXT("AH.PSO.Status"),
		TEXT("Log the number of bundled pipeline-cache precompiles still outstanding."),
		FConsoleCommandDelegate::CreateStatic(&LogShaderPipelineStatus));
}

bool UAHShaderPipelineWarmupSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return Super::ShouldCreateSubsystem(Outer) && FApp::CanEverRender() && !IsRunningDedicatedServer();
}

void UAHShaderPipelineWarmupSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	const UAHShaderPipelineSettings* Settings = UAHShaderPipelineSettings::Get();
	if (!Settings->bEnableStartupAssetPreload || Settings->StartupAssets.IsEmpty())
	{
		bStartupAssetPreloadComplete = true;
		SetBackgroundBatchMode();
		return;
	}

	if (Settings->bUseFastPipelineBatchDuringPreload)
	{
		FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Fast);
		bChangedBatchMode = true;
	}

	UE_LOG(LogAshesOfHeaven, Log, TEXT("Starting bounded shader warmup preload for %d assets; pipeline precompiles remaining=%u"),
		Settings->StartupAssets.Num(), FShaderPipelineCache::NumPrecompilesRemaining());

	StartupAssetHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Settings->StartupAssets,
		FStreamableDelegate::CreateUObject(this, &UAHShaderPipelineWarmupSubsystem::HandleStartupAssetsLoaded),
		FStreamableManager::AsyncLoadHighPriority,
		false,
		false,
		TEXT("AshesShaderWarmup"));

	if (!StartupAssetHandle.IsValid())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("Shader warmup preload request could not be created."));
		HandleStartupAssetsLoaded();
	}
}

void UAHShaderPipelineWarmupSubsystem::Deinitialize()
{
	if (StartupAssetHandle.IsValid() && !StartupAssetHandle->HasLoadCompleted())
	{
		StartupAssetHandle->CancelHandle();
	}
	StartupAssetHandle.Reset();
	SetBackgroundBatchMode();
	Super::Deinitialize();
}

int32 UAHShaderPipelineWarmupSubsystem::GetNumPipelinePrecompilesRemaining()
{
	return static_cast<int32>(FShaderPipelineCache::NumPrecompilesRemaining());
}

void UAHShaderPipelineWarmupSubsystem::HandleStartupAssetsLoaded()
{
	bStartupAssetPreloadComplete = true;
	SetBackgroundBatchMode();

	const UAHShaderPipelineSettings* Settings = UAHShaderPipelineSettings::Get();
	UE_LOG(LogAshesOfHeaven, Log, TEXT("Shader warmup preload completed for %d assets; pipeline precompiles remaining=%u"),
		Settings->StartupAssets.Num(), FShaderPipelineCache::NumPrecompilesRemaining());
}

void UAHShaderPipelineWarmupSubsystem::SetBackgroundBatchMode()
{
	if (bChangedBatchMode)
	{
		FShaderPipelineCache::SetBatchMode(FShaderPipelineCache::BatchMode::Background);
		bChangedBatchMode = false;
	}
}
