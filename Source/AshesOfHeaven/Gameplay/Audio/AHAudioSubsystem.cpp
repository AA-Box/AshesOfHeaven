#include "Gameplay/Audio/AHAudioSubsystem.h"

#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioPaletteData.h"
#include "Gameplay/Audio/AHAudioSettings.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Gameplay/Chapter/AHDialogueSubsystem.h"
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

}

void UAHAudioSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		if (UAHDialogueSubsystem* Dialogue = GetWorld()->GetSubsystem<UAHDialogueSubsystem>())
		{
			Dialogue->OnLineChanged.RemoveDynamic(this, &UAHAudioSubsystem::HandleDialogueLine);
			Dialogue->OnSequenceComplete.RemoveDynamic(this, &UAHAudioSubsystem::HandleDialogueComplete);
		}
		if (GetWorld()->GetGameInstance())
		{
			if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
			{
				Chapter->OnStageChanged.RemoveDynamic(this, &UAHAudioSubsystem::HandleChapterStageChanged);
			}
		}
	}
	if (ActiveEnvironmentComponent) ActiveEnvironmentComponent->Stop();
	if (FadingEnvironmentComponent) FadingEnvironmentComponent->Stop();
	ActiveEnvironmentComponent = nullptr;
	FadingEnvironmentComponent = nullptr;
	ActiveEnvironmentId = NAME_None;
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
		// The game instance is attached by BeginPlay, but may not exist at Initialize.
		Chapter->OnStageChanged.AddUniqueDynamic(this, &UAHAudioSubsystem::HandleChapterStageChanged);
		HandleChapterStageChanged(Chapter->GetStage());
	}
	else
	{
		HandleChapterStageChanged(EAHChapterStage::ErebusOpening);
	}
	if (UAHDialogueSubsystem* Dialogue = InWorld.GetSubsystem<UAHDialogueSubsystem>())
	{
		Dialogue->OnLineChanged.AddUniqueDynamic(this, &UAHAudioSubsystem::HandleDialogueLine);
		Dialogue->OnSequenceComplete.AddUniqueDynamic(this, &UAHAudioSubsystem::HandleDialogueComplete);
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

FName UAHAudioSubsystem::GetEnvironmentForStage(EAHChapterStage Stage)
{
	switch (Stage)
	{
	case EAHChapterStage::OpeningBlack:
	case EAHChapterStage::ChapterComplete: return NAME_None;
	case EAHChapterStage::TransitStation:
	case EAHChapterStage::VeilRevelation: return FName(TEXT("Environment.Transit"));
	case EAHChapterStage::ManticoreSection: return FName(TEXT("Environment.Manticore"));
	case EAHChapterStage::CathedralApproach:
	case EAHChapterStage::FailsafeOrder:
	case EAHChapterStage::CathedralInterior:
	case EAHChapterStage::SaelTransmission:
	case EAHChapterStage::FailsafeTerminal:
	case EAHChapterStage::Escape:
	case EAHChapterStage::OtherLucian:
	case EAHChapterStage::ErebusDestruction: return FName(TEXT("Environment.Cathedral"));
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
	if (EnvironmentId == ActiveEnvironmentId && IsValid(ActiveEnvironmentComponent) && ActiveEnvironmentComponent->IsPlaying())
	{
		return;
	}
	if (EnvironmentId.IsNone())
	{
		if (FadingEnvironmentComponent) FadingEnvironmentComponent->Stop();
		FadingEnvironmentComponent = ActiveEnvironmentComponent;
		if (FadingEnvironmentComponent) FadingEnvironmentComponent->FadeOut(1.5f, 0.0f);
		ActiveEnvironmentComponent = nullptr;
		ActiveEnvironmentId = NAME_None;
		return;
	}
	if (USoundBase* Environment = ResolveAuthoredEnvironment(EnvironmentId))
	{
		if (FadingEnvironmentComponent) FadingEnvironmentComponent->Stop();
		FadingEnvironmentComponent = ActiveEnvironmentComponent;
		if (ActiveEnvironmentComponent)
		{
			ActiveEnvironmentComponent->FadeOut(1.5f, 0.0f);
		}
		ActiveEnvironmentComponent = UGameplayStatics::SpawnSound2D(this, Environment, 1.0f, 1.0f, 0.0f, nullptr, false, true);
		ActiveEnvironmentId = EnvironmentId;
		if (ActiveEnvironmentComponent)
		{
			ActiveEnvironmentComponent->FadeIn(1.5f, bDialogueActive ? 0.16f : 0.35f);
		}
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.4][Audio] Stage=%s Environment=%s Asset=%s"), *UEnum::GetValueAsString(Stage), *EnvironmentId.ToString(), *GetNameSafe(Environment));
	}
	else
	{
		UE_LOG(LogAshesOfHeaven, Error, TEXT("[Phase4.4][Audio] authored environment missing id=%s; no fallback is permitted"), *EnvironmentId.ToString());
	}
}

void UAHAudioSubsystem::HandleDialogueLine(FName Speaker, FText Subtitle, float Duration)
{
	bDialogueActive = true;
	if (ActiveEnvironmentComponent) ActiveEnvironmentComponent->AdjustVolume(0.18f, 0.16f);
}

void UAHAudioSubsystem::HandleDialogueComplete(FName SequenceId)
{
	// A completed one-shot may notify listeners while a different sequence is still
	// speaking. Reentrant listeners may also have started a replacement sequence.
	const UAHDialogueSubsystem* Dialogue = GetWorld() ? GetWorld()->GetSubsystem<UAHDialogueSubsystem>() : nullptr;
	bDialogueActive = Dialogue && Dialogue->HasActiveDialogue();
	if (ActiveEnvironmentComponent)
	{
		ActiveEnvironmentComponent->AdjustVolume(bDialogueActive ? 0.18f : 0.75f, bDialogueActive ? 0.16f : 0.35f);
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
