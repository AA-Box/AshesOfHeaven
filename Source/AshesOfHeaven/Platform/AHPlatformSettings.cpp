// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHPlatformSettings.h"

UAHPlatformSettings::UAHPlatformSettings()
{
	CategoryName = TEXT("Game");
}

const UAHPlatformSettings* UAHPlatformSettings::Get()
{
	return GetDefault<UAHPlatformSettings>();
}

