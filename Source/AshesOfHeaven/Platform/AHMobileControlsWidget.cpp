// Copyright Epic Games, Inc. All Rights Reserved.

#include "Platform/AHMobileControlsWidget.h"
#include "AshesOfHeavenCharacter.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

UAHMobileControlsWidget::UAHMobileControlsWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void UAHMobileControlsWidget::PressAction(EAHMobileTouchAction Action)
{
	OnActionPressed.Broadcast(Action);
}

void UAHMobileControlsWidget::ReleaseAction(EAHMobileTouchAction Action)
{
	OnActionReleased.Broadcast(Action);
}

void UAHMobileControlsWidget::SetVehicleAxes(float Steering, float Camera)
{
	OnVehicleAxesChanged.Broadcast(Steering, Camera);
}

FReply UAHMobileControlsWidget::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const int32 PointerIndex = InGestureEvent.GetPointerIndex();
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());
	const FVector2D LocalSize = InGeometry.GetLocalSize();
	if (TryHandleActionTouch(LocalPosition, LocalSize, PointerIndex))
	{
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (LocalPosition.X < LocalSize.X * 0.48f && MovePointerIndex == INDEX_NONE)
	{
		MovePointerIndex = PointerIndex;
		MoveOrigin = LocalPosition;
		ApplyMoveInput(FVector2D::ZeroVector);
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	if (LookPointerIndex == INDEX_NONE)
	{
		LookPointerIndex = PointerIndex;
		LookPosition = LocalPosition;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}

	return FReply::Unhandled();
}

FReply UAHMobileControlsWidget::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	const int32 PointerIndex = InGestureEvent.GetPointerIndex();
	const FVector2D LocalPosition = InGeometry.AbsoluteToLocal(InGestureEvent.GetScreenSpacePosition());

	if (PointerIndex == MovePointerIndex)
	{
		const FVector2D Radius(120.0f * TouchControlScale, 120.0f * TouchControlScale);
		ApplyMoveInput(FVector2D(
			FMath::Clamp((LocalPosition.X - MoveOrigin.X) / Radius.X, -1.0f, 1.0f),
			FMath::Clamp((LocalPosition.Y - MoveOrigin.Y) / Radius.Y, -1.0f, 1.0f)));
		return FReply::Handled();
	}

	if (PointerIndex == LookPointerIndex)
	{
		const FVector2D Delta = LocalPosition - LookPosition;
		LookPosition = LocalPosition;
		ApplyLookInput(Delta * TouchSensitivity);
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply UAHMobileControlsWidget::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent)
{
	EndTouch(InGestureEvent.GetPointerIndex());
	return FReply::Handled().ReleaseMouseCapture();
}

void UAHMobileControlsWidget::EndTouch(int32 PointerIndex)
{
	if (const EAHMobileTouchAction* Action = ActionPointers.Find(PointerIndex))
	{
		ReleaseAction(*Action);
		ActionPointers.Remove(PointerIndex);
		return;
	}
	if (PointerIndex == MovePointerIndex)
	{
		MovePointerIndex = INDEX_NONE;
		ApplyMoveInput(FVector2D::ZeroVector);
	}
	if (PointerIndex == LookPointerIndex)
	{
		LookPointerIndex = INDEX_NONE;
	}
}

bool UAHMobileControlsWidget::TryHandleActionTouch(const FVector2D& LocalPosition, const FVector2D& LocalSize, int32 PointerIndex)
{
	if (LocalSize.X <= 1.0f || LocalSize.Y <= 1.0f)
	{
		return false;
	}

	EAHMobileTouchAction Action;
	bool bAction = false;
	const float X = LocalPosition.X / LocalSize.X;
	const float Y = LocalPosition.Y / LocalSize.Y;
	if (X > 0.72f)
	{
		if (Y < 0.34f) { Action = EAHMobileTouchAction::ADS; bAction = true; }
		else if (Y < 0.52f) { Action = EAHMobileTouchAction::Fire; bAction = true; }
		else if (Y < 0.68f) { Action = EAHMobileTouchAction::Reload; bAction = true; }
		else if (Y < 0.84f) { Action = EAHMobileTouchAction::Grenade; bAction = true; }
		else { Action = EAHMobileTouchAction::Melee; bAction = true; }
	}
	else if (Y > 0.80f && X > 0.28f)
	{
		if (X < 0.43f) { Action = EAHMobileTouchAction::Interact; bAction = true; }
		else if (X < 0.58f) { Action = EAHMobileTouchAction::Jump; bAction = true; }
		else if (X < 0.72f) { Action = EAHMobileTouchAction::Sprint; bAction = true; }
		else { Action = EAHMobileTouchAction::Crouch; bAction = true; }
	}

	if (bAction)
	{
		ActionPointers.Add(PointerIndex, Action);
		PressAction(Action);
	}
	return bAction;
}

void UAHMobileControlsWidget::ApplyMoveInput(const FVector2D& Value)
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (AAshesOfHeavenCharacter* Character = Cast<AAshesOfHeavenCharacter>(PlayerController->GetPawn()))
		{
			Character->ApplyTouchMoveInput(Value.X, Value.Y);
		}
	}
}

void UAHMobileControlsWidget::ApplyLookInput(const FVector2D& Value)
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (AAshesOfHeavenCharacter* Character = Cast<AAshesOfHeavenCharacter>(PlayerController->GetPawn()))
		{
			Character->ApplyTouchLookInput(Value.X, Value.Y);
		}
	}
}
