#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AHPresentationData.generated.h"

UCLASS(BlueprintType)
class ASHESOFHEAVEN_API UAHWeaponPresentationData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FName WeaponId = FName(TEXT("M91_Revenant"));
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FVector FirstPersonOffset = FVector(34.0f, 14.0f, -16.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Weapon")
	FVector FirstPersonScale = FVector(0.78f);
};

UCLASS(BlueprintType)
class ASHESOFHEAVEN_API UAHHUDStyleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor Bone = FLinearColor(0.84f, 0.85f, 0.81f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor Amber = FLinearColor(0.94f, 0.62f, 0.22f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	FLinearColor Cyan = FLinearColor(0.42f, 0.68f, 0.71f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="HUD")
	float ObjectiveRevealSeconds = 0.35f;
};

UCLASS(BlueprintType)
class ASHESOFHEAVEN_API UAHEnvironmentStyleData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
	FName EnvironmentId = NAME_None;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
	FLinearColor PrimaryColor = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
	FLinearColor EmissiveColor = FLinearColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Environment")
	float FogDensity = 0.015f;
};
