#include "Gameplay/Audio/AHAudioSubsystem.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Game/AHCombatPlayerController.h"
#include "Gameplay/UI/AHCombatHUD.h"
#include "Gameplay/UI/AHHUDRootWidget.h"
#include "AshesOfHeaven.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "UObject/SoftObjectPath.h"

#if !UE_BUILD_SHIPPING
namespace
{
	AAHCombatPlayerController* GetCombatController(UWorld* World)
	{
		return World ? Cast<AAHCombatPlayerController>(World->GetFirstPlayerController()) : nullptr;
	}

	void LogWidgetState(UWorld* World, const FString& Mode)
	{
		AAHCombatPlayerController* Controller = GetCombatController(World);
		AAHCombatHUD* HUD = Controller ? Cast<AAHCombatHUD>(Controller->GetHUD()) : nullptr;
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][UI] mode=%s hud=%s umg_root=%s"), *Mode, *GetNameSafe(HUD), HUD && HUD->GetRootWidget() ? TEXT("ready") : TEXT("missing"));
	}

	FAutoConsoleCommandWithWorldAndArgs DebugUI(
		TEXT("AH.Debug.UI"), TEXT("Inspect the production UMG presentation root."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World) { LogWidgetState(World, TEXT("root")); }));
	FAutoConsoleCommandWithWorldAndArgs DebugObjective(
		TEXT("AH.Debug.UI.Objective"), TEXT("Log the current event-driven objective presentation."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			LogWidgetState(World, TEXT("objective"));
			if (AAHCombatPlayerController* Controller = GetCombatController(World))
			{
				if (AAHCombatHUD* HUD = Cast<AAHCombatHUD>(Controller->GetHUD()))
				{
					UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][UI] objective index=%d/%d text=%s"), HUD->GetObjectiveIndex(), HUD->GetObjectiveCount(), *HUD->GetCurrentObjective().ToString());
				}
			}
		}));
	FAutoConsoleCommandWithWorldAndArgs DebugDamage(
		TEXT("AH.Debug.UI.Damage"), TEXT("Trigger a development damage indicator."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (AAHCombatPlayerController* Controller = GetCombatController(World)) if (AAHCombatHUD* HUD = Cast<AAHCombatHUD>(Controller->GetHUD())) HUD->ShowDamageFeedback(false, 0.0f);
		}));
	FAutoConsoleCommandWithWorldAndArgs DebugCountdown(
		TEXT("AH.Debug.UI.Countdown"), TEXT("Log countdown UI contract."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			LogWidgetState(World, TEXT("countdown"));
			if (World && World->GetGameInstance()) if (UAHChapterSubsystem* Chapter = World->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>()) UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][UI] countdown active=%s seconds=%0.1f"), Chapter->IsCountdownActive() ? TEXT("true") : TEXT("false"), Chapter->GetCountdownSeconds());
		}));
	FAutoConsoleCommandWithWorldAndArgs DebugAudioM91(
		TEXT("AH.Debug.Audio.M91"), TEXT("Play the authored M91 semantic event."),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (World) if (UAHAudioSubsystem* Audio = World->GetSubsystem<UAHAudioSubsystem>()) { UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][Audio] M91 authored=%s"), Audio->HasAuthoredCue(EAHAudioCue::Shot) ? TEXT("true") : TEXT("false")); Audio->PlayUICue(EAHAudioCue::Shot); }
		}));
	FAutoConsoleCommandWithWorldAndArgs DebugAudioErebus(TEXT("AH.Debug.Audio.Erebus"), TEXT("Log Erebus audio palette."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World) { if (World) if (UAHAudioSubsystem* Audio = World->GetSubsystem<UAHAudioSubsystem>()) UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][Audio] Erebus palette=%s"), Audio->IsAudioPaletteReady() ? TEXT("ready") : TEXT("missing")); }));
	FAutoConsoleCommandWithWorldAndArgs DebugAudioTransit(TEXT("AH.Debug.Audio.Transit"), TEXT("Log Transit audio target."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*) { UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][Audio] Transit environment contract=Environment.Transit")); }));
	FAutoConsoleCommandWithWorldAndArgs DebugAudioCathedral(TEXT("AH.Debug.Audio.Cathedral"), TEXT("Log Cathedral audio target."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*) { UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][Audio] Cathedral environment contract=Environment.Cathedral")); }));
	FAutoConsoleCommandWithWorldAndArgs DebugMaterials(TEXT("AH.Debug.Materials"), TEXT("Verify authored material target paths."), FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld*) { const TCHAR* Paths[] = { TEXT("/Game/Ashes/Materials/M_HumanMetal.M_HumanMetal"), TEXT("/Game/Ashes/Materials/M_VeilObsidian.M_VeilObsidian"), TEXT("/Game/Ashes/Materials/M_CathedralMatter.M_CathedralMatter") }; for (const TCHAR* Path : Paths) UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Debug][Materials] %s=%s"), Path, FSoftObjectPath(Path).ResolveObject() ? TEXT("loaded") : TEXT("soft")); }));
}
#endif
