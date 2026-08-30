#include "Gameplay/Audio/AHAudioSubsystem.h"

#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioPaletteData.h"
#include "Gameplay/Audio/AHAudioSettings.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

void UAHAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UAHAudioSettings* Settings = GetDefault<UAHAudioSettings>();
	const FSoftObjectPath PalettePath = Settings && Settings->DefaultPalette.IsValid()
		? Settings->DefaultPalette
		: FSoftObjectPath(TEXT("/Game/Ashes/Audio/DA_AudioPalette_Default.DA_AudioPalette_Default"));
	AudioPalette = Cast<UAHAudioPaletteData>(PalettePath.TryLoad());
	bAudioPaletteReady = AudioPalette != nullptr;
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Audio] palette=%s authored=%s"),
		*PalettePath.ToString(), bAudioPaletteReady ? TEXT("ready") : TEXT("missing"));

	if (GetWorld())
	{
		if (GetWorld()->GetGameInstance())
		{
			if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
			{
				Chapter->OnStageChanged.AddDynamic(this, &UAHAudioSubsystem::HandleChapterStageChanged);
			}
		}
	}
}

void UAHAudioSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		if (GetWorld()->GetGameInstance())
		{
			if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
			{
				Chapter->OnStageChanged.RemoveDynamic(this, &UAHAudioSubsystem::HandleChapterStageChanged);
			}
		}
	}
	ActiveEnvironmentComponent = nullptr;
	AudioPalette = nullptr;
	bAudioPaletteReady = false;
	Super::Deinitialize();
}

void UAHAudioSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (IsRunningCommandlet() || InWorld.WorldType == EWorldType::EditorPreview)
	{
		return;
	}
	if (UAHChapterSubsystem* Chapter = InWorld.GetGameInstance() ? InWorld.GetGameInstance()->GetSubsystem<UAHChapterSubsystem>() : nullptr)
	{
		HandleChapterStageChanged(Chapter->GetStage());
	}
	else
	{
		HandleChapterStageChanged(EAHChapterStage::ErebusOpening);
	}
}

FName UAHAudioSubsystem::GetSemanticEventName(EAHAudioCue Cue) const
{
	switch (Cue)
	{
	case EAHAudioCue::Shot: return FName(TEXT("Weapon.M91.Fire"));
	case EAHAudioCue::Reload: return FName(TEXT("Weapon.M91.Reload"));
	case EAHAudioCue::Empty: return FName(TEXT("Weapon.M91.Empty"));
	case EAHAudioCue::Impact: return FName(TEXT("Weapon.M91.Impact"));
	case EAHAudioCue::Melee: return FName(TEXT("Combat.Melee"));
	case EAHAudioCue::Hurt: return FName(TEXT("Combat.Hurt"));
	case EAHAudioCue::Armor: return FName(TEXT("Combat.Armor"));
	case EAHAudioCue::Death: return FName(TEXT("Combat.Death"));
	case EAHAudioCue::Grenade: return FName(TEXT("Combat.Grenade"));
	case EAHAudioCue::Objective: return FName(TEXT("UI.Objective"));
	case EAHAudioCue::Dialogue: return FName(TEXT("UI.Dialogue"));
	case EAHAudioCue::Pickup: return FName(TEXT("UI.Pickup"));
	case EAHAudioCue::Footstep: return FName(TEXT("Player.Footstep"));
	case EAHAudioCue::FootstepRun: return FName(TEXT("Player.Footstep.Run"));
	case EAHAudioCue::Ambient: return FName(TEXT("Environment.Erebus"));
	default: return NAME_None;
	}
}

USoundBase* UAHAudioSubsystem::ResolveAuthoredCue(EAHAudioCue Cue)
{
	if (!AudioPalette)
	{
		return nullptr;
	}
	if (const TSoftObjectPtr<USoundBase>* Entry = AudioPalette->Events.Find(GetSemanticEventName(Cue)))
	{
		return Entry->LoadSynchronous();
	}
	return nullptr;
}

