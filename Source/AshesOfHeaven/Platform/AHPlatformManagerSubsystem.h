// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AHPlatformTypes.h"
#include "AHPlatformManagerSubsystem.generated.h"

class UInputAction;
class UInputMappingContext;

DECLARE_MULTICAST_DELEGATE(FPlatformLifecycleEvent);

/**
 * Owns platform detection, capability profiles, runtime quality settings, fallback input,
 * lifecycle notifications, and the shared combat population budget.
 * Gameplay code consumes this service instead of checking operating-system macros.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHPlatformManagerSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	static UAHPlatformManagerSubsystem* Get(const UObject* WorldContextObject);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	EAHPlatformFamily GetPlatformFamily() const { return Capabilities.PlatformFamily; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	const FAHPlatformCapabilities& GetCapabilities() const { return Capabilities; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	const FAHGraphicsProfile& GetGraphicsProfile() const { return GraphicsProfile; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	const FAHPerformanceProfile& GetPerformanceProfile() const { return PerformanceProfile; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	const FAHInputProfile& GetInputProfile() const { return InputProfile; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	const FAHDeviceProfile& GetDeviceProfile() const { return DeviceProfile; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	EAHQualityPreset GetActiveQualityPreset() const { return ActiveQualityPreset; }

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	bool ShouldUseTouchControls() const;

	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Platform")
	void ApplyQualityPreset(EAHQualityPreset Preset);

	UFUNCTION(BlueprintCallable, Category="Ashes of Heaven|Platform")
	void SetMobilePerformanceMode(EAHMobilePerformanceMode Mode);

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Platform")
	bool IsApplicationSuspended() const { return bApplicationSuspended; }

	/** The generated input context is a fallback for maps whose Blueprint IMC is not configured yet. */
	UInputMappingContext* GetRuntimeInputMappingContext() const { return RuntimeInputMappingContext; }

	UInputAction* GetMoveAction() const { return MoveAction; }
	UInputAction* GetLookAction() const { return LookAction; }
	UInputAction* GetMouseLookAction() const { return MouseLookAction; }
	UInputAction* GetFireAction() const { return FireAction; }
	UInputAction* GetADSAction() const { return ADSAction; }
	UInputAction* GetReloadAction() const { return ReloadAction; }
	UInputAction* GetJumpAction() const { return JumpAction; }
	UInputAction* GetCrouchAction() const { return CrouchAction; }
	UInputAction* GetSprintAction() const { return SprintAction; }
	UInputAction* GetMeleeAction() const { return MeleeAction; }
	UInputAction* GetGrenadeAction() const { return GrenadeAction; }
	UInputAction* GetInteractAction() const { return InteractAction; }
	UInputAction* GetWeaponNextAction() const { return WeaponNextAction; }
	UInputAction* GetWeaponPreviousAction() const { return WeaponPreviousAction; }
	UInputAction* GetPauseAction() const { return PauseAction; }

	/** Tier-1 AI actors call these methods so all spawners share one budget. */
	bool TryRegisterActiveCombatant();
	void UnregisterActiveCombatant();
	int32 GetActiveCombatantCount() const { return ActiveCombatants; }

	FPlatformLifecycleEvent OnApplicationSuspended;
	FPlatformLifecycleEvent OnApplicationResumed;

private:
	void DetectPlatform();
	void BuildProfiles();
	void BuildRuntimeInputActions();
	void AddInputMapping(UInputAction* Action, const FKey& Key, bool bNegate = false, bool bSwizzleToY = false);
	void ApplyCurrentQualitySettings();
	void HandleApplicationWillEnterBackground();
	void HandleApplicationHasEnteredForeground();
	void SaveForApplicationSuspend();

	EAHQualityPreset ResolveQualityPreset(EAHQualityPreset Requested) const;

	FAHPlatformCapabilities Capabilities;
	FAHGraphicsProfile GraphicsProfile;
	FAHPerformanceProfile PerformanceProfile;
	FAHInputProfile InputProfile;
	FAHDeviceProfile DeviceProfile;
	EAHQualityPreset ActiveQualityPreset = EAHQualityPreset::Auto;
	EAHMobilePerformanceMode ActiveMobilePerformanceMode = EAHMobilePerformanceMode::Auto;
	bool bRuntimeQualityOverride = false;
	bool bRuntimeMobilePerformanceOverride = false;
	int32 ActiveCombatants = 0;
	bool bApplicationSuspended = false;

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> RuntimeInputMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MouseLookAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> FireAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ADSAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ReloadAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> CrouchAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> MeleeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> GrenadeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> InteractAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> WeaponNextAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> WeaponPreviousAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PauseAction;
};
