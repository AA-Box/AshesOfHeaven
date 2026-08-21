#include "Gameplay/Audio/AHAudioSubsystem.h"

#include "AshesOfHeaven.h"
#include "Gameplay/Audio/AHAudioPaletteData.h"
#include "Gameplay/Audio/AHAudioSettings.h"
#include "Gameplay/Chapter/AHChapterSubsystem.h"
#include "Gameplay/Chapter/AHChapterTypes.h"
#include "Components/AudioComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
	constexpr int32 SampleRate = 48000;
	constexpr float TwoPi = 6.28318530718f;

	float Envelope(float Time, float Duration, float Attack = 0.004f, float Release = 0.08f)
	{
		const float AttackShape = FMath::Clamp(Time / FMath::Max(Attack, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		const float ReleaseShape = FMath::Clamp((Duration - Time) / FMath::Max(Release, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
		return AttackShape * ReleaseShape;
	}

	float Tone(float Frequency, float Time)
	{
		return FMath::Sin(TwoPi * Frequency * Time);
	}

	float StableNoise(float Time)
	{
		return FMath::Frac(FMath::Sin(Time * 1297.31f) * 43758.5453f) * 2.0f - 1.0f;
	}
}

void UAHAudioSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	const UAHAudioSettings* Settings = GetDefault<UAHAudioSettings>();
	const FSoftObjectPath PalettePath = Settings && Settings->DefaultPalette.IsValid()
		? Settings->DefaultPalette
		: FSoftObjectPath(TEXT("/Game/Ashes/Audio/DA_AudioPalette_Default.DA_AudioPalette_Default"));
	AudioPalette = Cast<UAHAudioPaletteData>(PalettePath.TryLoad());
	bAudioPaletteReady = AudioPalette != nullptr;
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Audio] palette=%s authored=%s generated_fallback=%s"),
		*PalettePath.ToString(), bAudioPaletteReady ? TEXT("ready") : TEXT("missing"), ShouldUseGeneratedFallback() ? TEXT("enabled") : TEXT("disabled"));

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(CleanupTimer, this, &UAHAudioSubsystem::CleanupActiveWaves, 1.0f, true);
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
		GetWorld()->GetTimerManager().ClearTimer(CleanupTimer);
		if (GetWorld()->GetGameInstance())
		{
			if (UAHChapterSubsystem* Chapter = GetWorld()->GetGameInstance()->GetSubsystem<UAHChapterSubsystem>())
			{
				Chapter->OnStageChanged.RemoveDynamic(this, &UAHAudioSubsystem::HandleChapterStageChanged);
			}
		}
	}
	ActiveWaves.Reset();
	ActiveWaveExpiryTimes.Reset();
	AmbientWave = nullptr;
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

bool UAHAudioSubsystem::ShouldUseGeneratedFallback() const
{
	return GetDefault<UAHAudioSettings>()->bAllowGeneratedAudioFallback && !UE_BUILD_SHIPPING;
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
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.2][Audio] authored environment changed id=%s asset=%s"), *EnvironmentId.ToString(), *GetNameSafe(Environment));
	}
	else if (ShouldUseGeneratedFallback())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.2][Audio] environment missing id=%s; generated fallback is opt-in only"), *EnvironmentId.ToString());
	}
}

float UAHAudioSubsystem::GetCueDuration(EAHAudioCue Cue) const
{
	switch (Cue)
	{
	case EAHAudioCue::Shot: return 0.105f;
	case EAHAudioCue::Reload: return 0.24f;
	case EAHAudioCue::Empty: return 0.075f;
	case EAHAudioCue::Impact: return 0.13f;
	case EAHAudioCue::Melee: return 0.16f;
	case EAHAudioCue::Hurt: return 0.12f;
	case EAHAudioCue::Armor: return 0.15f;
	case EAHAudioCue::Death: return 0.34f;
	case EAHAudioCue::Grenade: return 0.42f;
	case EAHAudioCue::Objective: return 0.34f;
	case EAHAudioCue::Dialogue: return 0.11f;
	case EAHAudioCue::Pickup: return 0.20f;
	case EAHAudioCue::Footstep: return 0.12f;
	case EAHAudioCue::Ambient: return 2.0f;
	default: return 0.10f;
	}
}

