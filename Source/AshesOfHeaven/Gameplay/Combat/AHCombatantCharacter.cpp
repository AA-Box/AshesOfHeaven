#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
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
	EnsureGreyboxBody();
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AAHCombatantCharacter::HandleHealthDeath);
	}
	if (ArmorComponent)
	{
		ArmorComponent->OnArmorBroken.AddDynamic(this, &AAHCombatantCharacter::HandleArmorBroken);
	}
}

void AAHCombatantCharacter::EnsureGreyboxBody()
{
	// Greybox characters have no skeletal mesh, so a soldier is invisible while the weapon
	// attached to them still renders - the level reads as rifles gliding around on their own.
	// Stand in a block body until real meshes exist; with a mesh assigned this does nothing.
	if (!GetMesh() || GetMesh()->GetSkeletalMeshAsset() || GreyboxBodyMesh)
	{
		return;
	}

	UStaticMesh* BlockMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube"));
	if (!BlockMesh)
	{
		return;
	}

	GreyboxBodyMesh = NewObject<UStaticMeshComponent>(this, TEXT("GreyboxBody"));
	if (!GreyboxBodyMesh)
	{
		return;
	}
	GreyboxBodyMesh->SetStaticMesh(BlockMesh);
	GreyboxBodyMesh->SetupAttachment(GetCapsuleComponent());
	GreyboxBodyMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 0.0f));
	const bool bWardenSilhouette = GetClass()->GetName().Contains(TEXT("Warden"), ESearchCase::IgnoreCase);
	GreyboxBodyMesh->SetRelativeScale3D(bWardenSilhouette ? FVector(0.68f, 0.64f, 1.92f) : FVector(0.48f, 0.48f, 1.75f));
	GreyboxBodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GreyboxBodyMesh->SetCanEverAffectNavigation(false);
	const TCHAR* BodyMaterialPath = Faction == EAHFaction::Veil
		? TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian")
		: TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal");
	if (UMaterialInterface* BodyMaterial = LoadObject<UMaterialInterface>(nullptr, BodyMaterialPath))
	{
		GreyboxBodyMesh->SetMaterial(0, BodyMaterial);
	}
	// The owner is looking out of this body, so keep it out of their own view.
	GreyboxBodyMesh->SetOwnerNoSee(true);
	GreyboxBodyMesh->SetVisibility(true);
	AddInstanceComponent(GreyboxBodyMesh);
	GreyboxBodyMesh->RegisterComponent();

	if (UStaticMesh* HeadMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Sphere.SM_AH_Sphere")))
	{
		GreyboxHeadMesh = NewObject<UStaticMeshComponent>(this, TEXT("GreyboxHead"));
		if (GreyboxHeadMesh)
		{
			GreyboxHeadMesh->SetStaticMesh(HeadMesh);
			GreyboxHeadMesh->SetupAttachment(GetCapsuleComponent());
			GreyboxHeadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 112.0f));
			GreyboxHeadMesh->SetRelativeScale3D(FVector(0.34f));
			GreyboxHeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GreyboxHeadMesh->SetCanEverAffectNavigation(false);
			if (UMaterialInterface* HeadMaterial = LoadObject<UMaterialInterface>(nullptr, BodyMaterialPath))
			{
				GreyboxHeadMesh->SetMaterial(0, HeadMaterial);
			}
			GreyboxHeadMesh->SetOwnerNoSee(true);
			GreyboxHeadMesh->SetVisibility(true);
			AddInstanceComponent(GreyboxHeadMesh);
			GreyboxHeadMesh->RegisterComponent();
		}
	}

	if (bWardenSilhouette)
	{
		GreyboxShoulderMesh = NewObject<UStaticMeshComponent>(this, TEXT("GreyboxShoulderArmor"));
		if (GreyboxShoulderMesh)
		{
			GreyboxShoulderMesh->SetStaticMesh(BlockMesh);
			GreyboxShoulderMesh->SetupAttachment(GetCapsuleComponent());
			GreyboxShoulderMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 78.0f));
			GreyboxShoulderMesh->SetRelativeScale3D(FVector(0.88f, 0.74f, 0.24f));
			GreyboxShoulderMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			GreyboxShoulderMesh->SetCanEverAffectNavigation(false);
			if (UMaterialInterface* ShoulderMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/Instances/MI_HumanMetal_Dark.MI_HumanMetal_Dark")))
			{
				GreyboxShoulderMesh->SetMaterial(0, ShoulderMaterial);
			}
			GreyboxShoulderMesh->SetOwnerNoSee(true);
			AddInstanceComponent(GreyboxShoulderMesh);
			GreyboxShoulderMesh->RegisterComponent();
		}
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
	else if (HealthDamage > 0.0f && GetWorld())
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayWorldCue(EAHAudioCue::Hurt, GetActorLocation(), 0.7f);
		}
	}
	if (Absorbed > 0.0f && ArmorDamageSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ArmorDamageSound, GetActorLocation());
	}
	else if (Absorbed > 0.0f && GetWorld())
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayWorldCue(EAHAudioCue::Armor, GetActorLocation(), 0.75f);
		}
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
	else if (GetWorld())
	{
		if (UAHAudioSubsystem* Audio = GetWorld()->GetSubsystem<UAHAudioSubsystem>())
		{
			Audio->PlayWorldCue(EAHAudioCue::Death, GetActorLocation(), 0.8f);
		}
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