USoundBase* UAHAudioSubsystem::ResolveAuthoredEnvironment(FName EnvironmentId)
{
	if (!AudioPalette)
	{
		return nullptr;
	}
	if (const TSoftObjectPtr<USoundBase>* Entry = AudioPalette->Environments.Find(EnvironmentId))
	{
		return Entry->LoadSynchronous();
	}
	return nullptr;
}

bool UAHAudioSubsystem::HasAuthoredCue(EAHAudioCue Cue) const
{
	return AudioPalette && AudioPalette->Events.Contains(GetSemanticEventName(Cue));
}

FName UAHAudioSubsystem::GetEnvironmentForStage(EAHChapterStage Stage) const
{
	switch (Stage)
	{
	case EAHChapterStage::TransitStation: return FName(TEXT("Environment.Transit"));
	case EAHChapterStage::ManticoreSection: return FName(TEXT("Environment.Manticore"));
	case EAHChapterStage::CathedralApproach:
	case EAHChapterStage::FailsafeOrder:
	case EAHChapterStage::CathedralInterior:
	case EAHChapterStage::SaelTransmission:
	case EAHChapterStage::FailsafeTerminal:
	case EAHChapterStage::Escape: return FName(TEXT("Environment.Cathedral"));
	case EAHChapterStage::TenYearsLater:
	case EAHChapterStage::MayaScene:
	case EAHChapterStage::NysaTransmission:
	case EAHChapterStage::FleetDeparture:
	case EAHChapterStage::StarsDisappearing: return FName(TEXT("Environment.PresentDay"));
	default: return FName(TEXT("Environment.Erebus"));
	}
}

void UAHAudioSubsystem::HandleChapterStageChanged(EAHChapterStage Stage)
{
	if (!GetWorld() || IsRunningCommandlet())
	{
		return;
	}
	const FName EnvironmentId = GetEnvironmentForStage(Stage);
	if (USoundBase* Environment = ResolveAuthoredEnvironment(EnvironmentId))
	{
		if (ActiveEnvironmentComponent)
		{
			ActiveEnvironmentComponent->FadeOut(0.6f, 0.0f);
		}
		ActiveEnvironmentComponent = UGameplayStatics::SpawnSound2D(this, Environment, 0.35f, 1.0f, 0.0f, nullptr, true, true);
		if (ActiveEnvironmentComponent)
		{
			ActiveEnvironmentComponent->FadeIn(0.8f, 0.35f);
		}
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Audio] Stage=%s Environment=%s Asset=%s"), *UEnum::GetValueAsString(Stage), *EnvironmentId.ToString(), *GetNameSafe(Environment));
	}
	else
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Audio] authored environment missing id=%s; no fallback is permitted"), *EnvironmentId.ToString());
	}
}

void UAHAudioSubsystem::PlayWorldCue(EAHAudioCue Cue, const FVector& Location, float VolumeMultiplier, float PitchMultiplier)
{
	if (!bAudioPaletteReady || !GetWorld()) return;
	if (USoundBase* AuthoredCue = ResolveAuthoredCue(Cue))
	{
		UGameplayStatics::PlaySoundAtLocation(this, AuthoredCue, Location, VolumeMultiplier, PitchMultiplier);
		return;
	}
	UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.2][Audio] authored event missing=%s; playback skipped"),
		*GetSemanticEventName(Cue).ToString());
}

void UAHAudioSubsystem::PlayUICue(EAHAudioCue Cue, float VolumeMultiplier, float PitchMultiplier)
{
	if (!bAudioPaletteReady || !GetWorld()) return;
	if (USoundBase* AuthoredCue = ResolveAuthoredCue(Cue))
	{
		UGameplayStatics::PlaySound2D(this, AuthoredCue, VolumeMultiplier, PitchMultiplier);
		return;
	}
	UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.2][Audio] authored UI event missing=%s; playback skipped"),
		*GetSemanticEventName(Cue).ToString());
}
