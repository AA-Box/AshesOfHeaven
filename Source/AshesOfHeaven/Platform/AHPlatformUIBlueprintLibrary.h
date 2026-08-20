// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AHPlatformUIBlueprintLibrary.generated.h"

/** Responsive HUD helpers for safe areas, DPI, aspect ratios, and mobile control scale. */
UCLASS()
class ASHESOFHEAVEN_API UAHPlatformUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|UI", meta=(WorldContext="WorldContextObject"))
	static void GetResponsiveHUDMetrics(const UObject* WorldContextObject, FVector4& SafePadding, FVector2D& ViewportSize, float& DPIScale, float& RecommendedHUDScale);

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|UI", meta=(WorldContext="WorldContextObject"))
	static bool IsLandscapeViewport(const UObject* WorldContextObject);
};

