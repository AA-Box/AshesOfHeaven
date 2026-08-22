// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHPlatformManagerSubsystem.h"
#include "Platform/AHPlatformSaveSubsystem.h"
#include "Platform/AHPlatformSettings.h"
#include "AshesOfHeaven.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "Scalability.h"

UAHPlatformManagerSubsystem* UAHPlatformManagerSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}

	if (const UWorld* World = WorldContextObject->GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UAHPlatformManagerSubsystem>();
		}
	}

	return nullptr;
}

void UAHPlatformManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	DetectPlatform();
	BuildProfiles();
	BuildRuntimeInputActions();
	ApplyCurrentQualitySettings();

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.AddUObject(this, &UAHPlatformManagerSubsystem::HandleApplicationWillEnterBackground);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.AddUObject(this, &UAHPlatformManagerSubsystem::HandleApplicationHasEnteredForeground);

	const UAHPlatformSettings* Settings = UAHPlatformSettings::Get();
	if (Settings->bLogPlatformProfileAtStartup)
	{
		UE_LOG(LogAshesOfHeaven, Log, TEXT("Platform profile: %s | device=%s %s | quality=%s | target=%0.f FPS | active combatants=%d"),
			*Capabilities.PlatformName,
			*DeviceProfile.DeviceMake,
			*DeviceProfile.DeviceModel,
			*UEnum::GetValueAsString(ActiveQualityPreset),
			PerformanceProfile.TargetFrameRate,
			PerformanceProfile.MaxActiveCombatants);
	}
}

void UAHPlatformManagerSubsystem::Deinitialize()
{
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.RemoveAll(this);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.RemoveAll(this);
	RuntimeInputMappingContext = nullptr;
	ActiveCombatants = 0;
	Super::Deinitialize();
}

void UAHPlatformManagerSubsystem::DetectPlatform()
{
	const UAHPlatformSettings* Settings = UAHPlatformSettings::Get();
	const FString PlatformName = FString(FPlatformProperties::PlatformName());

	Capabilities.PlatformName = PlatformName;
	Capabilities.bIsMobile = PlatformName.Contains(TEXT("Android"), ESearchCase::IgnoreCase)
		|| PlatformName.Contains(TEXT("IOS"), ESearchCase::IgnoreCase)
		|| PlatformName.Contains(TEXT("iOS"), ESearchCase::IgnoreCase);
	Capabilities.bIsDesktop = !Capabilities.bIsMobile;

	if (Settings->bForceMobileProfileInEditor)
	{
		Capabilities.PlatformFamily = EAHPlatformFamily::Android;
		Capabilities.PlatformName = TEXT("EditorMobilePreview");
		Capabilities.bIsMobile = true;
		Capabilities.bIsDesktop = false;
	}
	else if (PlatformName.Contains(TEXT("Android"), ESearchCase::IgnoreCase))
	{
		Capabilities.PlatformFamily = EAHPlatformFamily::Android;
	}
	else if (PlatformName.Contains(TEXT("IOS"), ESearchCase::IgnoreCase) || PlatformName.Contains(TEXT("iOS"), ESearchCase::IgnoreCase))
	{
		Capabilities.PlatformFamily = EAHPlatformFamily::IOS;
	}
	else if (PlatformName.Contains(TEXT("Mac"), ESearchCase::IgnoreCase))
	{
		Capabilities.PlatformFamily = EAHPlatformFamily::Mac;
	}
	else if (PlatformName.Contains(TEXT("Win"), ESearchCase::IgnoreCase))
	{
		Capabilities.PlatformFamily = EAHPlatformFamily::Windows;
	}

	Capabilities.bSupportsTouch = Capabilities.bIsMobile;
	Capabilities.bSupportsGyro = Capabilities.PlatformFamily == EAHPlatformFamily::Android || Capabilities.PlatformFamily == EAHPlatformFamily::IOS;
	Capabilities.bSupportsExternalController = true;
	Capabilities.bSupportsDynamicResolution = true;
	Capabilities.bSupportsNanite = !Capabilities.bIsMobile;
	Capabilities.bSupportsLumen = !Capabilities.bIsMobile;
	Capabilities.bSupportsVirtualShadowMaps = !Capabilities.bIsMobile;
	Capabilities.bSupportsPlatformSaveLocation = true;

	DeviceProfile.DeviceProfileName = Capabilities.bIsMobile ? TEXT("Mobile") : TEXT("Desktop");
	DeviceProfile.DeviceMake = FPlatformMisc::GetDeviceMakeAndModel();
	DeviceProfile.DeviceModel = TEXT("See device make/model");
	DeviceProfile.bHighEnd = !Capabilities.bIsMobile;
}

