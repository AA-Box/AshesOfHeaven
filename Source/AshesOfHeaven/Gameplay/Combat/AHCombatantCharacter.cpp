#include "Gameplay/Combat/AHCombatantCharacter.h"
#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Audio/AHAudioPaletteData.h"
#include "Gameplay/Combat/AHArmorComponent.h"
#include "Gameplay/Combat/AHCombatComponent.h"
#include "Gameplay/Combat/AHCorpseManagerSubsystem.h"
#include "Gameplay/Combat/AHHealthComponent.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Combat/AHInventoryComponent.h"
#include "Gameplay/Enemies/AHEnemyAssetSubsystem.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Gameplay/Weapons/AHWeaponBase.h"
#include "Gameplay/AI/AHCombatAIController.h"
#include "Platform/AHPlatformManagerSubsystem.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimationAsset.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Engine/DamageEvents.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "TimerManager.h"

AAHCombatantCharacter::AAHCombatantCharacter()
{
	HealthComponent = CreateDefaultSubobject<UAHHealthComponent>(TEXT("Health"));
	ArmorComponent = CreateDefaultSubobject<UAHArmorComponent>(TEXT("Armor"));
	CombatComponent = CreateDefaultSubobject<UAHCombatComponent>(TEXT("Combat"));
	InteractionComponent = CreateDefaultSubobject<UAHInteractionComponent>(TEXT("Interaction"));
	InventoryComponent = CreateDefaultSubobject<UAHInventoryComponent>(TEXT("Inventory"));

	GetCharacterMovement()->MaxWalkSpeed = 460.0f;

	// Presentation is assigned by an asynchronously loaded enemy definition (or by the legacy
	// async fallback for player/friendly classes). Constructors must never force heavy packages.
	USkeletalMeshComponent* Body = GetMesh();
	// Skeletons are authored facing +Y with the origin at the feet; the capsule's origin is its
	// centre, so the mesh drops by the half height.
	Body->SetRelativeLocationAndRotation(FVector(0.0f, 0.0f, -96.0f), FRotator(0.0f, -90.0f, 0.0f));
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
	BodyFillLight->SetLightColor(BodyFillColor);
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

	if (EnemyDefinition)
	{
		ApplyDefinitionLoadout();
		if (SpawnEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SpawnEffect, GetActorLocation(), GetActorRotation());
		}
	}
	else if (const FPrimaryAssetId DefaultEnemyId = GetDefaultEnemyDefinitionId(); DefaultEnemyId.IsValid())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UAHEnemyAssetSubsystem* Assets = GameInstance->GetSubsystem<UAHEnemyAssetSubsystem>())
			{
				SelfAssetLease = Assets->PreloadEnemyAssets(
					{ DefaultEnemyId },
					Assets->BuildBundlesForCurrentPlatform(true, true),
					FName(*FString::Printf(TEXT("DirectSpawn.%s"), *GetName())),
					FAHEnemyAssetsReady::CreateUObject(this, &ThisClass::HandleSelfEnemyAssetsReady));
			}
		}
	}
	else
	{
		RequestLegacyPresentationAsync();
	}
}

void AAHCombatantCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (SelfAssetLease.IsValid())
	{
		if (UGameInstance* GameInstance = GetGameInstance())
		{
			if (UAHEnemyAssetSubsystem* Assets = GameInstance->GetSubsystem<UAHEnemyAssetSubsystem>())
			{
				Assets->ReleaseEncounterAssets(SelfAssetLease);
			}
		}
		SelfAssetLease.Invalidate();
	}
	if (LegacyPresentationHandle.IsValid() && !LegacyPresentationHandle->HasLoadCompleted())
	{
		LegacyPresentationHandle->CancelHandle();
	}
	LegacyPresentationHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AAHCombatantCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	ApplyDefinitionToController();
}

