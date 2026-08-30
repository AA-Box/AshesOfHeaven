#include "Gameplay/Animation/AHCreatureAnimInstance.h"

#include "Animation/AnimSequenceBase.h"

namespace
{
	constexpr int32 ToIndex(EAHCreatureAnimState State)
	{
		return static_cast<int32>(State);
	}
}

void FAHCreatureAnimInstanceProxy::Initialize(UAnimInstance* Instance)
{
	FAnimInstanceProxy::Initialize(Instance);

	BlendNode.ResetPoses();
	BlendNode.bAdditiveNode = false;
	BlendNode.bNormalizeAlpha = true;
	for (int32 Index = 0; Index < StateCount; ++Index)
	{
		BlendNode.AddPose();
		BlendNode.Poses[Index].SetLinkNode(&Players[Index]);
		Players[Index].SetLoopAnimation(Index <= ToIndex(EAHCreatureAnimState::Run));
		BlendNode.DesiredAlphas[Index] = Weights[Index];
	}
	BlendNode.Initialize_AnyThread(FAnimationInitializeContext(this));
}

void FAHCreatureAnimInstanceProxy::Configure(const FAHCreatureAnimationSet& Set)
{
	UAnimSequenceBase* Clips[StateCount] {
		Set.Idle.Get(), Set.Walk.Get(), Set.Run.Get(), Set.Attack.Get(), Set.Death.Get()
	};
	for (int32 Index = 0; Index < StateCount; ++Index)
	{
		Players[Index].SetSequence(Clips[Index]);
		Players[Index].SetLoopAnimation(Index <= ToIndex(EAHCreatureAnimState::Run));
	}
	BlendSeconds = FMath::Clamp(Set.TransitionBlendSeconds, 0.04f, 0.5f);
	RequestedState = EAHCreatureAnimState::Idle;
	RequestedPlayRate = 1.0f;
	bRestartRequested = true;
}

void FAHCreatureAnimInstanceProxy::SetState(EAHCreatureAnimState State, float PlayRate)
{
	const int32 StateIndex = ToIndex(State);
	if (StateIndex < 0 || StateIndex >= StateCount || !Players[StateIndex].GetSequence())
	{
		return;
	}
	bRestartRequested |= State != RequestedState || State == EAHCreatureAnimState::Attack
		|| State == EAHCreatureAnimState::Death;
	RequestedState = State;
	RequestedPlayRate = FMath::Clamp(PlayRate, 0.1f, 3.0f);
}

void FAHCreatureAnimInstanceProxy::UpdateAnimationNode(const FAnimationUpdateContext& Context)
{
	UpdateCounter.Increment();
	const int32 RequestedIndex = ToIndex(RequestedState);
	if (bRestartRequested)
	{
		Players[RequestedIndex].Initialize_AnyThread(FAnimationInitializeContext(this));
		Players[RequestedIndex].CacheBones_AnyThread(FAnimationCacheBonesContext(this));
		bRestartRequested = false;
	}

	Players[RequestedIndex].SetPlayRate(RequestedPlayRate);
	const float TransitionSeconds = RequestedState == EAHCreatureAnimState::Attack
		? FMath::Min(BlendSeconds, 0.09f) : BlendSeconds;
	const float WeightSpeed = 1.0f / FMath::Max(0.01f, TransitionSeconds);
	for (int32 Index = 0; Index < StateCount; ++Index)
	{
		const float Target = Index == RequestedIndex ? 1.0f : 0.0f;
		Weights[Index] = FMath::FInterpConstantTo(
			Weights[Index], Target, Context.GetDeltaTime(), WeightSpeed);
		BlendNode.DesiredAlphas[Index] = Weights[Index];
	}
	BlendNode.Update_AnyThread(Context);
}

bool FAHCreatureAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	BlendNode.Evaluate_AnyThread(Output);
	return true;
}

void FAHCreatureAnimInstanceProxy::AddReferencedObjects(
	UAnimInstance* Instance, FReferenceCollector& Collector)
{
	FAnimInstanceProxy::AddReferencedObjects(Instance, Collector);
	for (FAnimNode_SequencePlayer_Standalone& Player : Players)
	{
		Collector.AddPropertyReferencesWithStructARO(
			FAnimNode_SequencePlayer_Standalone::StaticStruct(), &Player);
	}
}

UAHCreatureAnimInstance::UAHCreatureAnimInstance()
{
	// The character drives this tiny graph on the game thread; avoiding a worker hand-off also
	// makes attack restarts deterministic when a melee timer and an animation update share a frame.
	bUseMultiThreadedAnimationUpdate = false;
}

void UAHCreatureAnimInstance::Configure(const FAHCreatureAnimationSet& Set)
{
	LoadedClips = {Set.Idle.Get(), Set.Walk.Get(), Set.Run.Get(), Set.Attack.Get(), Set.Death.Get()};
	GetProxyOnGameThread<FAHCreatureAnimInstanceProxy>().Configure(Set);
}

void UAHCreatureAnimInstance::SetCreatureState(EAHCreatureAnimState State, float PlayRate)
{
	GetProxyOnGameThread<FAHCreatureAnimInstanceProxy>().SetState(State, PlayRate);
}

FAnimInstanceProxy* UAHCreatureAnimInstance::CreateAnimInstanceProxy()
{
	return new FAHCreatureAnimInstanceProxy(this);
}