EAHQualityPreset UAHPlatformManagerSubsystem::ResolveQualityPreset(EAHQualityPreset Requested) const
{
	if (Requested != EAHQualityPreset::Auto)
	{
		return Requested;
	}

	if (Capabilities.bIsMobile)
	{
		return EAHQualityPreset::Mobile;
	}

	return DeviceProfile.bHighEnd ? EAHQualityPreset::High : EAHQualityPreset::Medium;
}

void UAHPlatformManagerSubsystem::BuildProfiles()
{
	const UAHPlatformSettings* Settings = UAHPlatformSettings::Get();
	if (!bRuntimeQualityOverride)
	{
		ActiveQualityPreset = ResolveQualityPreset(Settings->DefaultQualityPreset);
	}
	if (!bRuntimeMobilePerformanceOverride)
	{
		ActiveMobilePerformanceMode = Settings->DefaultMobilePerformanceMode;
	}

	GraphicsProfile = FAHGraphicsProfile();
	GraphicsProfile.Preset = ActiveQualityPreset;
	GraphicsProfile.bUseDynamicResolution = Settings->bEnableDynamicResolution && Capabilities.bSupportsDynamicResolution;

	switch (ActiveQualityPreset)
	{
	case EAHQualityPreset::Low:
		GraphicsProfile.RenderScale = 70.0f;
		GraphicsProfile.TextureQuality = 1;
		GraphicsProfile.ShadowQuality = 1;
		GraphicsProfile.EffectsQuality = 1;
		GraphicsProfile.PostProcessQuality = 1;
		GraphicsProfile.FoliageQuality = 1;
		GraphicsProfile.ShadowDistance = 5000.0f;
		GraphicsProfile.bMotionBlur = false;
		GraphicsProfile.bFilmGrain = false;
		GraphicsProfile.bCameraShake = false;
		break;
	case EAHQualityPreset::Medium:
		GraphicsProfile.RenderScale = 85.0f;
		GraphicsProfile.TextureQuality = 2;
		GraphicsProfile.ShadowQuality = 2;
		GraphicsProfile.EffectsQuality = 2;
		GraphicsProfile.PostProcessQuality = 2;
		GraphicsProfile.FoliageQuality = 2;
		GraphicsProfile.ShadowDistance = 8000.0f;
		GraphicsProfile.bMotionBlur = false;
		break;
	case EAHQualityPreset::High:
		GraphicsProfile.RenderScale = 100.0f;
		GraphicsProfile.TextureQuality = 3;
		GraphicsProfile.ShadowQuality = 3;
		GraphicsProfile.EffectsQuality = 3;
		GraphicsProfile.PostProcessQuality = 3;
		GraphicsProfile.FoliageQuality = 3;
		GraphicsProfile.ShadowDistance = 12000.0f;
		GraphicsProfile.bUseLumen = Capabilities.bSupportsLumen;
		GraphicsProfile.bUseNanite = Capabilities.bSupportsNanite;
		GraphicsProfile.bUseVirtualShadowMaps = Capabilities.bSupportsVirtualShadowMaps;
		break;
	case EAHQualityPreset::Ultra:
		GraphicsProfile.RenderScale = 100.0f;
		GraphicsProfile.TextureQuality = 4;
		GraphicsProfile.ShadowQuality = 4;
		GraphicsProfile.EffectsQuality = 4;
		GraphicsProfile.PostProcessQuality = 4;
		GraphicsProfile.FoliageQuality = 4;
		GraphicsProfile.ShadowDistance = 20000.0f;
		GraphicsProfile.bUseLumen = Capabilities.bSupportsLumen;
		GraphicsProfile.bUseNanite = Capabilities.bSupportsNanite;
		GraphicsProfile.bUseVirtualShadowMaps = Capabilities.bSupportsVirtualShadowMaps;
		GraphicsProfile.bMotionBlur = true;
		GraphicsProfile.bFilmGrain = true;
		break;
	case EAHQualityPreset::Mobile:
	default:
		GraphicsProfile.RenderScale = 75.0f;
		GraphicsProfile.TextureQuality = 1;
		GraphicsProfile.ShadowQuality = 1;
		GraphicsProfile.EffectsQuality = 1;
		GraphicsProfile.PostProcessQuality = 1;
		GraphicsProfile.FoliageQuality = 1;
		GraphicsProfile.ShadowDistance = 3500.0f;
		GraphicsProfile.bUseDynamicResolution = true;
		GraphicsProfile.bMotionBlur = false;
		GraphicsProfile.bFilmGrain = false;
		GraphicsProfile.bCameraShake = false;
		break;
	}

	if (Capabilities.bIsMobile)
	{
		const bool bPerformanceMode = ActiveMobilePerformanceMode == EAHMobilePerformanceMode::Performance;
		const bool bQualityMode = ActiveMobilePerformanceMode == EAHMobilePerformanceMode::Quality;
		if (bPerformanceMode)
		{
			GraphicsProfile.RenderScale = 60.0f;
			GraphicsProfile.ShadowQuality = 0;
			GraphicsProfile.EffectsQuality = 0;
			GraphicsProfile.FoliageQuality = 0;
		}
		else if (bQualityMode)
		{
			GraphicsProfile.RenderScale = 85.0f;
			GraphicsProfile.TextureQuality = 2;
			GraphicsProfile.EffectsQuality = 2;
		}
	}

	PerformanceProfile = FAHPerformanceProfile();
	if (Capabilities.bIsMobile)
	{
		PerformanceProfile.TargetFrameRate = ActiveMobilePerformanceMode == EAHMobilePerformanceMode::Performance ? 30.0f : 60.0f;
		PerformanceProfile.MinimumStableFrameRate = ActiveMobilePerformanceMode == EAHMobilePerformanceMode::Performance ? 30.0f : 45.0f;
		PerformanceProfile.MaxActiveCombatants = ActiveMobilePerformanceMode == EAHMobilePerformanceMode::Performance ? 8 : 16;
		PerformanceProfile.MaxMidDistanceActors = 24;
		PerformanceProfile.MaxDistantSimulationActors = 48;
		PerformanceProfile.MidDistanceTickInterval = 0.20f;
		PerformanceProfile.MaxPersistentVFX = 24;
		PerformanceProfile.MaxDynamicLights = 4;
		PerformanceProfile.MaxProjectilePoolSize = 64;
		PerformanceProfile.ThermalMitigationAfterMinutes = 5;
	}

	InputProfile = FAHInputProfile();
	InputProfile.bUsesTouchControls = Capabilities.bSupportsTouch || Settings->bForceTouchControls;
	InputProfile.bSupportsGyroAiming = Capabilities.bSupportsGyro && Settings->bEnableGyroAiming;
	InputProfile.bSupportsExternalController = Capabilities.bSupportsExternalController;
	InputProfile.bAimAssistEnabled = Capabilities.bIsMobile && Settings->bEnableMobileAimAssist;
}

