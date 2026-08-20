#include "Gameplay/Combat/AHHealthComponent.h"

UAHHealthComponent::UAHHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAHHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetHealth();
}

float UAHHealthComponent::ApplyDamage(float Damage)
{
	if (bDead || Damage <= 0.0f)
	{
		return 0.0f;
	}

	const float AppliedDamage = FMath::Min(CurrentHealth, Damage);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth - AppliedDamage);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.0f)
	{
		bDead = true;
		OnDeath.Broadcast();
	}

	return AppliedDamage;
}

void UAHHealthComponent::SetHealth(float NewHealth)
{
	CurrentHealth = FMath::Clamp(NewHealth, 0.0f, MaxHealth);
	bDead = CurrentHealth <= 0.0f;
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void UAHHealthComponent::ResetHealth()
{
	bDead = false;
	CurrentHealth = FMath::Max(1.0f, MaxHealth);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

float UAHHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}
