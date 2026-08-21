#include "Gameplay/Audio/AHAudioSubsystem.h"

#include "AshesOfHeaven.h"
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
	bAudioPaletteReady = true;
	UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.1][Audio] runtime palette initialized cues=14 sample_rate=%d"), SampleRate);

	if (GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimer(CleanupTimer, this, &UAHAudioSubsystem::CleanupActiveWaves, 1.0f, true);
	}
}

void UAHAudioSubsystem::Deinitialize()
{
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CleanupTimer);
	}
	ActiveWaves.Reset();
	ActiveWaveExpiryTimes.Reset();
	AmbientWave = nullptr;
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

	float AmbientDuration = 0.0f;
	AmbientWave = CreateCueWave(EAHAudioCue::Ambient, AmbientDuration);
	if (AmbientWave)
	{
		AmbientWave->OnSoundWaveProceduralUnderflow.BindUObject(this, &UAHAudioSubsystem::HandleAmbientUnderflow);
		UGameplayStatics::PlaySound2D(this, AmbientWave, 0.12f, 1.0f);
		UE_LOG(LogAshesOfHeaven, Display, TEXT("[Phase4.1][Audio] ambient bed started duration=%0.1fs"), AmbientDuration);
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
	const int32 NumSamples = OutSamples.Num();
	for (int32 Index = 0; Index < NumSamples; ++Index)
	{
		const float Time = StartTimeSeconds + static_cast<float>(Index) / SampleRate;
		const float LocalTime = FMath::Fmod(Time, Duration);
		const float Fade = Envelope(LocalTime, Duration, Cue == EAHAudioCue::Ambient ? 0.08f : 0.0025f, Cue == EAHAudioCue::Ambient ? 0.18f : 0.06f);
		float Sample = 0.0f;

		switch (Cue)
		{
		case EAHAudioCue::Shot:
			Sample = (Tone(92.0f, LocalTime) * 0.58f + Tone(780.0f, LocalTime) * 0.22f + StableNoise(LocalTime) * 0.18f) * Fade;
			break;
		case EAHAudioCue::Reload:
		{
			const float ClickA = FMath::Exp(-LocalTime * 115.0f) * Tone(1550.0f, LocalTime);
			const float ClickBTime = FMath::Max(0.0f, LocalTime - 0.115f);
			const float ClickB = FMath::Exp(-ClickBTime * 105.0f) * Tone(1120.0f, ClickBTime);
			Sample = (ClickA * 0.50f + ClickB * 0.42f + Tone(120.0f, LocalTime) * 0.16f) * Fade;
			break;
		}
		case EAHAudioCue::Empty:
			Sample = (Tone(430.0f, LocalTime) * 0.45f + StableNoise(LocalTime) * 0.12f) * Fade;
			break;
		case EAHAudioCue::Impact:
			Sample = (StableNoise(LocalTime) * 0.56f + Tone(86.0f, LocalTime) * 0.42f) * Fade;
			break;
		case EAHAudioCue::Melee:
			Sample = (StableNoise(LocalTime * 1.8f) * 0.38f + Tone(210.0f, LocalTime) * 0.20f) * Fade;
			break;
		case EAHAudioCue::Hurt:
			Sample = (Tone(148.0f, LocalTime) * 0.42f + Tone(310.0f, LocalTime) * 0.12f) * Fade;
			break;
		case EAHAudioCue::Armor:
			Sample = (Tone(980.0f, LocalTime) * 0.35f + Tone(520.0f, LocalTime) * 0.24f) * Fade;
			break;
		case EAHAudioCue::Death:
			Sample = (Tone(FMath::Lerp(260.0f, 72.0f, LocalTime / Duration), LocalTime) * 0.45f + StableNoise(LocalTime) * 0.10f) * Fade;
			break;
		case EAHAudioCue::Grenade:
			Sample = (Tone(58.0f, LocalTime) * 0.68f + StableNoise(LocalTime) * 0.42f) * Fade;
			break;
		case EAHAudioCue::Objective:
		{
			const float Frequency = LocalTime < 0.11f ? 520.0f : (LocalTime < 0.22f ? 740.0f : 1040.0f);
			Sample = Tone(Frequency, LocalTime) * Fade * 0.34f;
			break;
		}
		case EAHAudioCue::Dialogue:
			Sample = (Tone(420.0f, LocalTime) * 0.22f + Tone(630.0f, LocalTime) * 0.15f) * Fade;
			break;
		case EAHAudioCue::Pickup:
		{
			const float Frequency = FMath::Lerp(440.0f, 880.0f, LocalTime / Duration);
			Sample = Tone(Frequency, LocalTime) * Fade * 0.26f;
			break;
		}
		case EAHAudioCue::Footstep:
			Sample = (StableNoise(LocalTime * 3.4f) * 0.42f + Tone(72.0f, LocalTime) * 0.26f) * Fade;
			break;
		case EAHAudioCue::Ambient:
			Sample = (Tone(47.0f, Time) * 0.26f + Tone(93.0f, Time) * 0.11f + StableNoise(Time * 0.23f) * 0.035f) * 0.75f;
			break;
		default:
			break;
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
	if (!Wave)
	{
		return nullptr;
	}

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
	if (!bAudioPaletteReady || !GetWorld())
	{
		return;
	}

	float Duration = 0.0f;
	USoundWaveProcedural* Wave = CreateCueWave(Cue, Duration);
	if (!Wave)
	{
		return;
	}

	ActiveWaves.Add(Wave);
	ActiveWaveExpiryTimes.Add(GetWorld()->GetTimeSeconds() + Duration + 0.5f);
	UGameplayStatics::PlaySoundAtLocation(this, Wave, Location, VolumeMultiplier, PitchMultiplier);
}

void UAHAudioSubsystem::PlayUICue(EAHAudioCue Cue, float VolumeMultiplier, float PitchMultiplier)
{
	if (!bAudioPaletteReady || !GetWorld())
	{
		return;
	}

	float Duration = 0.0f;
	USoundWaveProcedural* Wave = CreateCueWave(Cue, Duration);
	if (!Wave)
	{
		return;
	}

	ActiveWaves.Add(Wave);
	ActiveWaveExpiryTimes.Add(GetWorld()->GetTimeSeconds() + Duration + 0.5f);
	UGameplayStatics::PlaySound2D(this, Wave, VolumeMultiplier, PitchMultiplier);
}

void UAHAudioSubsystem::CleanupActiveWaves()
{
	if (!GetWorld())
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();
	for (int32 Index = ActiveWaves.Num() - 1; Index >= 0; --Index)
	{
		if (!ActiveWaveExpiryTimes.IsValidIndex(Index) || ActiveWaveExpiryTimes[Index] <= Now)
		{
			ActiveWaves.RemoveAt(Index);
			if (ActiveWaveExpiryTimes.IsValidIndex(Index))
			{
				ActiveWaveExpiryTimes.RemoveAt(Index);
			}
		}
	}
}

void UAHAudioSubsystem::HandleAmbientUnderflow(USoundWaveProcedural* Wave, int32 SamplesNeeded)
{
	if (!Wave || Wave != AmbientWave || SamplesNeeded <= 0)
	{
		return;
	}

	TArray<int16> Samples;
	Samples.SetNumZeroed(SamplesNeeded);
	FillSamples(EAHAudioCue::Ambient, Samples, AmbientTimeSeconds);
	AmbientTimeSeconds += static_cast<float>(SamplesNeeded) / SampleRate;
	Wave->QueueAudio(reinterpret_cast<const uint8*>(Samples.GetData()), Samples.Num() * sizeof(int16));
}