void AAHCombatantCharacter::ApplyEnemyDefinition(UAHEnemyDefinition* Definition)
{
	if (!Definition)
	{
		return;
	}
	EnemyDefinition = Definition;
	Faction = Definition->CombatDefaults.Faction;
	HeadshotMultiplier = Definition->CombatDefaults.HeadshotMultiplier;
	bDestroyOnDeath = Definition->CombatDefaults.bDestroyOnDeath;
	CorpseLifeSpan = Definition->CombatDefaults.CorpseLifeSpan;
	if (HealthComponent)
	{
		HealthComponent->MaxHealth = Definition->CombatDefaults.MaxHealth * Definition->Difficulty.HealthScale;
		if (HasActorBegunPlay()) HealthComponent->ResetHealth();
	}
	if (ArmorComponent)
	{
		ArmorComponent->MaxArmor = Definition->CombatDefaults.MaxArmor * Definition->Difficulty.ArmorScale;
		if (HasActorBegunPlay()) ArmorComponent->ResetArmor();
	}
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = Definition->CombatDefaults.WalkSpeed;
	}
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		// Applied before the mesh offset below, because the offset is expressed against the
		// capsule centre and a creature sized to the mannequin capsule either floats or sinks.
		const float HalfHeight = Definition->CombatDefaults.CapsuleHalfHeight > 0.0f
			? Definition->CombatDefaults.CapsuleHalfHeight : Capsule->GetUnscaledCapsuleHalfHeight();
		const float Radius = Definition->CombatDefaults.CapsuleRadius > 0.0f
			? Definition->CombatDefaults.CapsuleRadius : Capsule->GetUnscaledCapsuleRadius();
		Capsule->SetCapsuleSize(Radius, HalfHeight, true);
	}
	if (UClass* ControllerClass = Definition->AISettings.ControllerClass.Get())
	{
		AIControllerClass = ControllerClass;
		AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	}
	if (CombatComponent && Definition->AISettings.bMeleeOnly)
	{
		// The bite numbers live with the rest of the archetype's AI tuning; the component is
		// where they have to end up, because it owns the sweep and the cooldown timer.
		CombatComponent->MeleeDamage = Definition->AISettings.MeleeDamage;
		CombatComponent->MeleeRange = Definition->AISettings.MeleeRange;
		CombatComponent->MeleeRadius = Definition->AISettings.MeleeRadius;
		CombatComponent->MeleeCooldown = Definition->AISettings.MeleeCooldown;
	}

	const UAHPlatformManagerSubsystem* Platform = UAHPlatformManagerSubsystem::Get(this);
	const bool bMobile = Platform && Platform->GetCapabilities().bIsMobile;
	const FAHEnemyVisualPayload Visuals = Definition->ResolveVisuals(bMobile);
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (USkeletalMesh* Mesh = Visuals.SkeletalMesh.Get()) Body->SetSkeletalMesh(Mesh);
		if (UClass* AnimClass = Visuals.AnimClass.Get())
		{
			Body->SetAnimInstanceClass(AnimClass);
		}
		else if (UAnimationAsset* Clip = Visuals.AnimationSet.IsEmpty() ? nullptr : Visuals.AnimationSet[0].Get())
		{
			// Creature meshes arrive with their own skeleton, so the mannequin AnimBP cannot
			// drive them and there is no retarget to borrow. Loop the archetype's first authored
			// clip in single-node mode: a moving body beats a T-pose.
			// ponytail: no state machine, so no hit reacts or attack poses. Author a per-skeleton
			// AnimBP when a creature needs to visibly telegraph its bite.
			Body->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			Body->PlayAnimation(Clip, true);
		}
		if (UPhysicsAsset* PhysicsAsset = Visuals.PhysicsAsset.Get()) Body->SetPhysicsAsset(PhysicsAsset, true);
		Body->SetRelativeScale3D(Visuals.MeshScale);
		if (Visuals.bOverrideMeshTransform)
		{
			Body->SetRelativeLocationAndRotation(Visuals.MeshOffset, Visuals.MeshRotation);
		}
		for (int32 Index = 0; Index < Visuals.Materials.Num(); ++Index)
		{
			if (UMaterialInterface* Material = Visuals.Materials[Index].Get()) ApplyBodyPaint(Body, Index, Material);
		}
	}
	const FAHEnemyAudioPayload AudioPayload = Definition->ResolveAudio(bMobile);
	VoicePalette = AudioPayload.VoicePalette.Get();
	HurtSound = AudioPayload.HurtSound.Get();
	ArmorDamageSound = AudioPayload.ArmorDamageSound.Get();
	DeathSound = AudioPayload.DeathSound.Get();
	const FAHEnemyVFXPayload VFXPayload = Definition->ResolveVFX(bMobile);
	SpawnEffect = VFXPayload.SpawnEffect.Get();
	DeathEffect = VFXPayload.DeathEffect.Get();

	ApplyFactionAppearance();
	ApplyDefinitionToController();
	if (HasActorBegunPlay())
	{
		ApplyDefinitionLoadout();
		if (SpawnEffect)
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, SpawnEffect, GetActorLocation(), GetActorRotation());
		}
	}
}

