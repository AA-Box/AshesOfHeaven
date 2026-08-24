#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
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

	// A combatant with no skeletal mesh has nothing that blocks ECC_Visibility: the capsule's
	// Pawn profile ignores that channel and the old greybox blocks were NoCollision, so every
	// rifle trace passed straight through everyone and no shot could ever land. The body is
	// what makes a combatant hittable, animated and visible - it is not decoration.
	USkeletalMeshComponent* Body = GetMesh();
	if (USkeletalMesh* BodyMesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")))
	{
		Body->SetSkeletalMesh(BodyMesh);
	}
	// Skeletons are authored facing +Y with the origin at the feet; the capsule's origin is its
	// centre, so the mesh drops by the half height.
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));
	if (UClass* BodyAnimClass = LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C")))
	{
		// This graph drives itself from the pawn's velocity and movement component and casts to
		// no particular character class, so it runs a full idle/walk/run blend plus jump, fall
		// and rifle aim offset on any ACharacter.
		Body->SetAnimInstanceClass(BodyAnimClass);
	}
	// Query only: movement and melee stay on the capsule (ECC_Pawn). The body exists for weapon
	// traces, and SKM_Manny_Simple ships PA_Mannequin, so a hit resolves to a bone name and the
	// headshot multiplier in TakeDamage finally has something to read.
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionObjectType(ECC_Pawn);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetGenerateOverlapEvents(false);
	Body->SetCanEverAffectNavigation(false);
	Body->bReceivesDecals = false;
	// The owner sees the world down the first person camera; their own body would fill it.
	// It still casts a shadow, which is the only part of it they should see.
	Body->SetOwnerNoSee(true);
	Body->bCastHiddenShadow = true;

	// Channel 1 is the character-only lighting channel; channel 0 keeps world lights on them.
	Body->LightingChannels.bChannel0 = true;
	Body->LightingChannels.bChannel1 = true;

	BodyFillLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BodyFill"));
	BodyFillLight->SetupAttachment(GetCapsuleComponent());
	// Overhead and slightly forward. A light in front of the chest only lit the side the
	// combatant happened to be facing, which is rarely the side the player is looking at; a
	// top-down key shapes the head, shoulders and weapon arm from every viewing angle.
	BodyFillLight->SetRelativeLocation(FVector(45.0f, 0.0f, 125.0f));
	BodyFillLight->SetAttenuationRadius(500.0f);
	BodyFillLight->SetIntensityUnits(ELightUnits::Candelas);
	BodyFillLight->SetIntensity(BodyFillIntensity);
	BodyFillLight->SetCastShadows(false);
	BodyFillLight->SetVolumetricScatteringIntensity(0.0f);
	BodyFillLight->bAffectTranslucentLighting = false;
	// One light per body times MaxActiveCombatants (24) is four times the 16-light scene budget,
	// and a 500uu radius covers a few pixels at range anyway. The fade starts at 45m, which is
	// past the far soldier of the Transit line-up (36m) and well past PreferredEngagementRange,
	// so nothing the player is actually reading loses its fill.
	BodyFillLight->MaxDrawDistance = 6000.0f;
	BodyFillLight->MaxDistanceFadeRange = 1500.0f;
	// Lumen ignores lighting channels, so leaving this in the indirect pass is both a cost and
	// the exact street-level bounce this light exists to avoid.
	BodyFillLight->bAffectGlobalIllumination = false;
	// Channel 0 off is what keeps this off the street. Only bodies are on channel 1.
	BodyFillLight->LightingChannels.bChannel0 = false;
	BodyFillLight->LightingChannels.bChannel1 = true;

	// AI-driven combatants aim by control rotation, which is what the rifle aim offset reads.
	bUseControllerRotationYaw = true;
}

void AAHCombatantCharacter::BeginPlay()
{
	Super::BeginPlay();
	// Faction is assigned by the subclass constructor, which runs after this class's, so the
	// skin cannot be picked until the object is fully constructed.
	ApplyFactionAppearance();
	if (BodyFillLight)
	{
		// The fill is one unshadowed point light per body. Mobile budgets four dynamic lights for
		// the whole scene and fields 8-16 combatants, so it does not get them.
		// ponytail: whole-platform switch, not a per-frame nearest-N budget. Add the budget only
		// if mobile art wants fills back on the two or three bodies nearest the camera.
		const UGameInstance* GI = GetGameInstance();
		const UAHPlatformManagerSubsystem* Platform = GI ? GI->GetSubsystem<UAHPlatformManagerSubsystem>() : nullptr;
		BodyFillLight->SetVisibility(!Platform || Platform->GetPerformanceProfile().bCharacterFillLights);
	}
	if (HealthComponent)
	{
		HealthComponent->OnDeath.AddDynamic(this, &AAHCombatantCharacter::HandleHealthDeath);
	}
	if (ArmorComponent)
	{
		ArmorComponent->OnArmorBroken.AddDynamic(this, &AAHCombatantCharacter::HandleArmorBroken);
	}
}

