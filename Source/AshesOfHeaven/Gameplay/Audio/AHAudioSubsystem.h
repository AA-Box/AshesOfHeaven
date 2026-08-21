#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "AHAudioSubsystem.generated.h"

class UAHAudioPaletteData;
class USoundBase;
class UAudioComponent;
enum class EAHChapterStage : uint8;

UENUM()
enum class EAHAudioCue : uint8
{
	Shot,
	Reload,
	Empty,
	Impact,
	Melee,
	Hurt,
	Armor,
	Death,
	Grenade,
	Objective,
	Dialogue,
	Pickup,
	Footstep,
	Ambient
};

/**
 * Runtime audio palette used until authored sound design assets replace the greybox cues.
 * Keeping the palette in the game module means packaged builds never silently lose feedback
 * when a Blueprint sound property has not been assigned.
 */
UCLASS()
class ASHESOFHEAVEN_API UAHAudioSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	void PlayWorldCue(EAHAudioCue Cue, const FVector& Location, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);
	void PlayUICue(EAHAudioCue Cue, float VolumeMultiplier = 1.0f, float PitchMultiplier = 1.0f);

	bool IsAudioPaletteReady() const { return bAudioPaletteReady; }
	bool HasAuthoredCue(EAHAudioCue Cue) const;

private:
	FName GetSemanticEventName(EAHAudioCue Cue) const;
	USoundBase* ResolveAuthoredCue(EAHAudioCue Cue);
	USoundBase* ResolveAuthoredEnvironment(FName EnvironmentId);
	UFUNCTION()
	void HandleChapterStageChanged(EAHChapterStage Stage);
	FName GetEnvironmentForStage(EAHChapterStage Stage) const;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ActiveEnvironmentComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAHAudioPaletteData> AudioPalette;

	bool bAudioPaletteReady = false;
};
