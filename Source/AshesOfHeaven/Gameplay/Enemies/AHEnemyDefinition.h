#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "Engine/DataAsset.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "AHEnemyDefinition.generated.h"

class AAHCombatantCharacter;
class AAHWeaponBase;
class AAIController;
class UAHEnemyDefinition;
class UAHAudioPaletteData;
class UAnimInstance;
class UAnimationAsset;
class UAnimSequenceBase;
class UBehaviorTree;
class UCurveFloat;
class UMaterialInterface;
class UNiagaraSystem;
class UPhysicsAsset;
class USkeletalMesh;
class UStaticMesh;
class USoundBase;
class UStateTree;
class UTexture2D;

namespace AHEnemyAssets
{
	ASHESOFHEAVEN_API extern const FPrimaryAssetType EnemyType;
	ASHESOFHEAVEN_API extern const FPrimaryAssetType EncounterType;
	ASHESOFHEAVEN_API extern const FName CoreBundle;
	ASHESOFHEAVEN_API extern const FName VisualBundle;
	ASHESOFHEAVEN_API extern const FName AudioBundle;
	ASHESOFHEAVEN_API extern const FName DesktopBundle;
	ASHESOFHEAVEN_API extern const FName MobileBundle;
	ASHESOFHEAVEN_API extern const FName DesktopAudioBundle;
	ASHESOFHEAVEN_API extern const FName MobileAudioBundle;

	ASHESOFHEAVEN_API FPrimaryAssetId EnemyId(FName Name);
	ASHESOFHEAVEN_API FPrimaryAssetId EncounterId(FName Name);
}

/** Immutable combat defaults copied into a newly spawned enemy's runtime components. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyCombatDefaults
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	EAHFaction Faction = EAHFaction::Veil;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1.0"))
	float MaxHealth = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float MaxArmor = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float WalkSpeed = 300.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="1.0"))
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat")
	bool bDestroyOnDeath = true;

	/** Seconds a corpse may remain after death. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float CorpseLifeSpan = 30.0f;

	/** Zero keeps the combat class default. A knee-high biter and a two-and-a-half metre
	 *  revenant cannot share the mannequin capsule: too tall and the body floats, too short
	 *  and the head pokes through geometry. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float CapsuleHalfHeight = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat", meta=(ClampMin="0.0"))
	float CapsuleRadius = 0.0f;
};

/** AI authoring values and optional behavior assets needed before possession. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyAISettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	TSoftClassPtr<AAIController> ControllerClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	TSoftObjectPtr<UBehaviorTree> BehaviorTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	TSoftObjectPtr<UStateTree> StateTree;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="0.0"))
	float SightRange = 2600.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="0.0", ClampMax="1.0"))
	float Accuracy = 0.72f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="0.0"))
	float MaxAimErrorDegrees = 9.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI")
	bool bPreferCover = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="100.0"))
	float PreferredEngagementRange = 1100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="100.0"))
	float MinimumEngagementRange = 650.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="1"))
	int32 BurstRounds = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="0.0"))
	float MinBurstPause = 0.75f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI", meta=(ClampMin="0.0"))
	float MaxBurstPause = 1.6f;

	/** Closes to contact and bites instead of holding a firing line. Set on archetypes whose
	 *  Loadout grants no weapon - a beast has nothing to stand off with, so the cover, standoff
	 *  and burst-fire logic that governs a rifleman would leave it circling out of reach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Melee")
	bool bMeleeOnly = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Melee", meta=(ClampMin="0.0"))
	float MeleeDamage = 45.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Melee", meta=(ClampMin="0.0"))
	float MeleeRange = 165.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Melee", meta=(ClampMin="0.0"))
	float MeleeRadius = 28.0f;

	/** Seconds between bites. This is the beast's whole damage cadence, so it is the knob that
	 *  decides whether a pack is pressure or an instant kill. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="AI|Melee", meta=(ClampMin="0.1"))
	float MeleeCooldown = 1.1f;
};

/** Which authored take a creature body is playing. */
UENUM(BlueprintType)
enum class EAHCreatureAnimState : uint8
{
	Idle,
	Walk,
	Run,
	Attack,
	Death
};

