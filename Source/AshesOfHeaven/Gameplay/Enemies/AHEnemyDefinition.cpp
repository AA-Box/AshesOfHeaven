#include "Gameplay/Enemies/AHEnemyDefinition.h"

#include "Engine/AssetManager.h"
#include "Misc/PackageName.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

namespace AHEnemyAssets
{
	const FPrimaryAssetType EnemyType(TEXT("AHEnemy"));
	const FPrimaryAssetType EncounterType(TEXT("AHEncounter"));
	const FName CoreBundle(TEXT("Core"));
	const FName VisualBundle(TEXT("Visual"));
	const FName AudioBundle(TEXT("Audio"));
	const FName DesktopBundle(TEXT("Desktop"));
	const FName MobileBundle(TEXT("Mobile"));
	const FName DesktopAudioBundle(TEXT("DesktopAudio"));
	const FName MobileAudioBundle(TEXT("MobileAudio"));

	FPrimaryAssetId EnemyId(FName Name)
	{
		return FPrimaryAssetId(EnemyType, Name);
	}

	FPrimaryAssetId EncounterId(FName Name)
	{
		return FPrimaryAssetId(EncounterType, Name);
	}
}

bool FAHEnemyVisualPayload::HasAnyAssetOverride() const
{
	return !SkeletalMesh.IsNull() || !AnimClass.IsNull() || !AnimationSet.IsEmpty()
		|| !PhysicsAsset.IsNull() || !Materials.IsEmpty() || bOverrideMeshTransform;
}

void FAHEnemyVisualPayload::OverlayOnto(FAHEnemyVisualPayload& Target) const
{
	if (!SkeletalMesh.IsNull()) Target.SkeletalMesh = SkeletalMesh;
	if (!AnimClass.IsNull()) Target.AnimClass = AnimClass;
	if (!AnimationSet.IsEmpty()) Target.AnimationSet = AnimationSet;
	if (!PhysicsAsset.IsNull()) Target.PhysicsAsset = PhysicsAsset;
	if (!Materials.IsEmpty()) Target.Materials = Materials;
	if (!MeshScale.Equals(FVector::OneVector)) Target.MeshScale = MeshScale;
	if (bOverrideMeshTransform)
	{
		Target.MeshOffset = MeshOffset;
		Target.MeshRotation = MeshRotation;
		Target.bOverrideMeshTransform = true;
	}
}

bool FAHEnemyAudioPayload::HasAnyAssetOverride() const
{
	return !VoicePalette.IsNull() || !VoiceBanks.IsEmpty() || !HurtSound.IsNull()
		|| !ArmorDamageSound.IsNull() || !DeathSound.IsNull();
}

void FAHEnemyAudioPayload::OverlayOnto(FAHEnemyAudioPayload& Target) const
{
	if (!VoicePalette.IsNull()) Target.VoicePalette = VoicePalette;
	if (!VoiceBanks.IsEmpty()) Target.VoiceBanks = VoiceBanks;
	if (!HurtSound.IsNull()) Target.HurtSound = HurtSound;
	if (!ArmorDamageSound.IsNull()) Target.ArmorDamageSound = ArmorDamageSound;
	if (!DeathSound.IsNull()) Target.DeathSound = DeathSound;
}

bool FAHEnemyVFXPayload::HasAnyAssetOverride() const
{
	return !SpawnEffect.IsNull() || !AmbientEffect.IsNull() || !HitEffect.IsNull() || !DeathEffect.IsNull();
}

void FAHEnemyVFXPayload::OverlayOnto(FAHEnemyVFXPayload& Target) const
{
	if (!SpawnEffect.IsNull()) Target.SpawnEffect = SpawnEffect;
	if (!AmbientEffect.IsNull()) Target.AmbientEffect = AmbientEffect;
	if (!HitEffect.IsNull()) Target.HitEffect = HitEffect;
	if (!DeathEffect.IsNull()) Target.DeathEffect = DeathEffect;
}