void AAHCombatantCharacter::ApplyFactionAppearance()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetSkeletalMeshAsset())
	{
		return;
	}

	if (BodyFillLight)
	{
		// Faction reads off the fill colour: human issue lighting is warm, Veil is cold.
		BodyFillLight->SetLightColor(Faction == EAHFaction::Veil ? FLinearColor(0.62f, 0.82f, 1.0f) : FLinearColor(1.0f, 0.94f, 0.84f));
		BodyFillLight->SetIntensity(BodyFillIntensity);
	}

	const TArray<TSoftObjectPtr<UMaterialInterface>>& Skin = Faction == EAHFaction::Veil ? VeilBodyMaterials : HumanBodyMaterials;
	for (int32 SlotIndex = 0; SlotIndex < Body->GetNumMaterials(); ++SlotIndex)
	{
		if (!Skin.IsValidIndex(SlotIndex))
		{
			// Fewer skins than slots: leave the remaining slots on the mesh's own materials
			// rather than smearing the last one over parts it was not authored for.
			break;
		}
		if (UMaterialInterface* SkinMaterial = Skin[SlotIndex].LoadSynchronous())
		{
			Body->SetMaterial(SlotIndex, SkinMaterial);
		}
	}
}

void AAHCombatantCharacter::StartRagdoll()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body || !Body->GetSkeletalMeshAsset() || !Body->GetPhysicsAsset() || Body->IsSimulatingPhysics())
	{
		return;
	}

	// The Ragdoll profile keeps blocking ECC_Visibility, which is what lets the interaction
	// trace find the body afterwards - a corpse you cannot look at is a corpse you cannot loot.
	Body->SetCollisionProfileName(TEXT("Ragdoll"));
	Body->SetAllBodiesBelowSimulatePhysics(TEXT("pelvis"), true, true);
	Body->SetSimulatePhysics(true);
	Body->WakeAllRigidBodies();
}

AAHWeaponBase* AAHCombatantCharacter::GetLootableWeapon() const
{
	if (!IsCombatantDead() || !InventoryComponent)
	{
		return nullptr;
	}
	AAHWeaponBase* Weapon = InventoryComponent->GetCurrentWeapon();
	return IsValid(Weapon) ? Weapon : nullptr;
}

FText AAHCombatantCharacter::GetInteractionPrompt_Implementation() const
{
	const AAHWeaponBase* Weapon = GetLootableWeapon();
	if (!Weapon)
	{
		// An empty prompt is how an interactable says "not right now"; the interaction component
		// treats it as no target, so a living enemy never offers to be looted.
		return FText::GetEmpty();
	}
	return FText::Format(NSLOCTEXT("AshesOfHeaven", "TakeWeaponPrompt", "E — TAKE {0}"), Weapon->DisplayName);
}

float AAHCombatantCharacter::GetInteractionPriority_Implementation() const
{
	// Lootable bodies remain selectable, but a deliberately dropped weapon should win when the
	// two occupy the same screen space. This value belongs to the actor, not the selector.
	return GetLootableWeapon() ? 0.20f : 0.0f;
}

void AAHCombatantCharacter::Interact_Implementation(AActor* Interactor)
{
	AAHCombatantCharacter* Looter = Cast<AAHCombatantCharacter>(Interactor);
	AAHWeaponBase* Weapon = GetLootableWeapon();
	if (!Looter || !Looter->GetInventoryComponent() || !Weapon)
	{
		return;
	}

	// What the dead soldier had left, not a fresh magazine.
	const FAHAmmoState LootedAmmo = Weapon->GetAmmoState();
	UAHInventoryComponent* LooterInventory = Looter->GetInventoryComponent();

	AAHWeaponBase* AlreadyCarried = nullptr;
	for (AAHWeaponBase* Carried : LooterInventory->GetWeapons())
	{
		if (IsValid(Carried) && Carried->GetClass() == Weapon->GetClass())
		{
			AlreadyCarried = Carried;
			break;
		}
	}

	if (AlreadyCarried)
	{
		// Carrying this weapon already, so the body is worth its ammunition rather than a duplicate.
		AlreadyCarried->AddReserveAmmo(LootedAmmo.Magazine + LootedAmmo.Reserve);
	}
	else if (AAHWeaponBase* Taken = LooterInventory->AddWeaponClass(Weapon->GetClass()))
	{
		Taken->SetAmmoState(LootedAmmo);
	}
	else
	{
		return;
	}

	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Pickup] looted weapon=%s from=%s ammo=%d/%d"),
		*Weapon->WeaponId.ToString(), *GetName(), LootedAmmo.Magazine, LootedAmmo.Reserve);
	#endif

	// Strip the body: the rifle leaves its hands and it stops offering a prompt.
	InventoryComponent->DiscardWeapon(Weapon);
	Weapon->Destroy();

	if (UAHAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UAHAudioSubsystem>() : nullptr)
	{
		Audio->PlayWorldCue(EAHAudioCue::Pickup, GetActorLocation(), 0.7f);
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
	#if !UE_BUILD_SHIPPING
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase3.2][Combat] death actor=%s faction=%s"), *GetName(), *GetFactionName().ToString());
	#endif
	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (CombatComponent)
	{
		CombatComponent->DisableCombat();
	}
	// Without this the body keeps standing in its idle loop and a kill reads as a bug.
	StartRagdoll();
	OnCombatantDeath.Broadcast();
	if (bDestroyOnDeath)
	{
		// Three seconds cut the corpse away before it finished falling.
		SetLifeSpan(CorpseLifeSpan);
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
