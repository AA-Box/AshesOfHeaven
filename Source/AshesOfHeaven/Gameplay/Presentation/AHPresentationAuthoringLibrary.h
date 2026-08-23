#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AHPresentationAuthoringLibrary.generated.h"

/** Editor-only content authoring entry points used by the checked-in Phase 4.3 generator. */
UCLASS()
class ASHESOFHEAVEN_API UAHPresentationAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Ashes|Presentation|Authoring")
	static bool AuthorPhase42Widgets();

	UFUNCTION(BlueprintCallable, Category="Ashes|Presentation|Authoring")
	static bool AuthorPhase42Niagara();

	/**
	 * Authors the near-camera Erebus effects (NS_Erebus_FireSmall / FireWreck /
	 * EmbersNear / SmokeLocal) with deliberate sprite sizes, spawn rates, lifetimes
	 * and velocities instead of the factory fountain defaults. The factory defaults
	 * render as giant additive columns at any near scale; these are the authored
	 * replacements the visual gate requires.
	 */
	UFUNCTION(BlueprintCallable, Category="Ashes|Presentation|Authoring")
	static bool AuthorErebusNearVFX();
};