/** Named locomotion and reaction takes for a body with no AnimBlueprint.

 *  The imported creatures each carry their own skeleton, so the mannequin AnimBlueprint cannot
 *  drive any of them and authoring four AnimBlueprints by hand is four graphs to maintain for
 *  what is a five-clip state machine. The character plays these through the single-node
 *  instance instead and picks between them on speed, which is the whole of what these bodies
 *  need: they walk, they run, they bite, they fall over. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHCreatureAnimationSet
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation")
	TSoftObjectPtr<UAnimSequenceBase> Idle;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation")
	TSoftObjectPtr<UAnimSequenceBase> Walk;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation")
	TSoftObjectPtr<UAnimSequenceBase> Run;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation")
	TSoftObjectPtr<UAnimSequenceBase> Attack;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation")
	TSoftObjectPtr<UAnimSequenceBase> Death;

	/** Ground speed at which the body stops idling and starts walking, cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation", meta=(ClampMin="1.0"))
	float WalkSpeed = 40.0f;

	/** Ground speed at which the walk gives way to the run, cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual|Animation", meta=(ClampMin="1.0"))
	float RunSpeed = 340.0f;

	bool IsEmpty() const;
};

namespace AHCreatureLocomotion
{
	/** Which take a body moving at this ground speed should be playing.
	 *
	 *  Free function, and deliberately knows nothing about actors or worlds, because the thing
	 *  worth testing here is the hysteresis: without it a body decelerating through a threshold
	 *  swaps clip on alternate frames and reads as a stutter instead of a gait change. */
	ASHESOFHEAVEN_API EAHCreatureAnimState SelectLocomotionState(
		const FAHCreatureAnimationSet& Set, float GroundSpeed, EAHCreatureAnimState Current);
}

/** Mesh, animation, physics, and material payload for one presentation tier. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyVisualPayload
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<USkeletalMesh> SkeletalMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TSoftClassPtr<UAnimInstance> AnimClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TArray<TSoftObjectPtr<UAnimationAsset>> AnimationSet;

	/** Preferred over AnimationSet when it names anything: AnimationSet only ever loops its
	 *  first entry, which is a body that walks on the spot while it charges you. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	FAHCreatureAnimationSet Locomotion;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TSoftObjectPtr<UPhysicsAsset> PhysicsAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	TArray<TSoftObjectPtr<UMaterialInterface>> Materials;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	FVector MeshScale = FVector::OneVector;

	/** Body transform relative to the capsule centre, used when bOverrideMeshTransform is set.
	 *  The mannequin convention (origin at the feet, facing +Y) is baked into the combat class
	 *  defaults; an imported creature honours neither, so each one carries its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	FVector MeshOffset = FVector::ZeroVector;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	FRotator MeshRotation = FRotator(0.0f, -90.0f, 0.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual")
	bool bOverrideMeshTransform = false;

	bool HasAnyAssetOverride() const;
	void OverlayOnto(FAHEnemyVisualPayload& Target) const;
};

/** Voice and one-shot audio payload for one presentation tier. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyAudioPayload
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TSoftObjectPtr<UAHAudioPaletteData> VoicePalette;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TArray<TSoftObjectPtr<USoundBase>> VoiceBanks;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TSoftObjectPtr<USoundBase> HurtSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TSoftObjectPtr<USoundBase> ArmorDamageSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio")
	TSoftObjectPtr<USoundBase> DeathSound;

	bool HasAnyAssetOverride() const;
	void OverlayOnto(FAHEnemyAudioPayload& Target) const;
};

/** Spawn, combat, ambient, and death effects for one presentation tier. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyVFXPayload
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
	TSoftObjectPtr<UNiagaraSystem> SpawnEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
	TSoftObjectPtr<UNiagaraSystem> AmbientEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
	TSoftObjectPtr<UNiagaraSystem> HitEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX")
	TSoftObjectPtr<UNiagaraSystem> DeathEffect;

	bool HasAnyAssetOverride() const;
	void OverlayOnto(FAHEnemyVFXPayload& Target) const;
};

/** Classes plus the presentation payload used by weapons granted to this enemy. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Core"))
	TArray<TSoftClassPtr<AAHWeaponBase>> WeaponClasses;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Visual"))
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Visual"))
	TSoftObjectPtr<UMaterialInterface> WeaponMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Visual"))
	TSoftObjectPtr<UStaticMesh> CapacitorMesh;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Visual"))
	TSoftObjectPtr<UMaterialInterface> CapacitorMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Audio"))
	TSoftObjectPtr<USoundBase> ShotSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Audio"))
	TSoftObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Audio"))
	TSoftObjectPtr<USoundBase> EmptySound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Audio"))
	TSoftObjectPtr<USoundBase> ImpactSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Visual"))
	TSoftObjectPtr<UNiagaraSystem> MuzzleFlashEffect;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout", meta=(AssetBundles="Visual"))
	TSoftObjectPtr<UNiagaraSystem> ImpactEffect;
};

/** Loot references and authored reward amounts; current inventory remains on the actor. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyLootSettings
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	TArray<FPrimaryAssetId> LootDefinitions;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	int32 CurrencyReward = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
	bool bWeaponCanBeRecovered = true;
};

/** World-marker presentation only; target selection and visibility are runtime concerns. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyUIMarkerData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	FText DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI")
	FLinearColor MarkerColor = FLinearColor::White;
};

/** Difficulty-space multipliers and named archetype knobs, never current combat state. */
USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHEnemyDifficultyTuning
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0"))
	float ThreatCost = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0"))
	float HealthScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0"))
	float ArmorScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0"))
	float DamageScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.0"))
	float AccuracyScale = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty")
	TMap<FName, float> AbilityScalars;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty")
	TSoftObjectPtr<UCurveFloat> DifficultyCurve;
};