void UAHPlatformManagerSubsystem::BuildRuntimeInputActions()
{
	RuntimeInputMappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_PlatformFallback"));

	auto CreateAction = [this](const TCHAR* Name, EInputActionValueType ValueType)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = ValueType;
		return Action;
	};

	MoveAction = CreateAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = CreateAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	MouseLookAction = CreateAction(TEXT("IA_MouseLook"), EInputActionValueType::Axis2D);
	FireAction = CreateAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);
	ADSAction = CreateAction(TEXT("IA_ADS"), EInputActionValueType::Boolean);
	ReloadAction = CreateAction(TEXT("IA_Reload"), EInputActionValueType::Boolean);
	JumpAction = CreateAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	CrouchAction = CreateAction(TEXT("IA_Crouch"), EInputActionValueType::Boolean);
	SprintAction = CreateAction(TEXT("IA_Sprint"), EInputActionValueType::Boolean);
	MeleeAction = CreateAction(TEXT("IA_Melee"), EInputActionValueType::Boolean);
	GrenadeAction = CreateAction(TEXT("IA_Grenade"), EInputActionValueType::Boolean);
	InteractAction = CreateAction(TEXT("IA_Interact"), EInputActionValueType::Boolean);
	WeaponNextAction = CreateAction(TEXT("IA_WeaponNext"), EInputActionValueType::Boolean);
	WeaponPreviousAction = CreateAction(TEXT("IA_WeaponPrevious"), EInputActionValueType::Boolean);
	PauseAction = CreateAction(TEXT("IA_Pause"), EInputActionValueType::Boolean);
	VehicleAccelerateAction = CreateAction(TEXT("IA_VehicleAccelerate"), EInputActionValueType::Boolean);
	VehicleBrakeAction = CreateAction(TEXT("IA_VehicleBrake"), EInputActionValueType::Boolean);
	VehicleExitAction = CreateAction(TEXT("IA_VehicleExit"), EInputActionValueType::Boolean);
	VehicleSwitchSeatAction = CreateAction(TEXT("IA_VehicleSwitchSeat"), EInputActionValueType::Boolean);

	AddInputMapping(MoveAction, EKeys::W, false, true);
	AddInputMapping(MoveAction, EKeys::S, true, true);
	AddInputMapping(MoveAction, EKeys::A, true, false);
	AddInputMapping(MoveAction, EKeys::D, false, false);
	AddInputMapping(MoveAction, EKeys::Gamepad_LeftX);
	AddInputMapping(MoveAction, EKeys::Gamepad_LeftY);

	AddInputMapping(LookAction, EKeys::Gamepad_RightX);
	AddInputMapping(LookAction, EKeys::Gamepad_RightY, false, true);
	AddInputMapping(MouseLookAction, EKeys::MouseX);
	AddInputMapping(MouseLookAction, EKeys::MouseY, false, true);

	AddInputMapping(FireAction, EKeys::LeftMouseButton);
	AddInputMapping(FireAction, EKeys::Gamepad_RightTrigger);
	AddInputMapping(ADSAction, EKeys::RightMouseButton);
	AddInputMapping(ADSAction, EKeys::Gamepad_LeftTrigger);
	AddInputMapping(ReloadAction, EKeys::R);
	AddInputMapping(ReloadAction, EKeys::Gamepad_FaceButton_Left);
	AddInputMapping(JumpAction, EKeys::SpaceBar);
	AddInputMapping(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
	AddInputMapping(CrouchAction, EKeys::LeftControl);
	AddInputMapping(CrouchAction, EKeys::Gamepad_LeftThumbstick);
	AddInputMapping(SprintAction, EKeys::LeftShift);
	AddInputMapping(SprintAction, EKeys::Gamepad_LeftThumbstick);
	AddInputMapping(MeleeAction, EKeys::V);
	AddInputMapping(MeleeAction, EKeys::Gamepad_FaceButton_Right);
	AddInputMapping(GrenadeAction, EKeys::G);
	AddInputMapping(GrenadeAction, EKeys::Gamepad_RightShoulder);
	AddInputMapping(InteractAction, EKeys::E);
	AddInputMapping(InteractAction, EKeys::Gamepad_FaceButton_Left);
	AddInputMapping(WeaponNextAction, EKeys::MouseScrollDown);
	AddInputMapping(WeaponNextAction, EKeys::Gamepad_DPad_Right);
	AddInputMapping(WeaponPreviousAction, EKeys::MouseScrollUp);
	AddInputMapping(WeaponPreviousAction, EKeys::Gamepad_DPad_Left);
	AddInputMapping(PauseAction, EKeys::Escape);
	AddInputMapping(PauseAction, EKeys::Gamepad_Special_Right);
	AddInputMapping(VehicleAccelerateAction, EKeys::W);
	AddInputMapping(VehicleAccelerateAction, EKeys::Gamepad_RightTrigger);
	AddInputMapping(VehicleBrakeAction, EKeys::S);
	AddInputMapping(VehicleBrakeAction, EKeys::Gamepad_LeftTrigger);
	AddInputMapping(VehicleExitAction, EKeys::F);
	AddInputMapping(VehicleExitAction, EKeys::Gamepad_FaceButton_Left);
	AddInputMapping(VehicleSwitchSeatAction, EKeys::Q);
	AddInputMapping(VehicleSwitchSeatAction, EKeys::Gamepad_DPad_Up);
}

