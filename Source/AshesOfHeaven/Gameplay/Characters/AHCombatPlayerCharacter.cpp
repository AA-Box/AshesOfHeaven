#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"

AAHCombatPlayerCharacter::AAHCombatPlayerCharacter()
{
	Faction = EAHFaction::Player;
	bDestroyOnDeath = false;
	HealthComponent->MaxHealth = 100.0f;
	ArmorComponent->MaxArmor = 100.0f;
	GetCharacterMovement()->AirControl = MilitaryAirControl;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	StartingWeaponClass = AAHWeaponBase::StaticClass();
}

void AAHCombatPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (StartingWeaponClass && InventoryComponent && InventoryComponent->GetWeapons().IsEmpty())
	{
		InventoryComponent->AddWeaponClass(StartingWeaponClass);
	}
	if (GetFirstPersonCameraComponent())
	{
		GetFirstPersonCameraComponent()->SetFieldOfView(HipFOV);
	}
}

void AAHCombatPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RefreshMovementSpeed();
	if (GetFirstPersonCameraComponent())
	{
		const float TargetFOV = IsAimingDownSights() ? ADSFOV : HipFOV;
		GetFirstPersonCameraComponent()->SetFieldOfView(FMath::FInterpTo(GetFirstPersonCameraComponent()->FieldOfView, TargetFOV, DeltaSeconds, 12.0f));
	}
}

void AAHCombatPlayerCharacter::SetupPlayerInputComponent(UInputComponent* InputComponent)
{
	Super::SetupPlayerInputComponent(InputComponent);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
	UAHPlatformManagerSubsystem* PlatformManager = UAHPlatformManagerSubsystem::Get(this);
	if (!EnhancedInput || !PlatformManager)
	{
		return;
	}

	EnhancedInput->BindAction(PlatformManager->GetFireAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::StartFire);
	EnhancedInput->BindAction(PlatformManager->GetFireAction(), ETriggerEvent::Completed, this, &AAHCombatPlayerCharacter::StopFire);
	EnhancedInput->BindAction(PlatformManager->GetADSAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::StartADS);
	EnhancedInput->BindAction(PlatformManager->GetADSAction(), ETriggerEvent::Completed, this, &AAHCombatPlayerCharacter::StopADS);
	EnhancedInput->BindAction(PlatformManager->GetReloadAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::Reload);
	EnhancedInput->BindAction(PlatformManager->GetSprintAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::StartSprint);
	EnhancedInput->BindAction(PlatformManager->GetSprintAction(), ETriggerEvent::Completed, this, &AAHCombatPlayerCharacter::StopSprint);
	EnhancedInput->BindAction(PlatformManager->GetCrouchAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::StartCrouch);
	EnhancedInput->BindAction(PlatformManager->GetCrouchAction(), ETriggerEvent::Completed, this, &AAHCombatPlayerCharacter::StopCrouch);
	EnhancedInput->BindAction(PlatformManager->GetMeleeAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::Melee);
	EnhancedInput->BindAction(PlatformManager->GetGrenadeAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::ThrowGrenade);
	EnhancedInput->BindAction(PlatformManager->GetInteractAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::Interact);
	EnhancedInput->BindAction(PlatformManager->GetWeaponNextAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::NextWeapon);
	EnhancedInput->BindAction(PlatformManager->GetWeaponPreviousAction(), ETriggerEvent::Started, this, &AAHCombatPlayerCharacter::PreviousWeapon);
}

void AAHCombatPlayerCharacter::DoMove(float Right, float Forward)
{
	if (bSprinting && (Forward <= 0.1f || bCrouched))
	{
		bSprinting = false;
	}
	Super::DoMove(Right, Forward);
}

void AAHCombatPlayerCharacter::DoJumpStart()
{
	if (!TryMantle())
	{
		Super::DoJumpStart();
	}
}

void AAHCombatPlayerCharacter::DoJumpEnd()
{
	Super::DoJumpEnd();
}

float AAHCombatPlayerCharacter::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	return bGodMode ? 0.0f : Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);
}

void AAHCombatPlayerCharacter::StartFire()
{
	if (CombatComponent)
	{
		CombatComponent->StartFire();
	}
}

void AAHCombatPlayerCharacter::StopFire()
{
	if (CombatComponent)
	{
		CombatComponent->StopFire();
	}
}

void AAHCombatPlayerCharacter::StartADS()
{
	if (CombatComponent)
	{
		CombatComponent->StartADS();
	}
}

void AAHCombatPlayerCharacter::StopADS()
{
	if (CombatComponent)
	{
		CombatComponent->StopADS();
	}
}

void AAHCombatPlayerCharacter::Reload()
{
	if (CombatComponent)
	{
		CombatComponent->Reload();
	}
}

void AAHCombatPlayerCharacter::StartSprint()
{
	if (!bCrouched && GetVelocity().SizeSquared2D() > 1.0f)
	{
		bSprinting = true;
	}
}

void AAHCombatPlayerCharacter::StopSprint()
{
	bSprinting = false;
}

void AAHCombatPlayerCharacter::StartCrouch()
{
	bCrouched = true;
	Crouch();
}

void AAHCombatPlayerCharacter::StopCrouch()
{
	bCrouched = false;
	UnCrouch();
}

void AAHCombatPlayerCharacter::Melee()
{
	if (CombatComponent)
	{
		CombatComponent->Melee();
	}
}

void AAHCombatPlayerCharacter::ThrowGrenade()
{
	if (CombatComponent)
	{
		CombatComponent->ThrowGrenade();
	}
}

void AAHCombatPlayerCharacter::Interact()
{
	if (CombatComponent)
	{
		CombatComponent->Interact();
	}
}

void AAHCombatPlayerCharacter::NextWeapon()
{
	if (CombatComponent)
	{
		CombatComponent->CycleWeapon(1);
	}
}

void AAHCombatPlayerCharacter::PreviousWeapon()
{
	if (CombatComponent)
	{
		CombatComponent->CycleWeapon(-1);
	}
}

void AAHCombatPlayerCharacter::OnDeathStarted()
{
	bSprinting = false;
	StopFire();
	Super::OnDeathStarted();
}

bool AAHCombatPlayerCharacter::TryMantle()
{
	if (!Controller || GetCharacterMovement()->IsMovingOnGround() == false)
	{
		return false;
	}

	const FVector Start = GetWeaponTraceOrigin();
	const FVector Forward = GetControlRotation().Vector();
	const FVector ObstacleEnd = Start + Forward * MantleDistance;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHMantle), true, this);
	FHitResult ObstacleHit;
	if (!GetWorld()->LineTraceSingleByChannel(ObstacleHit, Start, ObstacleEnd, ECC_Visibility, Params))
	{
		return false;
	}

	const FVector LedgeProbe = ObstacleHit.ImpactPoint + Forward * 28.0f + FVector(0.0f, 0.0f, 115.0f);
	FHitResult LedgeHit;
	if (GetWorld()->LineTraceSingleByChannel(LedgeHit, LedgeProbe, LedgeProbe - FVector(0.0f, 0.0f, 180.0f), ECC_Visibility, Params))
	{
		const FVector Target = LedgeHit.ImpactPoint + FVector(0.0f, 0.0f, 98.0f);
		SetActorLocation(Target, false, nullptr, ETeleportType::TeleportPhysics);
		return true;
	}
	return false;
}

void AAHCombatPlayerCharacter::RefreshMovementSpeed()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = bCrouched ? CrouchSpeed : (bSprinting ? SprintSpeed : WalkSpeed);
	}
}
