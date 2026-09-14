#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "AnimNodes/AnimNode_MultiWayBlend.h"
#include "Gameplay/Enemies/AHEnemyDefinition.h"
#include "AHCreatureAnimInstance.generated.h"

class UAnimSequenceBase;

/** Lightweight native graph for imported creatures whose skeletons cannot share a mannequin ABP. */
USTRUCT()
struct ASHESOFHEAVEN_API FAHCreatureAnimInstanceProxy : public FAnimInstanceProxy
{
	GENERATED_BODY()

	FAHCreatureAnimInstanceProxy() = default;
	explicit FAHCreatureAnimInstanceProxy(UAnimInstance* Instance)
		: FAnimInstanceProxy(Instance)
	{
	}

	virtual void Initialize(UAnimInstance* Instance) override;
	virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override;
	virtual bool Evaluate(FPoseContext& Output) override;
	virtual void AddReferencedObjects(UAnimInstance* Instance, FReferenceCollector& Collector) override;

	void Configure(const FAHCreatureAnimationSet& Set);
	void SetState(EAHCreatureAnimState State, float PlayRate);

private:
	static constexpr int32 StateCount = 5;
	FAnimNode_MultiWayBlend BlendNode;
	FAnimNode_SequencePlayer_Standalone Players[StateCount];
	float Weights[StateCount] {1.0f, 0.0f, 0.0f, 0.0f, 0.0f};
	EAHCreatureAnimState RequestedState = EAHCreatureAnimState::Idle;
	float RequestedPlayRate = 1.0f;
	float BlendSeconds = 0.16f;
	bool bRestartRequested = false;
};

/** Runtime owner for the native creature graph. Animation assets remain data-driven per archetype. */
UCLASS(Transient, NotBlueprintable)
class ASHESOFHEAVEN_API UAHCreatureAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	UAHCreatureAnimInstance();

	void Configure(const FAHCreatureAnimationSet& Set);
	void SetCreatureState(EAHCreatureAnimState State, float PlayRate = 1.0f);

protected:
	virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;

private:
	/** Keeps the soft-reference results alive while the proxy evaluates them. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UAnimSequenceBase>> LoadedClips;
};
