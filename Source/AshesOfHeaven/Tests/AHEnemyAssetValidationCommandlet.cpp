#include "Tests/AHEnemyAssetValidationCommandlet.h"

#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Engine/AssetManager.h"
#include "Gameplay/Enemies/AHEncounterDefinition.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "Misc/AssetRegistryInterface.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace AHEnemyAssetValidation
{
	bool IsDevelopmentPath(const FString& Path)
	{
		return Path.StartsWith(TEXT("/Game/Developers/"), ESearchCase::IgnoreCase)
			|| Path.StartsWith(TEXT("/Game/Tests/"), ESearchCase::IgnoreCase)
			|| Path.StartsWith(TEXT("/Game/Development/"), ESearchCase::IgnoreCase);
	}

	void AddError(FOutputDevice& Output, int32& ErrorCount, const FString& Message)
	{
		++ErrorCount;
		Output.Logf(ELogVerbosity::Error, TEXT("[EnemyAssets] %s"), *Message);
	}

	bool ValidateCookedPath(const FPrimaryAssetId& OwnerId, const FTopLevelAssetPath& AssetPath, FOutputDevice& Output, int32& ErrorCount)
	{
		const FString Path = AssetPath.ToString();
		if (IsDevelopmentPath(Path))
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s references Development-only path %s"), *OwnerId.ToString(), *Path));
			return false;
		}
		if (Path.StartsWith(TEXT("/Script/")))
		{
			return true;
		}
		FAssetData AssetData;
		if (!UAssetManager::Get().GetAssetDataForPath(FSoftObjectPath(AssetPath), AssetData) || !AssetData.IsValid())
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s bundle path does not resolve: %s"), *OwnerId.ToString(), *Path));
			return false;
		}
		if ((AssetData.PackageFlags & PKG_EditorOnly) != 0)
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s references EditorOnly package %s"), *OwnerId.ToString(), *Path));
			return false;
		}
		return true;
	}

	void ValidateBundle(
		const FPrimaryAssetId& OwnerId,
		FName BundleName,
		bool bMustContainAssets,
		FOutputDevice& Output,
		int32& ErrorCount,
		TSet<FTopLevelAssetPath>* OutPaths = nullptr)
	{
		const FAssetBundleEntry Entry = UAssetManager::Get().GetAssetBundleEntry(OwnerId, BundleName);
		if (bMustContainAssets && Entry.AssetPaths.IsEmpty())
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s has an empty required %s bundle"), *OwnerId.ToString(), *BundleName.ToString()));
		}
		for (const FTopLevelAssetPath& Path : Entry.AssetPaths)
		{
			ValidateCookedPath(OwnerId, Path, Output, ErrorCount);
			if (OutPaths) OutPaths->Add(Path);
		}
	}
}