void AAHCombatantCharacter::ApplyDefinitionLoadout()
{
	if (!EnemyDefinition || !InventoryComponent || !InventoryComponent->GetWeapons().IsEmpty())
	{
		return;
	}
	for (const TSoftClassPtr<AAHWeaponBase>& WeaponClass : EnemyDefinition->Loadout.WeaponClasses)
	{
		if (UClass* LoadedClass = WeaponClass.Get())
		{
			if (AAHWeaponBase* Weapon = InventoryComponent->AddWeaponClass(LoadedClass))
			{
				Weapon->ApplyStreamedLoadout(EnemyDefinition->Loadout);
			}
		}
	}
}

void AAHCombatantCharacter::ApplyDefinitionToController()
{
	if (EnemyDefinition)
	{
		if (AAHCombatAIController* AI = Cast<AAHCombatAIController>(GetController()))
		{
			AI->ApplyEnemySettings(EnemyDefinition->AISettings);
		}
	}
}

FPrimaryAssetId AAHCombatantCharacter::GetDefaultEnemyDefinitionId() const
{
	return FPrimaryAssetId();
}

void AAHCombatantCharacter::HandleSelfEnemyAssetsReady(
	FGuid RequestId,
	bool bSuccess,
	const TArray<UAHEnemyDefinition*>& Definitions,
	const FString& Error)
{
	if (bSuccess && !Definitions.IsEmpty())
	{
		ApplyEnemyDefinition(Definitions[0]);
	}
	else
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Assets] direct enemy spawn failed actor=%s error=%s"), *GetName(), *Error);
	}
}

void AAHCombatantCharacter::RequestLegacyPresentationAsync()
{
	if (GetMesh() && GetMesh()->GetSkeletalMeshAsset())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths {
		FSoftObjectPath(TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")),
		FSoftObjectPath(TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C"))
	};
	for (const TSoftObjectPtr<UMaterialInterface>& Material : HumanBodyMaterials) Paths.AddUnique(Material.ToSoftObjectPath());
	for (const TSoftObjectPtr<UMaterialInterface>& Material : VeilBodyMaterials) Paths.AddUnique(Material.ToSoftObjectPath());
	LegacyPresentationHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::HandleLegacyPresentationLoaded));
}

