// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AHPlatformTypes.h"
#include "AHMobileControlsWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAHMobileActionDelegate, EAHMobileTouchAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAHMobileVehicleAxisDelegate, float, Steering, float, Camera);

/**
 * Native touch foundation for FPS controls. A project-specific Blueprint can add visual buttons
 * and forward them to PressAction/ReleaseAction while this class owns the two-thumb touch zones.
 */
UCLASS(Blueprintable)
class ASHESOFHEAVEN_API UAHMobileControlsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UAHMobileControlsWidget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Touch", meta=(ClampMin=0.1, ClampMax=5.0))
	float TouchSensitivity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Touch", meta=(ClampMin=0.1, ClampMax=2.0))
	float TouchControlScale = 1.0f;

	UPROPERTY(BlueprintAssignable, Category="Touch")
	FAHMobileActionDelegate OnActionPressed;

	UPROPERTY(BlueprintAssignable, Category="Touch")
	FAHMobileActionDelegate OnActionReleased;

	UPROPERTY(BlueprintAssignable, Category="Touch|Vehicle")
	FAHMobileVehicleAxisDelegate OnVehicleAxesChanged;

	UFUNCTION(BlueprintCallable, Category="Touch")
	void PressAction(EAHMobileTouchAction Action);

	UFUNCTION(BlueprintCallable, Category="Touch")
	void ReleaseAction(EAHMobileTouchAction Action);

	UFUNCTION(BlueprintCallable, Category="Touch|Vehicle")
	void SetVehicleAxes(float Steering, float Camera);

protected:
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InGestureEvent) override;

private:
	void ApplyMoveInput(const FVector2D& Value);
	void ApplyLookInput(const FVector2D& Value);
	void EndTouch(int32 PointerIndex);

	int32 MovePointerIndex = INDEX_NONE;
	int32 LookPointerIndex = INDEX_NONE;
	FVector2D MoveOrigin = FVector2D::ZeroVector;
	FVector2D LookPosition = FVector2D::ZeroVector;
};

