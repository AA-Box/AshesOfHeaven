#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputActionValue.h"
#include "Materials/MaterialInterface.h"

AAHCombatPlayerCharacter::AAHCombatPlayerCharacter()
{
	Faction = EAHFaction::Player;
	bDestroyOnDeath = false;
	HealthComponent->MaxHealth = 100.0f;
	// 200 effective health against two attackers at roughly 60 damage a second is a few seconds
	// of exposure, and armour was the only part that came back - health was a one-way clock, so
	// a long fight killed the player however well it was played. Armour recharges after 4s at
	// 18/s; health now follows it 5s later at 20/s, so about ten seconds out of contact is a
	// full reset and only staying in the open is lethal. These two are the survivability knobs:
	// raise RegenerationPerSecond or drop RegenerationDelay to soften the opening.
	HealthComponent->RegenerationDelay = 5.0f;
	HealthComponent->RegenerationPerSecond = 20.0f;
	ArmorComponent->MaxArmor = 100.0f;
	GetCharacterMovement()->AirControl = MilitaryAirControl;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;

	StartingWeaponClass = AAHWeaponBase::StaticClass();

	UStaticMesh* GauntletMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Ashes/Presentation/Meshes/SM_AH_Cube.SM_AH_Cube"));
	UMaterialInterface* GauntletMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"));
	FirstPersonLeftGauntlet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonLeftGauntlet"));
	FirstPersonLeftGauntlet->SetupAttachment(GetFirstPersonCameraComponent());
	FirstPersonLeftGauntlet->SetStaticMesh(GauntletMesh);
	FirstPersonLeftGauntlet->SetRelativeLocation(FVector(32.0f, -18.0f, -16.0f));
	FirstPersonLeftGauntlet->SetRelativeRotation(FRotator(0.0f, -8.0f, 8.0f));
	FirstPersonLeftGauntlet->SetRelativeScale3D(FVector(0.20f, 0.15f, 0.52f));
	FirstPersonLeftGauntlet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonLeftGauntlet->SetOnlyOwnerSee(true);
	FirstPersonLeftGauntlet->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	// These cubes were only an authoring placeholder. They sit directly on the camera and read
	// as giant black slabs at runtime, so keep the components available for a future real arm mesh
	// but never expose them in the playable presentation.
	FirstPersonLeftGauntlet->SetVisibility(false);
	if (GauntletMaterial)
	{
		FirstPersonLeftGauntlet->SetMaterial(0, GauntletMaterial);
	}

	FirstPersonRightGauntlet = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FirstPersonRightGauntlet"));
	FirstPersonRightGauntlet->SetupAttachment(GetFirstPersonCameraComponent());
	FirstPersonRightGauntlet->SetStaticMesh(GauntletMesh);
	FirstPersonRightGauntlet->SetRelativeLocation(FVector(40.0f, 18.0f, -13.0f));
	FirstPersonRightGauntlet->SetRelativeRotation(FRotator(0.0f, 8.0f, -8.0f));
	FirstPersonRightGauntlet->SetRelativeScale3D(FVector(0.22f, 0.16f, 0.55f));
	FirstPersonRightGauntlet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FirstPersonRightGauntlet->SetOnlyOwnerSee(true);
	FirstPersonRightGauntlet->FirstPersonPrimitiveType = EFirstPersonPrimitiveType::FirstPerson;
	FirstPersonRightGauntlet->SetVisibility(false);
	if (GauntletMaterial)
	{
		FirstPersonRightGauntlet->SetMaterial(0, GauntletMaterial);
	}
}

void AAHCombatPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	// The OS mouse-capture warp on launch arrives as one huge look delta in the first
	// frames and rails the camera pitch into the sky before the player ever touches the
	// mouse. Swallow look input for a moment so every fresh spawn faces the authored
	// first frame.
	LookInputEnableTime = GetWorld() ? GetWorld()->GetTimeSeconds() + 0.35f : 0.0f;
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
	++TicksSinceBeginPlay;
	// RefreshMovementSpeed publishes this stance's stride and mix; the base class walks the
	// distance accumulator and plays the step. There is one footstep implementation, and every
	// combatant - player and AI alike - runs it.
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

void AAHCombatPlayerCharacter::DoAim(float Yaw, float Pitch)
{
	// The 0.35s window alone is not enough: a long blocking hitch during startup (level
	// streaming flush) can consume the whole window in one clamped tick, letting the OS
	// mouse-capture warp rail the pitch afterwards. Require real rendered frames too.
	if (TicksSinceBeginPlay < 3 || (GetWorld() && GetWorld()->GetTimeSeconds() < LookInputEnableTime))
	{
		return;
	}
	Super::DoAim(Yaw, Pitch);
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

void AAHCombatPlayerCharacter::SetFirstPersonPresentationVisible(bool bVisible)
{
	// Do not resurrect the cube gauntlets. Only the authored weapon is part of the current
	// first-person target and it is explicitly hidden during the opening dialogue.
	if (FirstPersonLeftGauntlet)
	{
		FirstPersonLeftGauntlet->SetVisibility(false);
	}
	if (FirstPersonRightGauntlet)
	{
		FirstPersonRightGauntlet->SetVisibility(false);
	}
	if (InventoryComponent)
	{
		if (AAHWeaponBase* Weapon = InventoryComponent->GetCurrentWeapon())
		{
			Weapon->SetLocalPresentationVisible(bVisible);
		}
	}
}

void AAHCombatPlayerCharacter::OnDeathStarted()
{
	bSprinting = false;
	FootstepDistanceRemaining = 0.0f;
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
	FootstepStride = bCrouched ? CrouchStride : (bSprinting ? SprintStride : WalkStride);
	// Crouching used to be as loud as walking; the whole point of it is not being heard.
	FootstepVolume = bCrouched ? 0.45f : (bSprinting ? 0.85f : 0.62f);
	FootstepPitch = bCrouched ? 0.82f : (bSprinting ? 1.08f : 1.0f);
}
