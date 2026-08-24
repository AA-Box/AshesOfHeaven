#include "Tests/AHPoolTestDamageActor.h"

float AAHPoolTestDamageActor::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	LastDamage = DamageAmount;
	++DamageEvents;
	return Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}
