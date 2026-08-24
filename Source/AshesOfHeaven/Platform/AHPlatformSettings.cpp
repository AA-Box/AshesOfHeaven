// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHPlatformSettings.h"

UAHPlatformSettings::UAHPlatformSettings()
{
	CategoryName = TEXT("Game");

	HighEndMobileCorpseBudget.SoftLimit = 10;
	HighEndMobileCorpseBudget.HardLimit = 12;
	HighEndMobileCorpseBudget.MinimumLifetimeSeconds = 14.0f;
	HighEndMobileCorpseBudget.EmergencyMinimumLifetimeSeconds = 3.0f;
	HighEndMobileCorpseBudget.MaximumRagdollSeconds = 6.0f;
	HighEndMobileCorpseBudget.MaximumCleanupPerPass = 3;

	BaselineMobileCorpseBudget.SoftLimit = 6;
	BaselineMobileCorpseBudget.HardLimit = 8;
	BaselineMobileCorpseBudget.MinimumLifetimeSeconds = 10.0f;
	BaselineMobileCorpseBudget.EmergencyMinimumLifetimeSeconds = 2.5f;
	BaselineMobileCorpseBudget.DeathReactionSeconds = 0.75f;
	BaselineMobileCorpseBudget.SettleDelaySeconds = 1.5f;
	BaselineMobileCorpseBudget.MaximumRagdollSeconds = 5.0f;
	BaselineMobileCorpseBudget.ReducedCostDelaySeconds = 0.25f;
	BaselineMobileCorpseBudget.MaximumCleanupPerPass = 3;
}

const UAHPlatformSettings* UAHPlatformSettings::Get()
{
	return GetDefault<UAHPlatformSettings>();
}
