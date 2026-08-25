#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManagerTypes.h"
#include "AshesOfHeavenCharacter.h"
#include "Gameplay/Combat/AHGameplayTypes.h"
#include "Gameplay/Combat/AHInteractionComponent.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "AHCombatantCharacter.generated.h"

class UAHHealthComponent;
class UAHArmorComponent;
class UAHCombatComponent;
class UAHInteractionComponent;
class UAHInventoryComponent;
class AAHWeaponBase;
class USoundBase;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UPointLightComponent;
class UNiagaraSystem;
class UAHAudioPaletteData;
struct FStreamableHandle;
class UAHCorpseManagerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(FAHCombatDamageFeedbackDelegate, float, Damage, bool, bHeadshot, bool, bArmorHit, bool, bArmorBroken, float, DirectionAngle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAHCombatantDeathDelegate);
DECLARE_MULTICAST_DELEGATE_TwoParams(FAHWeaponShotNativeDelegate, const FHitResult&, bool);

UCLASS(Abstract)
class ASHESOFHEAVEN_API AAHCombatantCharacter : public AAshesOfHeavenCharacter, public IAHInteractable
{
	GENERATED_BODY()

public:
	AAHCombatantCharacter();

	/** A body can be stripped for the weapon it was still holding. Live combatants offer nothing. */
	virtual void Interact_Implementation(AActor* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual float GetInteractionPriority_Implementation() const override;

	UFUNCTION(BlueprintPure, Category="Combat")
	AAHWeaponBase* GetLootableWeapon() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHHealthComponent> HealthComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHArmorComponent> ArmorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHCombatComponent> CombatComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHInteractionComponent> InteractionComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UAHInventoryComponent> InventoryComponent;

	/** Reads the combatant out of the dark without lighting the street.
	 * Erebus has no ground-level fill and auto-exposure keys off the bright sky, so a soldier
	 * standing in it is a flat black cut-out against the fog - no face, no armour, no read on
	 * which way they are facing. This light is restricted to lighting channel 1, which only
	 * character bodies are on, so it shapes them without touching the environment. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UPointLightComponent> BodyFillLight;

	/** Candelas. Derived from the scene key rather than picked: the fill sits ~0.5m off the body,
	 *  so E = I/d^2 means 110cd delivered ~440 lux to a body standing in a street lit by a 32 lux
	 *  sun - ten to thirty times its surroundings, which is why bodies read as white cut-outs at
	 *  any albedo. Dropping the body's albedo from 0.42 to 0.12 barely moved it (bench p90 0.76 ->
	 *  0.62), which is what proved the light and not the material was the dominant term.
	 *  15cd puts the fill in the same order as the scene key. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance", meta=(ClampMin=0.0))
	float BodyFillIntensity = 15.0f;

	/** Colour of that fill. White is what made every body look pasted into the shot rather than
	 *  standing in it: Erebus is lit by an orange sun through orange fog and every surface in the
	 *  scene is warm, so a neutral key on the one object that carries its own light reads as a
	 *  cool cut-out. Measured against the frame - road and wall sit around R>G>B, and the bodies
	 *  came back B>G>R until this matched them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appearance")
	FLinearColor BodyFillColor = FLinearColor(1.0f, 0.82f, 0.62f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	EAHFaction Faction = EAHFaction::Neutral;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin=1.0))
	float HeadshotMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat")
	bool bDestroyOnDeath = true;

	/** Ordinary combat bodies can be removed under platform-budget pressure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corpse")
	bool bAllowCorpseCleanup = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corpse")
	bool bPersistentCorpse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corpse")
	bool bNarrativeCorpse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corpse")
	bool bObjectiveCriticalCorpse = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corpse")
	bool bScriptedCivilianCorpse = false;

	/** Higher values make an otherwise ordinary body a later cleanup choice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Corpse", meta=(ClampMin=0.0, ClampMax=1.0))
	float CorpseImportance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> HurtSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> ArmorDamageSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio")
	TObjectPtr<USoundBase> DeathSound;

	/** Centimetres of ground travel between one footstep and the next.
	 * Footsteps used to run off a per-stance timer, which fixes the cadence and lets the stride
	 * be whatever the current speed happens to make it: 0.43s at 420cm/s is a 181cm stride, and
	 * the same 0.43s at half-stick is 90cm. A stride is a distance, so this is a distance, and
	 * cadence falls out of however fast the body is actually moving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=20.0))
	float FootstepStride = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=0.0))
	float FootstepVolume = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=0.1))
	float FootstepPitch = 1.0f;

	/** Ground speed under which the body counts as standing still rather than walking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio|Footsteps", meta=(ClampMin=0.0))
	float FootstepMinimumSpeed = 35.0f;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FAHCombatDamageFeedbackDelegate OnDamageFeedback;

	UPROPERTY(BlueprintAssignable, Category="Combat")
	FAHCombatantDeathDelegate OnCombatantDeath;

	FAHWeaponShotNativeDelegate OnWeaponShot;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual float TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent, AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category="Combat")
	void SetFaction(EAHFaction NewFaction) { Faction = NewFaction; }

	UFUNCTION(BlueprintPure, Category="Combat")
	EAHFaction GetFaction() const { return Faction; }

	FName GetFactionName() const;
	bool IsHostileTo(const AAHCombatantCharacter* Other) const;

	UFUNCTION(BlueprintPure, Category="Combat")
	bool IsCombatantDead() const;

	UFUNCTION(BlueprintPure, Category="Corpse")
	bool AllowsCorpseCleanup() const;

	UFUNCTION(BlueprintPure, Category="Corpse")
	bool IsPersistentCorpse() const;

	UFUNCTION(BlueprintPure, Category="Corpse")
	bool IsNarrativeCorpse() const;

	UFUNCTION(BlueprintPure, Category="Corpse")
	bool IsObjectiveCriticalCorpse() const;

	UFUNCTION(BlueprintPure, Category="Corpse")
	bool IsScriptedCivilianCorpse() const;

	UFUNCTION(BlueprintPure, Category="Corpse")
	bool HasUnlootedImportantWeapon() const;

	float GetCorpseImportance() const { return FMath::Clamp(CorpseImportance, 0.0f, 1.0f); }

	FVector GetWeaponTraceOrigin() const;
	virtual FVector GetWeaponTargetLocation() const;
	void SetCombatTarget(AActor* NewTarget);
	AActor* GetCombatTarget() const { return CombatTarget.Get(); }

	bool IsAimingDownSights() const { return bAimingDownSights; }

	/** Extra cone this combatant's shots scatter into, on top of the weapon's own spread.
	 * Zero for the player; AI controllers set it from their Accuracy so a squad is dangerous
	 * without being a wall of instant hitscan. */
	float GetAimSpreadPenaltyDegrees() const { return AimSpreadPenaltyDegrees; }
	void SetAimSpreadPenaltyDegrees(float Degrees) { AimSpreadPenaltyDegrees = FMath::Max(0.0f, Degrees); }
	void SetAimingDownSights(bool bNewAiming);
	void ApplyCameraRecoil(float Vertical, float Horizontal);
	void NotifyWeaponShot(const FHitResult& Hit, bool bHit);

	/** Applies an already-streamed immutable definition before deferred spawning finishes. */
	virtual void ApplyEnemyDefinition(UAHEnemyDefinition* Definition);
	UAHEnemyDefinition* GetEnemyDefinition() const { return EnemyDefinition; }

	virtual void OnDeathStarted();

	UAHHealthComponent* GetHealthComponent() const { return HealthComponent; }
	UAHArmorComponent* GetArmorComponent() const { return ArmorComponent; }
	UAHCombatComponent* GetCombatComponent() const { return CombatComponent; }
	UAHInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }
	UAHInventoryComponent* GetInventoryComponent() const { return InventoryComponent; }

#if !UE_BUILD_SHIPPING
	/** Development only. Resolves the body presentation synchronously and runs the normal faction
	 *  material path, for the -ArtTarget=Combatant bench. Combatants normally receive their mesh
	 *  from the enemy-asset streaming subsystem when their encounter activates, so a body spawned
	 *  outside an encounter never gets one and cannot be scored. Not used by gameplay. */
	void DebugForcePresentationSync();
#endif

protected:
	/** Per-slot faction skin, applied in BeginPlay once the subclass has set Faction.
	 * These are lit character materials on purpose: the environment materials that were here
	 * before have a near-black base colour, and against Erebus's bright fog a combatant wearing
	 * one is a flat silhouette with no face, no armour and no readable pose. */
	UPROPERTY(EditDefaultsOnly, Category="Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> HumanBodyMaterials {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal")))
	};

	UPROPERTY(EditDefaultsOnly, Category="Appearance")
	TArray<TSoftObjectPtr<UMaterialInterface>> VeilBodyMaterials {
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"))),
		TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian")))
	};

	/** Seconds a ragdolled corpse stays in the world before cleanup. Long enough to read as a kill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Combat", meta=(ClampMin=0.0))
	float CorpseLifeSpan = 30.0f;

	void ApplyFactionAppearance();
	/** Every body material goes through here: both the faction skins and the definition-driven
	 * visuals, so the paint cannot be right on one path and stock grey on the other. */
	void ApplyBodyPaint(USkeletalMeshComponent* Body, int32 SlotIndex, UMaterialInterface* Source);
	void ApplyDefinitionLoadout();
	void ApplyDefinitionToController();
	void RequestLegacyPresentationAsync();
	void HandleLegacyPresentationLoaded();
	void HandleSelfEnemyAssetsReady(FGuid RequestId, bool bSuccess, const TArray<UAHEnemyDefinition*>& Definitions, const FString& Error);
	virtual FPrimaryAssetId GetDefaultEnemyDefinitionId() const;
	void StartRagdoll();
	bool ShouldManageCorpseLifecycle() const;
	void PrepareForCorpseManagement();
	void SettleCorpsePhysics();
	void ApplyReducedCorpseCost();
	void PrepareForCorpseRemoval();

	/** One step, at the feet, with the jitter that stops a single sample sounding like a machine. */
	void PlayFootstep(float VolumeScale, float PitchScale);

	UFUNCTION()
	void HandleHealthDeath();

	UFUNCTION()
	void HandleArmorBroken();

	TWeakObjectPtr<AActor> CombatTarget;

	/** The immutable archetype reference; mutable health, inventory, timers, and AI remain on the actor. */
	UPROPERTY(Transient)
	TObjectPtr<UAHEnemyDefinition> EnemyDefinition;

	UPROPERTY(Transient)
	TObjectPtr<UAHAudioPaletteData> VoicePalette;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> SpawnEffect;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraSystem> DeathEffect;

	FGuid SelfAssetLease;
	TSharedPtr<FStreamableHandle> LegacyPresentationHandle;
	bool bAimingDownSights = false;
	float AimSpreadPenaltyDegrees = 0.0f;
	/** Ground distance still owed before the next step, in centimetres. */
	float FootstepDistanceRemaining = 0.0f;
	/** Alternated per step so consecutive steps are not the same pitch twice running. */
	bool bNextFootIsLeft = true;

	friend class UAHCorpseManagerSubsystem;
};