void UAHAudioSubsystem::FillSamples(EAHAudioCue Cue, TArray<int16>& OutSamples, float StartTimeSeconds) const
{
	const float Duration = GetCueDuration(Cue);
	for (int32 Index = 0; Index < OutSamples.Num(); ++Index)
	{
		const float Time = StartTimeSeconds + static_cast<float>(Index) / SampleRate;
		const float LocalTime = FMath::Fmod(Time, Duration);
		const float Fade = Envelope(LocalTime, Duration, Cue == EAHAudioCue::Ambient ? 0.08f : 0.0025f, Cue == EAHAudioCue::Ambient ? 0.18f : 0.06f);
		float Sample = 0.0f;
		switch (Cue)
		{
		case EAHAudioCue::Shot: Sample = (Tone(92.0f, LocalTime) * 0.58f + Tone(780.0f, LocalTime) * 0.22f + StableNoise(LocalTime) * 0.18f) * Fade; break;
		case EAHAudioCue::Reload: Sample = (Tone(1550.0f, LocalTime) * FMath::Exp(-LocalTime * 115.0f) + Tone(1120.0f, FMath::Max(0.0f, LocalTime - 0.115f)) * 0.42f) * Fade; break;
		case EAHAudioCue::Empty: Sample = (Tone(430.0f, LocalTime) * 0.45f + StableNoise(LocalTime) * 0.12f) * Fade; break;
		case EAHAudioCue::Impact: Sample = (StableNoise(LocalTime) * 0.56f + Tone(86.0f, LocalTime) * 0.42f) * Fade; break;
		case EAHAudioCue::Melee: Sample = (StableNoise(LocalTime * 1.8f) * 0.38f + Tone(210.0f, LocalTime) * 0.20f) * Fade; break;
		case EAHAudioCue::Hurt: Sample = (Tone(148.0f, LocalTime) * 0.42f + Tone(310.0f, LocalTime) * 0.12f) * Fade; break;
		case EAHAudioCue::Armor: Sample = (Tone(980.0f, LocalTime) * 0.35f + Tone(520.0f, LocalTime) * 0.24f) * Fade; break;
		case EAHAudioCue::Death: Sample = (Tone(FMath::Lerp(260.0f, 72.0f, LocalTime / Duration), LocalTime) * 0.45f + StableNoise(LocalTime) * 0.10f) * Fade; break;
		case EAHAudioCue::Grenade: Sample = (Tone(58.0f, LocalTime) * 0.68f + StableNoise(LocalTime) * 0.42f) * Fade; break;
		case EAHAudioCue::Objective: Sample = Tone(LocalTime < 0.11f ? 520.0f : (LocalTime < 0.22f ? 740.0f : 1040.0f), LocalTime) * Fade * 0.34f; break;
		case EAHAudioCue::Dialogue: Sample = (Tone(420.0f, LocalTime) * 0.22f + Tone(630.0f, LocalTime) * 0.15f) * Fade; break;
		case EAHAudioCue::Pickup: Sample = Tone(FMath::Lerp(440.0f, 880.0f, LocalTime / Duration), LocalTime) * Fade * 0.26f; break;
		case EAHAudioCue::Footstep: Sample = (StableNoise(LocalTime * 3.4f) * 0.42f + Tone(72.0f, LocalTime) * 0.26f) * Fade; break;
		case EAHAudioCue::Ambient: Sample = (Tone(47.0f, Time) * 0.26f + Tone(93.0f, Time) * 0.11f + StableNoise(Time * 0.23f) * 0.035f) * 0.75f; break;
		default: break;
		}
		OutSamples[Index] = static_cast<int16>(FMath::Clamp(Sample, -1.0f, 1.0f) * 32767.0f);
	}
}

