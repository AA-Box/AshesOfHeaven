#include "Gameplay/Combat/AHGameplayTypes.h"

bool UAHCombatRulesLibrary::IsHostile(EAHFaction Source, EAHFaction Target)
{
	if (Source == EAHFaction::Neutral || Target == EAHFaction::Neutral || Source == Target)
	{
		return false;
	}

	return (Source == EAHFaction::Veil) != (Target == EAHFaction::Veil);
}

float UAHCombatRulesLibrary::ApplyArmorAbsorption(float IncomingDamage, float CurrentArmor, float& ArmorDamage)
{
	const float ClampedDamage = FMath::Max(0.0f, IncomingDamage);
	ArmorDamage = FMath::Min(FMath::Max(0.0f, CurrentArmor), ClampedDamage);
	return FMath::Max(0.0f, ClampedDamage - ArmorDamage);
}

int32 UAHCombatRulesLibrary::CalculateReloadTransfer(const FAHAmmoState& Ammo)
{
	const int32 Missing = FMath::Max(0, Ammo.MagazineCapacity - Ammo.Magazine);
	return FMath::Clamp(Missing, 0, FMath::Max(0, Ammo.Reserve));
}
