#include "Misc/AutomationTest.h"

#include "Platform/AHPlatformManagerSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "EnhancedActionKeyMapping.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

#if WITH_DEV_AUTOMATION_TESTS

/**
 *  Mouse and stick pitch shipped inverted because the generated fallback context mapped
 *  MouseY/Gamepad_RightY straight through. Those keys are positive when the device moves UP,
 *  and the project runs with legacy input scales, where BaseGame.ini's InputPitchScale=-2.5
 *  flips the sign again inside APlayerController::AddPitchInput. Either half of that pair
 *  changing without the other silently inverts the whole game, so both are asserted here.
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHLookPitchIsNotInvertedTest,
	"AshesOfHeaven.Input.LookPitchIsNotInverted",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHLookPitchIsNotInvertedTest::RunTest(const FString& Parameters)
{
	const bool bLegacyScales = GetDefault<UInputSettings>()->bEnableLegacyInputScales;
	TestTrue(TEXT("project still runs with legacy input scales"), bLegacyScales);

	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->AddToRoot();
	GameInstance->InitializeStandalone(FName(TEXT("AHLookInputWorld")));

	UAHPlatformManagerSubsystem* Platform = GameInstance->GetSubsystem<UAHPlatformManagerSubsystem>();
	if (!TestNotNull(TEXT("platform manager subsystem exists"), Platform))
	{
		GameInstance->Shutdown();
		GameInstance->RemoveFromRoot();
		return false;
	}

	const UInputMappingContext* Context = Platform->GetRuntimeInputMappingContext();
	if (!TestNotNull(TEXT("runtime fallback mapping context exists"), Context))
	{
		GameInstance->Shutdown();
		GameInstance->RemoveFromRoot();
		return false;
	}

	auto PitchKeyIsNegated = [Context](const FKey& Key, bool& bOutFound)
	{
		bOutFound = false;
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Key != Key)
			{
				continue;
			}
			bOutFound = true;
			for (const UInputModifier* Modifier : Mapping.Modifiers)
			{
				if (Cast<UInputModifierNegate>(Modifier))
				{
					return true;
				}
			}
			return false;
		}
		return false;
	};

	bool bFoundMouse = false;
	const bool bMouseNegated = PitchKeyIsNegated(EKeys::MouseY, bFoundMouse);
	TestTrue(TEXT("fallback context maps MouseY"), bFoundMouse);
	TestTrue(TEXT("MouseY is negated so mouse-up looks up"), bMouseNegated);

	bool bFoundStick = false;
	const bool bStickNegated = PitchKeyIsNegated(EKeys::Gamepad_RightY, bFoundStick);
	TestTrue(TEXT("fallback context maps Gamepad_RightY"), bFoundStick);
	TestTrue(TEXT("Gamepad_RightY is negated so stick-up looks up"), bStickNegated);

	GameInstance->Shutdown();
	GameInstance->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FAHInvertLookYPreferenceTest,
	"AshesOfHeaven.Input.InvertLookYPreference",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::ProductFilter)

bool FAHInvertLookYPreferenceTest::RunTest(const FString& Parameters)
{
	const bool bOriginal = UAHPlatformManagerSubsystem::IsLookYInverted();

	UAHPlatformManagerSubsystem::SetLookYInverted(false);
	TestFalse(TEXT("preference reads back off"), UAHPlatformManagerSubsystem::IsLookYInverted());
	TestEqual(TEXT("pitch passes through when not inverted"), UAHPlatformManagerSubsystem::GetLookPitchSign(), 1.0f);

	UAHPlatformManagerSubsystem::SetLookYInverted(true);
	TestTrue(TEXT("preference reads back on"), UAHPlatformManagerSubsystem::IsLookYInverted());
	TestEqual(TEXT("pitch flips when inverted"), UAHPlatformManagerSubsystem::GetLookPitchSign(), -1.0f);

	UAHPlatformManagerSubsystem::SetLookYInverted(bOriginal);
	return true;
}

#endif
