#include "Gameplay/Vehicles/AHManticoreVehicle.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Characters/AHCombatPlayerCharacter.h"
#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/StaticMeshActor.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"

AAHManticoreVehicle::AAHManticoreVehicle()
{
	PrimaryActorTick.bCanEverTick = true;
	VehicleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleMesh"));
	RootComponent = VehicleMesh;
	VehicleMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleMesh->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleMesh->SetRelativeScale3D(FVector(2.8f, 1.45f, 0.75f));

	const UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	const UStaticMesh* CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	HullArmor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HullArmor"));
	HullArmor->SetupAttachment(VehicleMesh);
	HullArmor->SetStaticMesh(const_cast<UStaticMesh*>(CubeMesh));
	HullArmor->SetRelativeLocation(FVector(35.0f, 0.0f, 82.0f));
	HullArmor->SetRelativeScale3D(FVector(1.65f, 0.92f, 0.26f));
	HullArmor->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TurretAssembly = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TurretAssembly"));
	TurretAssembly->SetupAttachment(VehicleMesh);
	TurretAssembly->SetStaticMesh(const_cast<UStaticMesh*>(CylinderMesh));
	TurretAssembly->SetRelativeLocation(FVector(52.0f, 0.0f, 112.0f));
	TurretAssembly->SetRelativeScale3D(FVector(0.68f, 0.68f, 0.25f));
	TurretAssembly->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MountedWeapon = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MountedWeapon"));
	MountedWeapon->SetupAttachment(VehicleMesh);
	MountedWeapon->SetStaticMesh(const_cast<UStaticMesh*>(CubeMesh));
	MountedWeapon->SetRelativeLocation(FVector(122.0f, 0.0f, 116.0f));
	MountedWeapon->SetRelativeScale3D(FVector(1.35f, 0.14f, 0.14f));
	MountedWeapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	for (int32 Index = 0; Index < 4; ++Index)
	{
		UStaticMeshComponent* Wheel = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("WheelVisual_%d"), Index));
		Wheel->SetupAttachment(VehicleMesh);
		Wheel->SetStaticMesh(const_cast<UStaticMesh*>(CylinderMesh));
		Wheel->SetRelativeLocation(FVector(Index < 2 ? 72.0f : -72.0f, Index % 2 == 0 ? -118.0f : 118.0f, -38.0f));
		Wheel->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		Wheel->SetRelativeScale3D(FVector(0.35f, 0.35f, 0.18f));
		Wheel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		WheelVisuals.Add(Wheel);
	}

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 620.0f;
	CameraBoom->SetRelativeLocation(FVector(-20.0f, 0.0f, 130.0f));
	CameraBoom->SetRelativeRotation(FRotator(-12.0f, 0.0f, 0.0f));
	CameraBoom->bUsePawnControlRotation = true;

	VehicleCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VehicleCamera"));
	VehicleCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	VehicleCamera->bUsePawnControlRotation = false;

	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	Health = MaxHealth;
}

void AAHManticoreVehicle::BeginPlay()
{
	Super::BeginPlay();
	Health = FMath::Clamp(Health, 0.0f, MaxHealth);
}

void AAHManticoreVehicle::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bDestroyed || !Driver.IsValid())
	{
		return;
	}

	const float TargetSpeed = Throttle * MaxSpeed;
	const float Rate = FMath::Abs(TargetSpeed) > FMath::Abs(CurrentSpeed) ? Acceleration : BrakeStrength;
	CurrentSpeed = FMath::FInterpTo(CurrentSpeed, TargetSpeed, DeltaSeconds, Rate / FMath::Max(1.0f, MaxSpeed));
	const float SpeedRatio = FMath::Clamp(FMath::Abs(CurrentSpeed) / MaxSpeed, 0.0f, 1.0f);
	AddActorWorldRotation(FRotator(0.0f, Steering * TurnRate * SpeedRatio * DeltaSeconds, 0.0f));

	const FVector DesiredLocation = GetActorLocation() + GetActorForwardVector() * CurrentSpeed * DeltaSeconds;
	SetActorLocation(DesiredLocation, true);
	SuspensionCompression = FMath::Sin(GetWorld()->GetTimeSeconds() * 9.0f) * SpeedRatio * 4.0f;
	VehicleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, SuspensionCompression));
}

void AAHManticoreVehicle::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	if (!EnhancedInput || !Platform)
	{
		return;
	}

	EnhancedInput->BindAction(Platform->GetMoveAction(), ETriggerEvent::Triggered, this, &AAHManticoreVehicle::HandleMoveInput);
	EnhancedInput->BindAction(Platform->GetMoveAction(), ETriggerEvent::Completed, this, &AAHManticoreVehicle::HandleMoveInput);
	EnhancedInput->BindAction(Platform->GetLookAction(), ETriggerEvent::Triggered, this, &AAHManticoreVehicle::HandleLookInput);
	EnhancedInput->BindAction(Platform->GetMouseLookAction(), ETriggerEvent::Triggered, this, &AAHManticoreVehicle::HandleLookInput);
	EnhancedInput->BindAction(Platform->GetFireAction(), ETriggerEvent::Started, this, &AAHManticoreVehicle::HandleFireStarted);
	EnhancedInput->BindAction(Platform->GetVehicleExitAction(), ETriggerEvent::Started, this, &AAHManticoreVehicle::HandleExitStarted);
	EnhancedInput->BindAction(Platform->GetVehicleSwitchSeatAction(), ETriggerEvent::Started, this, &AAHManticoreVehicle::HandleSeatSwitchStarted);
}