void UAHPlatformManagerSubsystem::AddInputMapping(UInputAction* Action, const FKey& Key, bool bNegate, bool bSwizzleToY)
{
	if (!RuntimeInputMappingContext || !Action)
	{
		return;
	}

	FEnhancedActionKeyMapping& Mapping = RuntimeInputMappingContext->MapKey(Action, Key);
	if (bSwizzleToY)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(RuntimeInputMappingContext);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		Mapping.Modifiers.Add(Swizzle);
	}
	if (bNegate)
	{
		UInputModifierNegate* Negate = NewObject<UInputModifierNegate>(RuntimeInputMappingContext);
		Mapping.Modifiers.Add(Negate);
	}
}

void UAHPlatformManagerSubsystem::ApplyCurrentQualitySettings()
{
	if (!GEngine)
	{
		return;
	}

	if (UGameUserSettings* UserSettings = GEngine->GetGameUserSettings())
	{
		UserSettings->SetOverallScalabilityLevel(FMath::Clamp(static_cast<int32>(ActiveQualityPreset) - 1, 0, 4));
		UserSettings->SetResolutionScaleValueEx(GraphicsProfile.RenderScale);
		UserSettings->SetFrameRateLimit(PerformanceProfile.TargetFrameRate);
		UserSettings->ApplySettings(false);
	}

	if (IConsoleVariable* DynamicResolution = IConsoleManager::Get().FindConsoleVariable(TEXT("r.DynamicRes.OperationMode")))
	{
		DynamicResolution->Set(GraphicsProfile.bUseDynamicResolution ? 2 : 0, ECVF_SetByGameSetting);
	}
	if (IConsoleVariable* MotionBlur = IConsoleManager::Get().FindConsoleVariable(TEXT("r.MotionBlurQuality")))
	{
		MotionBlur->Set(GraphicsProfile.bMotionBlur ? GraphicsProfile.PostProcessQuality : 0, ECVF_SetByGameSetting);
	}

	const auto SetQualityCVar = [](const TCHAR* Name, int32 Value)
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			CVar->Set(Value, ECVF_SetByGameSetting);
		}
	};
	SetQualityCVar(TEXT("r.Nanite.ProjectEnabled"), GraphicsProfile.bUseNanite ? 1 : 0);
	SetQualityCVar(TEXT("r.Shadow.Virtual.Enable"), GraphicsProfile.bUseVirtualShadowMaps ? 1 : 0);
	SetQualityCVar(TEXT("r.DynamicGlobalIlluminationMethod"), GraphicsProfile.bUseLumen ? 1 : 0);
	SetQualityCVar(TEXT("r.ReflectionMethod"), GraphicsProfile.bUseLumen ? 1 : 0);
	// Apple Metal can expose Lumen without accepting every hardware ray-tracing
	// vertex descriptor emitted by the cooked Shipping pipeline. Keep the
	// high-end lighting path on Mac, but use the stable software fallback for
	// ray-traced effects. Windows retains the full desktop profile.
	const bool bUseHardwareRayTracing = GraphicsProfile.bUseLumen
		&& !Capabilities.bIsMobile
		&& Capabilities.PlatformFamily != EAHPlatformFamily::Mac;
	SetQualityCVar(TEXT("r.RayTracing"), bUseHardwareRayTracing ? 1 : 0);
}