USoundWaveProcedural* UAHAudioSubsystem::CreateCueWave(EAHAudioCue Cue, float& OutDuration) const
{
	OutDuration = GetCueDuration(Cue);
	const int32 NumSamples = FMath::Max(1, FMath::RoundToInt(OutDuration * SampleRate));
	TArray<int16> Samples;
	Samples.SetNumZeroed(NumSamples);
	FillSamples(Cue, Samples);
	USoundWaveProcedural* Wave = NewObject<USoundWaveProcedural>(const_cast<UAHAudioSubsystem*>(this));
	if (!Wave) return nullptr;
	Wave->SetSampleRate(SampleRate);
	Wave->SetNumFrames(NumSamples);
	Wave->NumChannels = 1;
	Wave->SoundGroup = SOUNDGROUP_Effects;
	Wave->bLooping = false;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
	return Wave;
}

void UAHAudioSubsystem::PlayWorldCue(EAHAudioCue Cue, const FVector& Location, float VolumeMultiplier, float PitchMultiplier)
{
	if (!bAudioPaletteReady || !GetWorld()) return;
	if (USoundBase* AuthoredCue = ResolveAuthoredCue(Cue))
	{
		UGameplayStatics::PlaySoundAtLocation(this, AuthoredCue, Location, VolumeMultiplier, PitchMultiplier);
		return;
	}
	if (!ShouldUseGeneratedFallback())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.2][Audio] missing authored event=%s; playback skipped"), *GetSemanticEventName(Cue).ToString());
		return;
	}
	float Duration = 0.0f;
	if (USoundWaveProcedural* Wave = CreateCueWave(Cue, Duration))
	{
		ActiveWaves.Add(Wave);
		ActiveWaveExpiryTimes.Add(GetWorld()->GetTimeSeconds() + Duration + 0.5f);
		UGameplayStatics::PlaySoundAtLocation(this, Wave, Location, VolumeMultiplier, PitchMultiplier);
	}
}

void UAHAudioSubsystem::PlayUICue(EAHAudioCue Cue, float VolumeMultiplier, float PitchMultiplier)
{
	if (!bAudioPaletteReady || !GetWorld()) return;
	if (USoundBase* AuthoredCue = ResolveAuthoredCue(Cue))
	{
		UGameplayStatics::PlaySound2D(this, AuthoredCue, VolumeMultiplier, PitchMultiplier);
		return;
	}
	if (!ShouldUseGeneratedFallback())
	{
		UE_LOG(LogAshesOfHeaven, Warning, TEXT("[Phase4.2][Audio] missing authored UI event=%s; playback skipped"), *GetSemanticEventName(Cue).ToString());
		return;
	}
	float Duration = 0.0f;
	if (USoundWaveProcedural* Wave = CreateCueWave(Cue, Duration))
	{
		ActiveWaves.Add(Wave);
		ActiveWaveExpiryTimes.Add(GetWorld()->GetTimeSeconds() + Duration + 0.5f);
		UGameplayStatics::PlaySound2D(this, Wave, VolumeMultiplier, PitchMultiplier);
	}
}

void UAHAudioSubsystem::CleanupActiveWaves()
{
	if (!GetWorld()) return;
	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = ActiveWaves.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveWaveExpiryTimes.IsValidIndex(Index) || ActiveWaveExpiryTimes[Index] <= Now)
		{
			ActiveWaves.RemoveAt(Index);
			if (ActiveWaveExpiryTimes.IsValidIndex(Index)) ActiveWaveExpiryTimes.RemoveAt(Index);
		}
	}
}

void UAHAudioSubsystem::HandleAmbientUnderflow(USoundWaveProcedural* Wave, int32 SamplesNeeded)
{
	if (!Wave || Wave != AmbientWave || SamplesNeeded <= 0) return;
	TArray<int16> Samples;
	Samples.SetNumZeroed(SamplesNeeded);
	FillSamples(EAHAudioCue::Ambient, Samples, AmbientTimeSeconds);
	AmbientTimeSeconds += static_cast<float>(SamplesNeeded) / SampleRate;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
}
