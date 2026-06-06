#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "MenuSlotButton.generated.h"

/**
 * UMenuSlotButton
 *
 * A UButton that carries a slot index + an action code so one handler can route
 * clicks from the main menu and the checkpoint save/manage menu.
 */
UCLASS()
class JRPGCOMBATTESTING_API UMenuSlotButton : public UButton
{
    GENERATED_BODY()

public:
    int32 SlotIndex = -1;
    int32 Action    = 0;

    TFunction<void(UMenuSlotButton*)> OnAction;

    UFUNCTION()
    void HandleClicked() { if (OnAction) { OnAction(this); } }
};