/**
 * Stable enemy archetype identity and immutable authoring defaults. Heavy payloads are soft
 * references assigned to Asset Manager bundles and are never used as mutable runtime state.
 */
UCLASS(BlueprintType, Const)
class ASHESOFHEAVEN_API UAHEnemyDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Identity")
	FName EnemyId = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Core", meta=(AssetBundles="Core"))
	TSoftClassPtr<AAHCombatantCharacter> CombatClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Core")
	FAHEnemyCombatDefaults CombatDefaults;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Core", meta=(AssetBundles="Core"))
	FAHEnemyAISettings AISettings;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loadout")
	FAHEnemyLoadout Loadout;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Visual", meta=(AssetBundles="Visual"))
	FAHEnemyVisualPayload Visuals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Audio", meta=(AssetBundles="Audio"))
	FAHEnemyAudioPayload Audio;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="VFX", meta=(AssetBundles="Visual"))
	FAHEnemyVFXPayload VFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(AssetBundles="Core"))
	FAHEnemyLootSettings Loot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI", meta=(AssetBundles="Visual"))
	FAHEnemyUIMarkerData UIMarker;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(AssetBundles="Core"))
	FAHEnemyDifficultyTuning Difficulty;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Platform|Desktop", meta=(AssetBundles="Desktop"))
	FAHEnemyVisualPayload DesktopVisuals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Platform|Desktop", meta=(AssetBundles="Desktop"))
	FAHEnemyVFXPayload DesktopVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Platform|Desktop", meta=(AssetBundles="DesktopAudio"))
	FAHEnemyAudioPayload DesktopAudio;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Platform|Mobile", meta=(AssetBundles="Mobile"))
	FAHEnemyVisualPayload MobileVisuals;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Platform|Mobile", meta=(AssetBundles="Mobile"))
	FAHEnemyVFXPayload MobileVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Platform|Mobile", meta=(AssetBundles="MobileAudio"))
	FAHEnemyAudioPayload MobileAudio;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	FAHEnemyVisualPayload ResolveVisuals(bool bMobile) const;
	FAHEnemyAudioPayload ResolveAudio(bool bMobile) const;
	FAHEnemyVFXPayload ResolveVFX(bool bMobile) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif
};
