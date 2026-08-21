#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AHPresentationAuthoringLibrary.generated.h"

/** Editor-only content authoring entry points used by the checked-in Phase 4.2 generator. */
UCLASS()
class ASHESOFHEAVEN_API UAHPresentationAuthoringLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Ashes|Presentation|Authoring")
	static bool AuthorPhase42Widgets();

	UFUNCTION(BlueprintCallable, Category="Ashes|Presentation|Authoring")
	static bool AuthorPhase42Niagara();
};
