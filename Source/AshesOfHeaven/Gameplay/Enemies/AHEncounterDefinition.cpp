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
	for (const FAHEncounterEnemyPoolEntry& Entry : EnemyArchetypePool)
	{
		if (Entry.ArchetypeId.IsValid()) OutEnemyIds.AddUnique(Entry.ArchetypeId);
	}
	for (const FAHEncounterPhaseDefinition& Phase : Phases)
	{
		for (const FAHEncounterSpawnSlot& Slot : Phase.FixedComposition)
		{
			if (Slot.ArchetypeId.IsValid()) OutEnemyIds.AddUnique(Slot.ArchetypeId);
		}
	}
	for (const FAHEncounterSpawnSlot& Slot : BossHeroSlots)
	{
		if (Slot.ArchetypeId.IsValid()) OutEnemyIds.AddUnique(Slot.ArchetypeId);
	}
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

const FAHEncounterDifficultyModifier& UAHEncounterDefinition::GetDifficultyModifier(EAHEncounterDifficulty DifficultyValue) const
{
	for (const FAHEncounterDifficultyModifier& Modifier : DifficultyModifiers)
	{
		if (Modifier.Difficulty == DifficultyValue) return Modifier;
	}
	static const FAHEncounterDifficultyModifier DefaultModifier;
	return DefaultModifier;
}

void UAHEncounterDefinition::BuildSpawnSequence(int32 EnemyCount, TArray<FPrimaryAssetId>& OutEnemyIds) const
{
	OutEnemyIds.Reset();
	if (EnemyCount <= 0 || !PrimaryEnemy.IsValid())
	{
		return;
	}

	TArray<FPrimaryAssetId> Roster;
	Roster.Add(PrimaryEnemy);
	for (const FPrimaryAssetId& EnemyId : AdditionalEnemies)
	{
		if (EnemyId.IsValid())
		{
			Roster.AddUnique(EnemyId);
		}
	}
	if (Roster.Num() == 1)
	{
		OutEnemyIds.Init(PrimaryEnemy, EnemyCount);
		return;
	}

	// Every listed archetype shows up at least once, the primary included - a roster of six that
	// rolls three of the same beast reads as a bug, not as variety, and a purely weighted draw can
	// leave an archetype out of a fight entirely. The remaining slots are drawn at random.
	for (int32 Index = 0; Index < Roster.Num() && OutEnemyIds.Num() < EnemyCount; ++Index)
	{
		OutEnemyIds.Add(Roster[Index]);
	}

	// Seeded from the encounter, not the clock: FAHEncounterCheckpointState restores an encounter
	// by replaying its draws, so a checkpoint reload has to rebuild the same line-up. Different
	// encounters carry different seeds, which is where the variety between fights comes from.
	FRandomStream Stream(DeterministicSeed + EnemyCount * 977);
	while (OutEnemyIds.Num() < EnemyCount)
	{
		OutEnemyIds.Add(Roster[Stream.RandRange(0, Roster.Num() - 1)]);
	}
	// Shuffle, or the guaranteed archetypes are always the first bodies through the door.
	for (int32 Index = OutEnemyIds.Num() - 1; Index > 0; --Index)
	{
		OutEnemyIds.Swap(Index, Stream.RandRange(0, Index));
	}
}

#if WITH_EDITOR
EDataValidationResult UAHEncounterDefinition::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	auto Error = [&Context, &Result](const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	};
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
	const bool bHasDirectedComposition = !EnemyArchetypePool.IsEmpty() || !Phases.IsEmpty() || !BossHeroSlots.IsEmpty();
	if (bHasDirectedComposition)
	{
		if (EnemyBudget <= 0.0f) Error(TEXT("A directed encounter requires a positive enemy budget."));
		if (Phases.IsEmpty()) Error(TEXT("A directed encounter requires at least one finite phase."));
		if (AllowedSpawnRegions.IsEmpty()) Error(TEXT("A directed encounter requires explicitly bounded spawn regions."));
		if (!SpawnQuery) Error(TEXT("A directed encounter requires an EQS spawn query."));
		for (const FAHEncounterEnemyPoolEntry& Entry : EnemyArchetypePool) ValidateEnemyId(Entry.ArchetypeId);
		for (const FAHEncounterPhaseDefinition& Phase : Phases)
		{
			for (const FAHEncounterSpawnSlot& Slot : Phase.FixedComposition) ValidateEnemyId(Slot.ArchetypeId);
		}
		for (const FAHEncounterSpawnSlot& Slot : BossHeroSlots) ValidateEnemyId(Slot.ArchetypeId);
	}
	else
	{
		ValidateEnemyId(PrimaryEnemy);
		for (const FPrimaryAssetId& EnemyId : AdditionalEnemies)
		{
			ValidateEnemyId(EnemyId);
		}
	}
	return Result == EDataValidationResult::NotValidated ? EDataValidationResult::Valid : Result;
}
#endif
