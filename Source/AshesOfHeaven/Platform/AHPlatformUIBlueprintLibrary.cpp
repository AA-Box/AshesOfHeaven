// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHPlatformUIBlueprintLibrary.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetLayoutLibrary.h"

void UAHPlatformUIBlueprintLibrary::GetResponsiveHUDMetrics(const UObject* WorldContextObject, FVector4& SafePadding, FVector2D& ViewportSize, float& DPIScale, float& RecommendedHUDScale)
{
	SafePadding = FVector4(0.0, 0.0, 0.0, 0.0);
	ViewportSize = UWidgetLayoutLibrary::GetViewportSize(WorldContextObject);
	DPIScale = UWidgetLayoutLibrary::GetViewportScale(WorldContextObject);

	FVector2D SafePaddingScale;
	FVector4 SpillOverPadding;
	UWidgetBlueprintLibrary::GetSafeZonePadding(WorldContextObject, SafePadding, SafePaddingScale, SpillOverPadding);

	const float WidthScale = ViewportSize.X > 0.0f ? ViewportSize.X / 1920.0f : 1.0f;
	const float HeightScale = ViewportSize.Y > 0.0f ? ViewportSize.Y / 1080.0f : 1.0f;
	RecommendedHUDScale = FMath::Clamp(FMath::Min(WidthScale, HeightScale) / FMath::Max(DPIScale, 0.5f), 0.75f, 1.35f);
}

bool UAHPlatformUIBlueprintLibrary::IsLandscapeViewport(const UObject* WorldContextObject)
{
	const FVector2D ViewportSize = UWidgetLayoutLibrary::GetViewportSize(WorldContextObject);
	return ViewportSize.X >= ViewportSize.Y;
}