FPrimaryAssetId UAHEnemyDefinition::GetPrimaryAssetId() const
{
	return AHEnemyAssets::EnemyId(EnemyId.IsNone() ? GetFName() : EnemyId);
}

FAHEnemyVisualPayload UAHEnemyDefinition::ResolveVisuals(bool bMobile) const
{
	FAHEnemyVisualPayload Result = Visuals;
	(bMobile ? MobileVisuals : DesktopVisuals).OverlayOnto(Result);
	return Result;
}

FAHEnemyAudioPayload UAHEnemyDefinition::ResolveAudio(bool bMobile) const
{
	FAHEnemyAudioPayload Result = Audio;
	(bMobile ? MobileAudio : DesktopAudio).OverlayOnto(Result);
	return Result;
}

FAHEnemyVFXPayload UAHEnemyDefinition::ResolveVFX(bool bMobile) const
{
	FAHEnemyVFXPayload Result = VFX;
	(bMobile ? MobileVFX : DesktopVFX).OverlayOnto(Result);
	return Result;
}

#if WITH_EDITOR
EDataValidationResult UAHEnemyDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Error = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};

	if (EnemyId.IsNone()) Error(TEXT("EnemyId must be set so the Primary Asset ID survives asset renames."));
	if (CombatClass.IsNull()) Error(TEXT("CombatClass is required in the Core bundle."));
	if (Visuals.SkeletalMesh.IsNull()) Error(TEXT("A base skeletal mesh is required in the Visual bundle."));
	// Creature archetypes ship their own skeleton, so there is no mannequin AnimBP to point at and
	// AAHCombatantCharacter falls back to looping the first clip in AnimationSet. A body with
	// neither still renders, so this is a warning about a bind-pose enemy, not a broken asset.
	if (Visuals.AnimClass.IsNull() && Visuals.AnimationSet.IsEmpty())
	{
		Context.AddWarning(FText::FromString(FString::Printf(
			TEXT("%s has no animation class and no animation set; it will spawn in its bind pose."),
			*EnemyId.ToString())));
	}
	if (Visuals.PhysicsAsset.IsNull()) Error(TEXT("A physics asset is required for hit zones and ragdolls."));
	// A melee archetype is weaponless on purpose - that is what bMeleeOnly declares.
	if (!AISettings.bMeleeOnly && Loadout.WeaponClasses.IsEmpty())
	{
		Error(TEXT("At least one soft weapon class is required in the Core bundle unless AISettings.bMeleeOnly is set."));
	}

	TArray<FSoftObjectPath> SoftReferences;
	UAssetManager::Get().ExtractSoftObjectPaths(GetClass(), this, SoftReferences);
	for (const FSoftObjectPath& Path : SoftReferences)
	{
		if (Path.IsNull())
		{
			continue;
		}
		const FString PackageName = Path.GetLongPackageName();
		if (PackageName.StartsWith(TEXT("/Game/Developers/")) || PackageName.StartsWith(TEXT("/Game/Tests/"))
			|| PackageName.StartsWith(TEXT("/Engine/Developer/")))
		{
			Error(FString::Printf(TEXT("Development-only asset is referenced by a shipping enemy definition: %s"), *Path.ToString()));
		}
		if (!PackageName.StartsWith(TEXT("/Script/")) && !FPackageName::DoesPackageExist(PackageName))
		{
			Error(FString::Printf(TEXT("Soft reference does not resolve to a package: %s"), *Path.ToString()));
		}
	}

	for (const FPrimaryAssetId& LootId : Loot.LootDefinitions)
	{
		if (!LootId.IsValid() || UAssetManager::Get().GetPrimaryAssetPath(LootId).IsNull())
		{
			Error(FString::Printf(TEXT("Loot Primary Asset ID does not resolve: %s"), *LootId.ToString()));
		}
	}

	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