UAHEnemyAssetValidationCommandlet::UAHEnemyAssetValidationCommandlet()
{
	// A game-engine commandlet is deliberate: CI runs the same manifest checks from the
	// packaged Development executable to prove cooked Asset Registry resolution.
	IsEditor = false;
	IsClient = false;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 UAHEnemyAssetValidationCommandlet::Main(const FString& Params)
{
	int32 ValidatedAssets = 0;
	const bool bValid = ValidateEnemyAssetManifest(*GLog, ValidatedAssets);
	UE_LOG(LogTemp, Display, TEXT("[EnemyAssets] validation=%s assets=%d cooked_data=%s"),
		bValid ? TEXT("PASS") : TEXT("FAIL"), ValidatedAssets,
		FPlatformProperties::RequiresCookedData() ? TEXT("yes") : TEXT("no"));
	return bValid ? 0 : 1;
}

bool UAHEnemyAssetValidationCommandlet::ValidateEnemyAssetManifest(FOutputDevice& Output, int32& OutValidatedAssets)
{
	using namespace AHEnemyAssetValidation;
	OutValidatedAssets = 0;
	int32 ErrorCount = 0;
	UAssetManager& AssetManager = UAssetManager::Get();
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AssetRegistry.WaitForCompletion();

	TArray<FPrimaryAssetId> EnemyIds;
	AssetManager.GetPrimaryAssetIdList(AHEnemyAssets::EnemyType, EnemyIds);
	// The whole shipped roster, not just the two the game started with. An archetype that stops
	// registering does not fail anything else loudly - the encounters that field it simply spawn
	// fewer bodies - so this list is what makes its disappearance an error.
	for (const FName RequiredName : { FName(TEXT("Pilgrim")), FName(TEXT("Hound")), FName(TEXT("Spider")), FName(TEXT("Teuthisan")) })
	{
		const FPrimaryAssetId RequiredId = AHEnemyAssets::EnemyId(RequiredName);
		if (!EnemyIds.Contains(RequiredId))
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("required Primary Asset is not registered: %s"), *RequiredId.ToString()));
		}
	}

	for (const FPrimaryAssetId& EnemyId : EnemyIds)
	{
		++OutValidatedAssets;
		const FSoftObjectPath PrimaryPath = AssetManager.GetPrimaryAssetPath(EnemyId);
		UAHEnemyDefinition* Definition = Cast<UAHEnemyDefinition>(PrimaryPath.TryLoad());
		if (!Definition || Definition->GetPrimaryAssetId() != EnemyId)
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("Primary Asset does not resolve to its declared UAHEnemyDefinition: %s"), *EnemyId.ToString()));
			continue;
		}
		const FPrimaryAssetRules Rules = AssetManager.GetPrimaryAssetRules(EnemyId);
		if (Rules.CookRule != EPrimaryAssetCookRule::AlwaysCook)
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s is not governed by AlwaysCook"), *EnemyId.ToString()));
		}

		ValidateBundle(EnemyId, AHEnemyAssets::CoreBundle, true, Output, ErrorCount);
		ValidateBundle(EnemyId, AHEnemyAssets::VisualBundle, true, Output, ErrorCount);
		ValidateBundle(EnemyId, AHEnemyAssets::AudioBundle, true, Output, ErrorCount);
		TSet<FTopLevelAssetPath> DesktopPaths;
		TSet<FTopLevelAssetPath> MobilePaths;
		ValidateBundle(EnemyId, AHEnemyAssets::DesktopBundle, true, Output, ErrorCount, &DesktopPaths);
		ValidateBundle(EnemyId, AHEnemyAssets::MobileBundle, true, Output, ErrorCount, &MobilePaths);
		ValidateBundle(EnemyId, AHEnemyAssets::DesktopAudioBundle, false, Output, ErrorCount, &DesktopPaths);
		ValidateBundle(EnemyId, AHEnemyAssets::MobileAudioBundle, false, Output, ErrorCount, &MobilePaths);
		for (const FTopLevelAssetPath& MobilePath : MobilePaths)
		{
			if (DesktopPaths.Contains(MobilePath))
			{
				AddError(Output, ErrorCount, FString::Printf(TEXT("%s puts desktop-only payload into a mobile bundle: %s"),
					*EnemyId.ToString(), *MobilePath.ToString()));
			}
		}

		TArray<FName> HardDependencies;
		UE::AssetRegistry::FDependencyQuery HardQuery;
		HardQuery.Required = UE::AssetRegistry::EDependencyProperty::Hard;
		AssetRegistry.GetDependencies(FName(*PrimaryPath.GetLongPackageName()), HardDependencies,
			UE::AssetRegistry::EDependencyCategory::Package, HardQuery);
		for (const FName Dependency : HardDependencies)
		{
			const FString Path = Dependency.ToString();
			if (Path.StartsWith(TEXT("/Game/")))
			{
				AddError(Output, ErrorCount, FString::Printf(TEXT("%s has forbidden hard game-content dependency %s"),
					*EnemyId.ToString(), *Path));
			}
		}

#if WITH_EDITOR
		FDataValidationContext ValidationContext;
		if (Definition->IsDataValid(ValidationContext) == EDataValidationResult::Invalid)
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s failed UAHEnemyDefinition data validation"), *EnemyId.ToString()));
		}
#endif
	}

	TArray<FPrimaryAssetId> EncounterIds;
	AssetManager.GetPrimaryAssetIdList(AHEnemyAssets::EncounterType, EncounterIds);
	for (const FName RequiredName : { FName(TEXT("PilgrimPatrol")), FName(TEXT("PilgrimHound")) })
	{
		const FPrimaryAssetId RequiredId = AHEnemyAssets::EncounterId(RequiredName);
		if (!EncounterIds.Contains(RequiredId))
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("required encounter Primary Asset is not registered: %s"), *RequiredId.ToString()));
		}
	}
	for (const FPrimaryAssetId& EncounterId : EncounterIds)
	{
		++OutValidatedAssets;
		const FSoftObjectPath PrimaryPath = AssetManager.GetPrimaryAssetPath(EncounterId);
		UAHEncounterDefinition* Definition = Cast<UAHEncounterDefinition>(PrimaryPath.TryLoad());
		if (!Definition || Definition->GetPrimaryAssetId() != EncounterId)
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("encounter Primary Asset does not resolve: %s"), *EncounterId.ToString()));
			continue;
		}
		if (AssetManager.GetPrimaryAssetRules(EncounterId).CookRule != EPrimaryAssetCookRule::AlwaysCook)
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s is not governed by AlwaysCook"), *EncounterId.ToString()));
		}
		TArray<FPrimaryAssetId> PredictedEnemies;
		Definition->GetPredictedEnemySet(PredictedEnemies);
		if (PredictedEnemies.IsEmpty())
		{
			AddError(Output, ErrorCount, FString::Printf(TEXT("%s predicts no enemy assets"), *EncounterId.ToString()));
		}
		for (const FPrimaryAssetId& EnemyId : PredictedEnemies)
		{
			if (!EnemyIds.Contains(EnemyId) || AssetManager.GetPrimaryAssetPath(EnemyId).IsNull())
			{
				AddError(Output, ErrorCount, FString::Printf(TEXT("%s references unresolved enemy %s"),
					*EncounterId.ToString(), *EnemyId.ToString()));
			}
		}
	}

	if (ErrorCount == 0)
	{
		Output.Logf(ELogVerbosity::Display, TEXT("[EnemyAssets] validated %d AHEnemy/AHEncounter assets"), OutValidatedAssets);
	}
	return ErrorCount == 0;
}
