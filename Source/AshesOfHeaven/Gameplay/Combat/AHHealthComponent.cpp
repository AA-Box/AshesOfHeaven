#include "Gameplay/Combat/AHHealthComponent.h"
#include "Engine/World.h"

UAHHealthComponent::UAHHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAHHealthComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetHealth();
	// Nothing to tick for the 24 combatants that never regenerate.
	SetComponentTickEnabled(RegenerationPerSecond > 0.0f);
}

void UAHHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bDead || RegenerationPerSecond <= 0.0f || CurrentHealth >= MaxHealth || !GetWorld()
		|| GetWorld()->GetTimeSeconds() - LastDamageTime < RegenerationDelay)
	{
		return;
	}

	SetHealth(CurrentHealth + (RegenerationPerSecond * DeltaTime));
}

float UAHHealthComponent::ApplyDamage(float Damage)
{
	if (bDead || Damage <= 0.0f)
	{
		return 0.0f;
	}

	LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
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

float UAHHealthComponent::GetTimeUntilRegeneration(float CurrentTime) const
{
	return FMath::Max(0.0f, RegenerationDelay - (CurrentTime - LastDamageTime));
}

float UAHHealthComponent::GetHealthPercent() const
{
	return MaxHealth > 0.0f ? CurrentHealth / MaxHealth : 0.0f;
}
