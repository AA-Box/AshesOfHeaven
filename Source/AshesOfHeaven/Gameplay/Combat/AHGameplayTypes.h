#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AHGameplayTypes.generated.h"

UENUM(BlueprintType)
enum class EAHFaction : uint8
{
	Player,
	Human,
	Veil,
	Neutral
};

UENUM(BlueprintType)
enum class EAHHitZone : uint8
{
	Body,
	Head,
	WeakPoint
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHAmmoState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	int32 Magazine = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	int32 Reserve = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	int32 MagazineCapacity = 36;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ammo")
	int32 ReserveCapacity = 180;
};

USTRUCT(BlueprintType)
struct ASHESOFHEAVEN_API FAHCombatCheckpointState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	bool bValid = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FName CheckpointId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FString MapName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FVector PlayerLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FRotator PlayerRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	float Health = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	float Armor = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	FAHAmmoState Ammo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 Grenades = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	int32 ObjectiveIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Checkpoint")
	TArray<FName> CompletedEncounters;
};

UCLASS()
class ASHESOFHEAVEN_API UAHCombatRulesLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Combat")
	static bool IsHostile(EAHFaction Source, EAHFaction Target);

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Combat")
	static float ApplyArmorAbsorption(float IncomingDamage, float CurrentArmor, float& ArmorDamage);

	UFUNCTION(BlueprintPure, Category="Ashes of Heaven|Combat")
	static int32 CalculateReloadTransfer(const FAHAmmoState& Ammo);
};