float AAHManticoreVehicle::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser)
{
	if (bDestroyed)
	{
		return 0.0f;
	}
	Health = FMath::Max(0.0f, Health - FMath::Max(0.0f, DamageAmount));
	if (Health <= 0.0f)
	{
		DestroyVehicle();
	}
	return DamageAmount;
}

void AAHManticoreVehicle::Interact_Implementation(AActor* Interactor)
{
	if (bDestroyed)
	{
		return;
	}
	if (Driver.IsValid())
	{
		ExitVehicle();
	}
	else if (AAHCombatPlayerCharacter* Player = Cast<AAHCombatPlayerCharacter>(Interactor))
	{
		EnterVehicle(Player);
	}
}

FText AAHManticoreVehicle::GetInteractionPrompt_Implementation() const
{
	if (bDestroyed)
	{
		return FText::FromString(TEXT("MANTICORE DESTROYED"));
	}
	return Driver.IsValid() ? FText::FromString(TEXT("INTERACT  EXIT MANTICORE")) : FText::FromString(TEXT("INTERACT  ENTER MANTICORE"));
}

bool AAHManticoreVehicle::EnterVehicle(AAHCombatPlayerCharacter* Player)
{
	if (!Player || Driver.IsValid() || bDestroyed)
	{
		return false;
	}
	AController* PlayerController = Player->GetController();
	if (!PlayerController)
	{
		return false;
	}

	Driver = Player;
	Player->SetActorHiddenInGame(true);
	Player->SetActorEnableCollision(false);
	Player->SetActorLocation(GetActorLocation() + FVector(0.0f, 0.0f, 90.0f));
	PlayerController->Possess(this);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Manticore] enter driver=%s location=%s"), *GetNameSafe(Player), *GetActorLocation().ToCompactString());
	#endif
	OnDriverEntered.Broadcast(Player);
	return true;
}

void AAHManticoreVehicle::ExitVehicle()
{
	AAHCombatPlayerCharacter* Player = Driver.Get();
	AController* PlayerController = Player ? Player->GetController() : GetController();
	if (!Player || !PlayerController)
	{
		Driver.Reset();
		return;
	}

	Player->SetActorLocation(GetActorLocation() - GetActorRightVector() * 180.0f + FVector(0.0f, 0.0f, 100.0f));
	Player->SetActorHiddenInGame(false);
	Player->SetActorEnableCollision(true);
	Driver.Reset();
	PlayerController->Possess(Player);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Manticore] exit driver=%s location=%s"), *GetNameSafe(Player), *GetActorLocation().ToCompactString());
	#endif
	OnDriverExited.Broadcast(Player);
}

void AAHManticoreVehicle::FireMountedWeapon()
{
	if (bDestroyed || !Driver.IsValid() || !VehicleCamera)
	{
		return;
	}
	const FVector Start = VehicleCamera->GetComponentLocation();
	const FVector End = Start + VehicleCamera->GetForwardVector() * 5000.0f;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(AHManticoreGun), true, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) && Hit.GetActor())
	{
		UGameplayStatics::ApplyDamage(Hit.GetActor(), 55.0f, GetController(), this, nullptr);
		if (AStaticMeshActor* Barricade = Cast<AStaticMeshActor>(Hit.GetActor()))
		{
			Barricade->Destroy();
		}
	}
}

void AAHManticoreVehicle::SetMobileThrottle(float Value)
{
	Throttle = FMath::Clamp(Value, -1.0f, 1.0f);
}

void AAHManticoreVehicle::SetMobileSteering(float Value)
{
	Steering = FMath::Clamp(Value, -1.0f, 1.0f);
}

FAHVehicleState AAHManticoreVehicle::GetVehicleState() const
{
	FAHVehicleState State;
	State.bSpawned = true;
	State.bDestroyed = bDestroyed;
	State.bOccupied = Driver.IsValid();
	State.Health = Health;
	State.Location = GetActorLocation();
	State.Rotation = GetActorRotation();
	return State;
}

void AAHManticoreVehicle::RestoreVehicleState(const FAHVehicleState& State)
{
	SetActorLocationAndRotation(State.Location, State.Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	Health = FMath::Clamp(State.Health, 0.0f, MaxHealth);
	bDestroyed = State.bDestroyed;
	SetActorHiddenInGame(false);
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Manticore] restore location=%s health=%0.1f destroyed=%s"), *State.Location.ToCompactString(), State.Health, State.bDestroyed ? TEXT("true") : TEXT("false"));
	#endif
}

void AAHManticoreVehicle::HandleMoveInput(const FInputActionValue& Value)
{
	const FVector2D Movement = Value.Get<FVector2D>();
	Throttle = FMath::Clamp(Movement.Y, -1.0f, 1.0f);
	Steering = FMath::Clamp(Movement.X, -1.0f, 1.0f);
}

void AAHManticoreVehicle::HandleLookInput(const FInputActionValue& Value)
{
	const FVector2D Look = Value.Get<FVector2D>();
	AddControllerYawInput(Look.X);
	AddControllerPitchInput(Look.Y);
}

void AAHManticoreVehicle::HandleFireStarted()
{
	FireMountedWeapon();
}

void AAHManticoreVehicle::HandleExitStarted()
{
	ExitVehicle();
}

void AAHManticoreVehicle::HandleSeatSwitchStarted()
{
	// The greybox supports the driver/gunner capability from the same seat.
	FireMountedWeapon();
}

void AAHManticoreVehicle::DestroyVehicle()
{
	if (bDestroyed)
	{
		return;
	}
	bDestroyed = true;
	Throttle = 0.0f;
	CurrentSpeed = 0.0f;
	if (Driver.IsValid())
	{
		ExitVehicle();
	}
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase3.2][Manticore] destroyed"));
	#endif
	OnVehicleDestroyed.Broadcast();
}