void AAHCombatantCharacter::HandleLegacyPresentationLoaded()
{
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->SetSkeletalMesh(Cast<USkeletalMesh>(FSoftObjectPath(
			TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple")).ResolveObject()));
		Body->SetAnimInstanceClass(Cast<UClass>(FSoftObjectPath(
			TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C")).ResolveObject()));
	}
	ApplyFactionAppearance();
	LegacyPresentationHandle.Reset();
}

void AAHCombatantCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	const UCharacterMovementComponent* Movement = GetCharacterMovement();
	const float GroundSpeed = GetVelocity().Size2D();
	if (IsCombatantDead() || !Movement || !Movement->IsMovingOnGround() || GroundSpeed <= FootstepMinimumSpeed)
	{
		// Owe a third of a stride on the next start: the first step lands promptly when the body
		// sets off, without a tapped movement key firing one step per tap.
		FootstepDistanceRemaining = FMath::Max(FootstepDistanceRemaining, FootstepStride * 0.35f);
		return;
	}

	FootstepDistanceRemaining -= GroundSpeed * DeltaSeconds;
	if (FootstepDistanceRemaining <= 0.0f)
	{
		PlayFootstep(1.0f, bNextFootIsLeft ? 1.03f : 0.97f);
		bNextFootIsLeft = !bNextFootIsLeft;
		// Carry the overshoot instead of resetting, so cadence stays exact across frame times.
		FootstepDistanceRemaining += FMath::Max(20.0f, FootstepStride);
		if (FootstepDistanceRemaining <= 0.0f)
		{
			// One frame covered more than a whole stride - a teleport or a huge hitch, not a walk.
			FootstepDistanceRemaining = FMath::Max(20.0f, FootstepStride);
		}
	}
}

void AAHCombatantCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);
	if (IsCombatantDead())
	{
		return;
	}

	// Velocity still holds the fall when ProcessLanded calls this. If something has already
	// zeroed it, the mapping floors at the light end rather than dropping the sound entirely.
	const float FallSpeed = FMath::Abs(GetVelocity().Z);
	const float Weight = FMath::Lerp(0.9f, 1.6f, FMath::Clamp((FallSpeed - 250.0f) / 650.0f, 0.0f, 1.0f));
	PlayFootstep(Weight, 0.78f);
	// The landing is the step. Do not follow it with another one half a metre later.
	FootstepDistanceRemaining = FMath::Max(20.0f, FootstepStride) * 0.6f;
}

void AAHCombatantCharacter::PlayFootstep(float VolumeScale, float PitchScale)
{
	UAHAudioSubsystem* Audio = GetWorld() ? GetWorld()->GetSubsystem<UAHAudioSubsystem>() : nullptr;
	if (!Audio)
	{
		return;
	}

	// At the actor origin a step comes from the middle of the chest. Boots are at the bottom of
	// the capsule, which is also where attenuation should be measured from.
	const float HalfHeight = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleHalfHeight() : 0.0f;
	const FVector Feet = GetActorLocation() - FVector(0.0f, 0.0f, HalfHeight);
	// One sample at one pitch every step reads as a click track. Jitter plus alternating feet is
	// what a variation set would do, without needing four more assets and four palette entries.
	const float Volume = FMath::Max(0.0f, FootstepVolume * VolumeScale * FMath::FRandRange(0.92f, 1.08f));
	const float Pitch = FMath::Max(0.1f, FootstepPitch * PitchScale * FMath::FRandRange(0.96f, 1.04f));
	Audio->PlayWorldCue(EAHAudioCue::Footstep, Feet, Volume, Pitch);
	UE_LOG(LogAshesOfHeaven, Verbose, TEXT("[Audio] footstep actor=%s speed=%.0f stride=%.0f volume=%.2f pitch=%.2f"),
		*GetName(), GetVelocity().Size2D(), FootstepStride, Volume, Pitch);
}

void AAHCombatantCharacter::ApplyBodyPaint(USkeletalMeshComponent* Body, int32 SlotIndex, UMaterialInterface* Source)
{
	if (!Body || !Source)
	{
		return;
	}

	UMaterialInstanceDynamic* Paint = Body->CreateDynamicMaterialInstance(SlotIndex, Source);
	if (!Paint)
	{
		Body->SetMaterial(SlotIndex, Source);
		return;
	}

	// The white-mannequin bug is albedo, not the fill light: these bodies wear the engine's
	// showroom mannequin suit, and the -ArtTarget=Battle capture shows it still reading as a
	// white cutout against Erebus's fog. Setting "Paint Tint" alone was a no-op, because
	// M_Mannequin gates the paint layer behind "Masking Paint" and takes its actual base colour
	// from T_Manny_01_D. So: open the mask, then tint. Soldiers wear issue fabric and painted
	// plate - warm olive-brown for human, cold slate for Veil, both in the 24-42% range where
	// the fill reads as a lit body instead of an emissive silhouette.
	// Measured on the -ArtTarget=Combatant bench: at 0.42 the body renders near-white against this
	// street. Erebus road albedo is 0.016-0.029, so a 42% body is four stops brighter before a
	// single lumen is added, and the fill light sits ~50uu off the chest on top of that. The
	// "24-42% is field kit" reasoning was right about real fabric and wrong about this scene.
	const FLinearColor BodyTint = Faction == EAHFaction::Veil
		? FLinearColor(0.075f, 0.095f, 0.130f)
		: FLinearColor(0.120f, 0.100f, 0.075f);
	// M_HumanMetal / M_VeilObsidian expose BaseTint, which is the actual albedo. The mannequin
	// master does not expose one at all: its parameters are exactly
	//   scalar  MetalPaintMetallic, MetalPaintRoughness, LogoPosX/Y/Scale, EmissivePower, Scale, Head Cutout
	//   vector  Paint Tint, LogoTint, Blend Offset
	//   texture BNormal, Base Texture, MRA
	// so the body's white comes from the Base Texture and no parameter darkens it. "Masking Paint"
	// and "Base Color" do not exist on it - both of those earlier writes were silent no-ops, which
	// is why the bench still rendered a white mannequin after they were added.
	Paint->SetVectorParameterValue(TEXT("BaseTint"), BodyTint);
	Paint->SetScalarParameterValue(TEXT("Roughness"), 0.55f);
	Paint->SetScalarParameterValue(TEXT("Metallic"), 0.10f);
	// Procedural surface break-up, so a single flat tint does not read as a paper cut-out.
	Paint->SetScalarParameterValue(TEXT("GrimeAmount"), 0.45f);
	Paint->SetScalarParameterValue(TEXT("WearAmount"), 0.40f);
	Paint->SetScalarParameterValue(TEXT("EdgeVariation"), 0.35f);
	// Kept for any body still wearing the mannequin master: a no-op on the faction materials.
	Paint->SetVectorParameterValue(TEXT("Paint Tint"), BodyTint);
	Paint->SetScalarParameterValue(TEXT("MetalPaintRoughness"), 0.55f);
}

#if !UE_BUILD_SHIPPING
void AAHCombatantCharacter::DebugForcePresentationSync()
{
	// Preferred: the same enemy definition the streaming subsystem would have applied, resolved
	// synchronously. This is the real final presentation path, which is the whole point of the
	// bench - a body dressed by some other route would not be evidence about the shipping look.
	if (!EnemyDefinition)
	{
		if (const FPrimaryAssetId DefaultEnemyId = GetDefaultEnemyDefinitionId(); DefaultEnemyId.IsValid())
		{
			if (UAssetManager* Assets = UAssetManager::GetIfInitialized())
			{
				const FSoftObjectPath DefinitionPath = Assets->GetPrimaryAssetPath(DefaultEnemyId);
				if (UAHEnemyDefinition* Definition = Cast<UAHEnemyDefinition>(DefinitionPath.TryLoad()))
				{
					ApplyEnemyDefinition(Definition);
				}
				UE_LOG(LogAshesOfHeaven, Display, TEXT("[Bench] definition id=%s path=%s resolved=%s"),
					*DefaultEnemyId.ToString(), *DefinitionPath.ToString(),
					EnemyDefinition ? TEXT("yes") : TEXT("NO"));
			}
		}
	}

	if (USkeletalMeshComponent* Body = GetMesh())
	{
		if (!Body->GetSkeletalMeshAsset())
		{
			USkeletalMesh* Legacy = LoadObject<USkeletalMesh>(nullptr,
				TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
			UClass* LegacyAnim = LoadObject<UClass>(nullptr,
				TEXT("/Game/Variant_Shooter/Anims/ABP_TP_Rifle.ABP_TP_Rifle_C"));
			UE_LOG(LogAshesOfHeaven, Display, TEXT("[Bench] legacy mesh=%s anim=%s"),
				Legacy ? TEXT("loaded") : TEXT("NULL"), LegacyAnim ? TEXT("loaded") : TEXT("NULL"));
			Body->SetSkeletalMesh(Legacy);
			Body->SetAnimInstanceClass(LegacyAnim);
		}
	}
	// The skin arrays are soft pointers and ApplyFactionAppearance uses .Get(), which is null
	// unless something already loaded them - so resolve them first and let the real path run.
	for (TSoftObjectPtr<UMaterialInterface>& Material : HumanBodyMaterials)
	{
		Material.LoadSynchronous();
	}
	for (TSoftObjectPtr<UMaterialInterface>& Material : VeilBodyMaterials)
	{
		Material.LoadSynchronous();
	}
	ApplyFactionAppearance();
	if (USkeletalMeshComponent* Result = GetMesh())
	{
		// The base class hides the body from its owner, because the player looks out through their
		// own head. A bench subject is not the player, so that flag has to come off or the capture
		// measures the corridor behind an invisible soldier - which is exactly what it did.
		Result->SetOwnerNoSee(false);
		Result->SetVisibility(true, true);
		Result->SetHiddenInGame(false, true);
		UE_LOG(LogAshesOfHeaven, Display,
			TEXT("[Bench] final mesh=%s visible=%d ownerNoSee=%d owner=%s materials=%d"),
			Result->GetSkeletalMeshAsset() ? *Result->GetSkeletalMeshAsset()->GetName() : TEXT("MISSING"),
			Result->IsVisible() ? 1 : 0, Result->bOwnerNoSee ? 1 : 0,
			GetOwner() ? *GetOwner()->GetName() : TEXT("none"), Result->GetNumMaterials());
	}
}
#endif

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
	if (EnemyDefinition)
	{
		return;
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
		if (UMaterialInterface* SkinMaterial = Skin[SlotIndex].Get())
		{
			ApplyBodyPaint(Body, SlotIndex, SkinMaterial);
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

bool AAHCombatantCharacter::AllowsCorpseCleanup() const
{
	return (bDestroyOnDeath && bAllowCorpseCleanup) || ActorHasTag(FAHCorpseTags::AllowCleanup);
}

bool AAHCombatantCharacter::IsPersistentCorpse() const
{
	return bPersistentCorpse || ActorHasTag(FAHCorpseTags::Persistent);
}

bool AAHCombatantCharacter::IsNarrativeCorpse() const
{
	return bNarrativeCorpse || ActorHasTag(FAHCorpseTags::Narrative);
}

bool AAHCombatantCharacter::IsObjectiveCriticalCorpse() const
{
	return bObjectiveCriticalCorpse || ActorHasTag(FAHCorpseTags::ObjectiveCritical);
}

bool AAHCombatantCharacter::IsScriptedCivilianCorpse() const
{
	return bScriptedCivilianCorpse || ActorHasTag(FAHCorpseTags::ScriptedCivilian);
}

bool AAHCombatantCharacter::HasUnlootedImportantWeapon() const
{
	const AAHWeaponBase* Weapon = GetLootableWeapon();
	return Weapon && (Weapon->bImportantCorpseLoot || ActorHasTag(FAHCorpseTags::Lootable));
}

bool AAHCombatantCharacter::ShouldManageCorpseLifecycle() const
{
	return bDestroyOnDeath
		|| bPersistentCorpse
		|| bNarrativeCorpse
		|| bObjectiveCriticalCorpse
		|| bScriptedCivilianCorpse
		|| ActorHasTag(FAHCorpseTags::Persistent)
		|| ActorHasTag(FAHCorpseTags::Narrative)
		|| ActorHasTag(FAHCorpseTags::ObjectiveCritical)
		|| ActorHasTag(FAHCorpseTags::ScriptedCivilian)
		|| ActorHasTag(FAHCorpseTags::AllowCleanup);
}

void AAHCombatantCharacter::PrepareForCorpseManagement()
{
	SetActorTickEnabled(false);
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
		Movement->SetComponentTickEnabled(false);
	}
	auto DisableComponentTick = [](UActorComponent* Component)
	{
		if (Component)
		{
			Component->SetComponentTickEnabled(false);
		}
	};
	DisableComponentTick(HealthComponent.Get());
	DisableComponentTick(ArmorComponent.Get());
	DisableComponentTick(CombatComponent.Get());
	DisableComponentTick(InteractionComponent.Get());
	DisableComponentTick(InventoryComponent.Get());
	DisableComponentTick(BodyFillLight.Get());
	if (BodyFillLight)
	{
		BodyFillLight->SetVisibility(false);
	}
	if (InventoryComponent)
	{
		for (AAHWeaponBase* Weapon : InventoryComponent->GetWeapons())
		{
			if (IsValid(Weapon))
			{
				Weapon->StopFire();
				Weapon->SetActorTickEnabled(false);
			}
		}
	}
}

void AAHCombatantCharacter::SettleCorpsePhysics()
{
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->PutAllRigidBodiesToSleep();
	}
}

void AAHCombatantCharacter::ApplyReducedCorpseCost()
{
	USkeletalMeshComponent* Body = GetMesh();
	if (!Body)
	{
		return;
	}

	// Pause the last physics-authored component-space pose before removing it from the solver.
	// The mesh remains visibility-queryable for targeting and loot, but no longer animates or
	// maintains a set of simulated rigid bodies.
	Body->PutAllRigidBodiesToSleep();
	Body->bPauseAnims = true;
	Body->SetEnableGravity(false);
	Body->SetAllBodiesSimulatePhysics(false);
	Body->SetSimulatePhysics(false);
	Body->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Body->SetCollisionResponseToAllChannels(ECR_Ignore);
	Body->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Body->SetComponentTickEnabled(false);
}

void AAHCombatantCharacter::PrepareForCorpseRemoval()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	if (USkeletalMeshComponent* Body = GetMesh())
	{
		Body->PutAllRigidBodiesToSleep();
		Body->SetAllBodiesSimulatePhysics(false);
		Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (InventoryComponent)
	{
		InventoryComponent->DestroyWeaponsForCorpseCleanup();
	}
	OnDamageFeedback.Clear();
	OnCombatantDeath.Clear();
	OnWeaponShot.Clear();
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
	if (DeathEffect)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, DeathEffect, GetActorLocation(), GetActorRotation());
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

	if (ShouldManageCorpseLifecycle())
	{
		if (UAHCorpseManagerSubsystem* CorpseManager = GetWorld() ? GetWorld()->GetSubsystem<UAHCorpseManagerSubsystem>() : nullptr)
		{
			CorpseManager->RegisterCorpse(this);
		}
		else if (bDestroyOnDeath)
		{
			// Safe fallback for an unsupported world type; normal gameplay always uses the manager.
			SetLifeSpan(CorpseLifeSpan);
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
