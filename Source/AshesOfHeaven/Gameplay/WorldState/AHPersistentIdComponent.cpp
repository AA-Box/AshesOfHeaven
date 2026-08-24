#include "Gameplay/WorldState/AHPersistentIdComponent.h"

#include "Gameplay/WorldState/AHSavableActor.h"
#include "Gameplay/WorldState/AHWorldStateSubsystem.h"
#include "Engine/Level.h"
#include "Engine/World.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

UAHPersistentIdComponent::UAHPersistentIdComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAHPersistentIdComponent::SetPersistentId(const FGuid& NewPersistentId)
{
	if (PersistentId == NewPersistentId)
	{
		return;
	}
	PersistentId = NewPersistentId;
	if (AActor* Owner = GetOwner(); Owner && Owner->HasActorBegunPlay())
	{
		if (UAHWorldStateSubsystem* WorldState = Owner->GetWorld()->GetSubsystem<UAHWorldStateSubsystem>())
		{
			WorldState->RegisterSavableActor(Owner);
		}
	}
}

TArray<FGuid> UAHPersistentIdComponent::FindDuplicateIds(const TArray<AActor*>& Actors)
{
	TMap<FGuid, int32> Counts;
	for (const AActor* Actor : Actors)
	{
		const UAHPersistentIdComponent* Component = Actor ? Actor->FindComponentByClass<UAHPersistentIdComponent>() : nullptr;
		if (Component && Component->PersistentId.IsValid())
		{
			Counts.FindOrAdd(Component->PersistentId)++;
		}
	}
	TArray<FGuid> Duplicates;
	for (const TPair<FGuid, int32>& Pair : Counts)
	{
		if (Pair.Value > 1)
		{
			Duplicates.Add(Pair.Key);
		}
	}
	Duplicates.Sort([](const FGuid& Left, const FGuid& Right) { return Left.ToString() < Right.ToString(); });
	return Duplicates;
}

void UAHPersistentIdComponent::OnComponentCreated()
{
	Super::OnComponentCreated();
	EnsureAuthoredId();
}

void UAHPersistentIdComponent::EnsureAuthoredId()
{
#if WITH_EDITOR
	AActor* Owner = GetOwner();
	if (!PersistentId.IsValid() && Owner && !Owner->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
		&& Owner->GetWorld() && Owner->GetWorld()->WorldType == EWorldType::Editor && !IsRunningCookCommandlet())
	{
		PersistentId = FGuid::NewGuid();
		Owner->MarkPackageDirty();
	}
#endif
}

#if WITH_EDITOR
void UAHPersistentIdComponent::PostEditImport()
{
	Super::PostEditImport();
	PersistentId = FGuid::NewGuid();
}

void UAHPersistentIdComponent::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (!bDuplicateForPIE)
	{
		PersistentId = FGuid::NewGuid();
	}
}

EDataValidationResult UAHPersistentIdComponent::IsDataValid(FDataValidationContext& Context) const
{
	const EDataValidationResult ParentResult = Super::IsDataValid(Context);
	const AActor* Owner = GetOwner();
	if (!PersistentId.IsValid())
	{
		Context.AddError(FText::FromString(TEXT("Savable actor has no persistent ID.")));
		return EDataValidationResult::Invalid;
	}
	if (Owner && !Owner->Implements<UAHSavableActor>())
	{
		Context.AddWarning(FText::FromString(TEXT("Persistent ID component owner does not implement UAHSavableActor.")));
	}
	if (Owner && Owner->GetWorld())
	{
		for (const ULevel* Level : Owner->GetWorld()->GetLevels())
		{
			if (!Level)
			{
				continue;
			}
			for (const AActor* Candidate : Level->Actors)
			{
				if (!Candidate || Candidate == Owner)
				{
					continue;
				}
				const UAHPersistentIdComponent* Other = Candidate->FindComponentByClass<UAHPersistentIdComponent>();
				if (Other && Other->PersistentId == PersistentId)
				{
					Context.AddError(FText::Format(
						FText::FromString(TEXT("Duplicate persistent ID also used by {0}.")),
						FText::FromString(Candidate->GetActorLabel())));
					return EDataValidationResult::Invalid;
				}
			}
		}
	}
	return ParentResult;
}
#endif