void UAHPlatformManagerSubsystem::ApplyQualityPreset(EAHQualityPreset Preset)
{
	ActiveQualityPreset = ResolveQualityPreset(Preset);
	bRuntimeQualityOverride = true;
	BuildProfiles();
	ApplyCurrentQualitySettings();
}

void UAHPlatformManagerSubsystem::SetMobilePerformanceMode(EAHMobilePerformanceMode Mode)
{
	ActiveMobilePerformanceMode = Mode;
	bRuntimeMobilePerformanceOverride = true;
	BuildProfiles();
	ApplyCurrentQualitySettings();
}

bool UAHPlatformManagerSubsystem::ShouldUseTouchControls() const
{
	return InputProfile.bUsesTouchControls;
}

bool UAHPlatformManagerSubsystem::TryRegisterActiveCombatant()
{
	if (ActiveCombatants >= PerformanceProfile.MaxActiveCombatants)
	{
		return false;
	}

	++ActiveCombatants;
	return true;
}

void UAHPlatformManagerSubsystem::UnregisterActiveCombatant()
{
	ActiveCombatants = FMath::Max(0, ActiveCombatants - 1);
}

void UAHPlatformManagerSubsystem::HandleApplicationWillEnterBackground()
{
	bApplicationSuspended = true;
	SaveForApplicationSuspend();
	OnApplicationSuspended.Broadcast();
}

void UAHPlatformManagerSubsystem::HandleApplicationHasEnteredForeground()
{
	bApplicationSuspended = false;
	OnApplicationResumed.Broadcast();
}

void UAHPlatformManagerSubsystem::SaveForApplicationSuspend()
{
	const UAHPlatformSettings* Settings = UAHPlatformSettings::Get();
	if (!Settings->bSaveOnApplicationSuspend)
	{
		return;
	}

	if (UAHPlatformSaveSubsystem* SaveSubsystem = GetGameInstance()->GetSubsystem<UAHPlatformSaveSubsystem>())
	{
		SaveSubsystem->SaveSuspensionCheckpoint();
	}
}
