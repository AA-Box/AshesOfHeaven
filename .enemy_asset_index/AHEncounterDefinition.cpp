#include "Gameplay/Enemies/AHEncounterDefinition.h"

#include "Engine/AssetManager.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

FPrimaryAssetId UAHEncounterDefinition::GetPrimaryAssetId() const
{
	return AHEnemyAssets::EncounterId(EncounterId.IsNone() ? GetFName() : EncounterId);
}

void UAHEncounterDefinition::GetPredictedEnemySet(TArray<FPrimaryAssetId>& OutEnemyIds) const
{
	OutEnemyIds.Reset();
	if (PrimaryEnemy.IsValid())
	{
		OutEnemyIds.Add(PrimaryEnemy);
	}
	for (const FPrimaryAssetId& EnemyId : AdditionalEnemies)
	{
		if (EnemyId.IsValid())
		{
			OutEnemyIds.AddUnique(EnemyId);
		}
	}
}

void UAHEncounterDefinition::BuildSpawnSequence(int32 EnemyCount, TArray<FPrimaryAssetId>& OutEnemyIds) const
{
	OutEnemyIds.Reset();
	if (EnemyCount <= 0 || !PrimaryEnemy.IsValid())
	{
		return;
	}

	const int32 MainCount = FMath::Max(0, EnemyCount - AdditionalEnemies.Num());
	for (int32 Index = 0; Index < MainCount; ++Index)
	{
		OutEnemyIds.Add(PrimaryEnemy);
	}
	for (int32 Index = 0; Index < AdditionalEnemies.Num() && OutEnemyIds.Num() < EnemyCount; ++Index)
	{
		OutEnemyIds.Add(AdditionalEnemies[Index].IsValid() ? AdditionalEnemies[Index] : PrimaryEnemy);
	}
	while (OutEnemyIds.Num() < EnemyCount)
	{
		OutEnemyIds.Add(PrimaryEnemy);
	}
}

#if WITH_EDITOR
EDataValidationResult UAHEncounterDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto ValidateEnemyId = [&Context, &Result](const FPrimaryAssetId& EnemyId)
	{
		if (!EnemyId.IsValid() || EnemyId.PrimaryAssetType != AHEnemyAssets::EnemyType
			|| UAssetManager::Get().GetPrimaryAssetPath(EnemyId).IsNull())
		{
			Context.AddError(FText::FromString(FString::Printf(
				TEXT("Encounter enemy Primary Asset ID does not resolve as AHEnemy: %s"), *EnemyId.ToString())));
			Result = EDataValidationResult::Invalid;
		}
	};

	if (EncounterId.IsNone())
	{
		Context.AddError(FText::FromString(TEXT("EncounterId must be set.")));
		Result = EDataValidationResult::Invalid;
	}
	ValidateEnemyId(PrimaryEnemy);
	for (const FPrimaryAssetId& EnemyId : AdditionalEnemies)
	{
		ValidateEnemyId(EnemyId);
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
