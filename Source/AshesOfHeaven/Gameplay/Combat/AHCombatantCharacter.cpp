#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

AAHCombatantCharacter::AAHCombatantCharacter()
{
	HealthComponent = CreateDefaultSubobject<UAHHealthComponent>(TEXT("Health"));
	ArmorComponent = CreateDefaultSubobject<UAHArmorComponent>(TEXT("Armor"));
	CombatComponent = CreateDefaultSubobject<UAHCombatComponent>(TEXT("Combat"));
	InteractionComponent = CreateDefaultSubobject<UAHInteractionComponent>(TEXT("Interaction"));
	InventoryComponent = CreateDefaultSubobject<UAHInventoryComponent>(TEXT("Inventory"));

	GetCharacterMovement()->MaxWalkSpeed = 460.0f;
}

void AAHCombatantCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AAHCombatantCharacter::HandleHealthDeath);
	}
	if (ArmorComponent)
	{
		ArmorComponent->OnArmorBroken.AddDynamic(this, &AAHCombatantCharacter::HandleArmorBroken);
	}
}

float AAHCombatantCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (IsCombatantDead() || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	bool bHeadshot = false;
	FHitResult Hit;
	if (const FPointDamageEvent* PointDamage = DamageEvent.IsOfType(FPointDamageEvent::ClassID) ? static_cast<const FPointDamageEvent*>(&DamageEvent) : nullptr)
	{
		Hit = PointDamage->HitInfo;
		bHeadshot = Hit.BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase);
	}

	const float ZoneMultiplier = bHeadshot ? HeadshotMultiplier : 1.0f;
	const float ZonedDamage = DamageAmount * ZoneMultiplier;
	const float ArmorBefore = ArmorComponent ? ArmorComponent->GetArmor() : 0.0f;
	const float Absorbed = ArmorComponent ? ArmorComponent->AbsorbDamage(ZonedDamage) : 0.0f;
	const float HealthDamage = HealthComponent ? HealthComponent->ApplyDamage(ZonedDamage - Absorbed) : ZonedDamage;
	const bool bArmorBroken = ArmorBefore > 0.0f && ArmorComponent && ArmorComponent->GetArmor() <= 0.0f;
	float DirectionAngle = 0.0f;
	if (DamageCauser)
	{
		const FVector ToSource = (DamageCauser->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const FVector Forward = GetActorForwardVector().GetSafeNormal2D();
		DirectionAngle = FMath::RadiansToDegrees(FMath::Atan2(FVector::CrossProduct(Forward, ToSource).Z, FVector::DotProduct(Forward, ToSource)));
	}
	if (HealthDamage > 0.0f && HurtSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, HurtSound, GetActorLocation());
	}
	if (Absorbed > 0.0f && ArmorDamageSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ArmorDamageSound, GetActorLocation());
	}
	OnDamageFeedback.Broadcast(HealthDamage + Absorbed, bHeadshot, Absorbed > 0.0f || ArmorBefore > 0.0f, bArmorBroken, DirectionAngle);
	return HealthDamage + Absorbed;
}

FName AAHCombatantCharacter::GetFactionName() const
{
	switch (Faction)
	{
	case EAHFaction::Player:
		return FName(TEXT("Player"));
	case EAHFaction::Human:
		return FName(TEXT("Human"));
	case EAHFaction::Veil:
		return FName(TEXT("Veil"));
	default:
		return FName(TEXT("Neutral"));
	}
}

bool AAHCombatantCharacter::IsHostileTo(const AAHCombatantCharacter* Other) const
{
	return Other && UAHCombatRulesLibrary::IsHostile(Faction, Other->Faction);
}

bool AAHCombatantCharacter::IsCombatantDead() const
{
	return !HealthComponent || HealthComponent->IsDead();
}

FVector AAHCombatantCharacter::GetWeaponTraceOrigin() const
{
	return GetFirstPersonCameraComponent() ? GetFirstPersonCameraComponent()->GetComponentLocation() : GetActorLocation() + FVector(0.0f, 0.0f, 70.0f);
}

FVector AAHCombatantCharacter::GetWeaponTargetLocation() const
{
	if (const AAHCombatantCharacter* TargetCharacter = Cast<AAHCombatantCharacter>(CombatTarget.Get()))
	{
		return TargetCharacter->GetActorLocation() + FVector(0.0f, 0.0f, 62.0f);
	}

	return GetWeaponTraceOrigin() + GetControlRotation().Vector() * 20000.0f;
}

void AAHCombatantCharacter::SetCombatTarget(AActor* NewTarget)
{
	CombatTarget = NewTarget;
}

void AAHCombatantCharacter::SetAimingDownSights(bool bNewAiming)
{
	bAimingDownSights = bNewAiming;
}

void AAHCombatantCharacter::ApplyCameraRecoil(float Vertical, float Horizontal)
{
	if (Controller && IsPlayerControlled())
	{
		AddControllerPitchInput(-Vertical * 0.35f);
		AddControllerYawInput(FMath::FRandRange(-Horizontal, Horizontal) * 0.18f);
	}
}

void AAHCombatantCharacter::NotifyWeaponShot(const FHitResult& Hit, bool bHit)
{
	OnWeaponShot.Broadcast(Hit, bHit);
}

void AAHCombatantCharacter::OnDeathStarted()
{
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CombatComponent)
	{
		CombatComponent->DisableCombat();
	}
	OnCombatantDeath.Broadcast();
	if (bDestroyOnDeath)
	{
		SetLifeSpan(3.0f);
	}
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation());
	}
}

void AAHCombatantCharacter::HandleHealthDeath()
{
	OnDeathStarted();
}

void AAHCombatantCharacter::HandleArmorBroken()
{
	OnDamageFeedback.Broadcast(0.0f, false, true, true, 0.0f);
}
