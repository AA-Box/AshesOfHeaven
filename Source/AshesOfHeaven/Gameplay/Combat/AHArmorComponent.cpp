#include "Gameplay/Combat/AHArmorComponent.h"
#include "Engine/World.h"

UAHArmorComponent::UAHArmorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAHArmorComponent::BeginPlay()
{
	Super::BeginPlay();
	ResetArmor();
}

void UAHArmorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!GetWorld() || CurrentArmor >= MaxArmor || GetWorld()->GetTimeSeconds() - LastDamageTime < RegenerationDelay)
	{
		return;
	}

	SetArmor(CurrentArmor + (RegenerationPerSecond * DeltaTime));
	if (CurrentArmor > 0.0f)
	{
		bWasBroken = false;
	}
}

float UAHArmorComponent::AbsorbDamage(float IncomingDamage)
{
	if (IncomingDamage <= 0.0f || CurrentArmor <= 0.0f)
	{
		return 0.0f;
	}

	const float Absorbed = FMath::Min(CurrentArmor, IncomingDamage);
	CurrentArmor = FMath::Max(0.0f, CurrentArmor - Absorbed);
	LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	OnArmorChanged.Broadcast(CurrentArmor, MaxArmor);

	if (CurrentArmor <= 0.0f && !bWasBroken)
	{
		bWasBroken = true;
		OnArmorBroken.Broadcast();
	}

	return Absorbed;
}

void UAHArmorComponent::SetArmor(float NewArmor)
{
	CurrentArmor = FMath::Clamp(NewArmor, 0.0f, MaxArmor);
	OnArmorChanged.Broadcast(CurrentArmor, MaxArmor);
}

void UAHArmorComponent::ResetArmor()
{
	CurrentArmor = FMath::Max(0.0f, MaxArmor);
	LastDamageTime = -BIG_NUMBER;
	bWasBroken = false;
	OnArmorChanged.Broadcast(CurrentArmor, MaxArmor);
}

float UAHArmorComponent::GetArmorPercent() const
{
	return MaxArmor > 0.0f ? CurrentArmor / MaxArmor : 0.0f;
}

float UAHArmorComponent::GetTimeUntilRegeneration(float CurrentTime) const
{
	return FMath::Max(0.0f, RegenerationDelay - (CurrentTime - LastDamageTime));
}
