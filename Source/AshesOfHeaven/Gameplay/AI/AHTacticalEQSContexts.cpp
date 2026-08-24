#include "Gameplay/AI/AHTacticalEQSContexts.h"

#include "EnvironmentQuery/EnvQueryTypes.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Actor.h"
#include "EnvironmentQuery/Items/EnvQueryItemType_Point.h"
#include "Gameplay/AI/AHTacticalPositionSubsystem.h"

namespace
{
	const FAHTacticalPositionRequest* FindRequest(const FEnvQueryInstance& QueryInstance)
	{
		const UObject* Owner = QueryInstance.Owner.Get();
		const UWorld* World = QueryInstance.World;
		const UAHTacticalPositionSubsystem* Tactical = World ? World->GetSubsystem<UAHTacticalPositionSubsystem>() : nullptr;
		return Tactical ? Tactical->FindRequest(Owner) : nullptr;
	}
}

void UAH_EQSContext_CombatTarget::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (const FAHTacticalPositionRequest* Request = FindRequest(QueryInstance))
	{
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Request->CombatTarget.Get());
	}
}

void UAH_EQSContext_Squadmates::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (const FAHTacticalPositionRequest* Request = FindRequest(QueryInstance))
	{
		TArray<AActor*> Squadmates;
		Squadmates.Reserve(Request->Squadmates.Num());
		for (const TWeakObjectPtr<AActor>& Squadmate : Request->Squadmates)
		{
			if (Squadmate.IsValid())
			{
				Squadmates.Add(Squadmate.Get());
			}
		}
		UEnvQueryItemType_Actor::SetContextHelper(ContextData, Squadmates);
	}
}

void UAH_EQSContext_LastKnownTarget::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (const FAHTacticalPositionRequest* Request = FindRequest(QueryInstance))
	{
		UEnvQueryItemType_Point::SetContextHelper(ContextData, Request->LastKnownTarget);
	}
}

void UAH_EQSContext_GrenadeThreat::ProvideContext(FEnvQueryInstance& QueryInstance, FEnvQueryContextData& ContextData) const
{
	if (const FAHTacticalPositionRequest* Request = FindRequest(QueryInstance))
	{
		UEnvQueryItemType_Point::SetContextHelper(ContextData, Request->GrenadeThreat);
	}
}
