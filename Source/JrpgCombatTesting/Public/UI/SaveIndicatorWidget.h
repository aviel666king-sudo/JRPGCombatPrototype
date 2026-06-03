#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SaveIndicatorWidget.generated.h"

/**
 * USaveIndicatorWidget
 *
 * Tiny bottom-left "Autosaving…" toast. Added to the viewport on an open-world
 * autosave and removes itself after a couple of seconds. Pure C++ UMG.
 */
UCLASS()
class JRPGCOMBATTESTING_API USaveIndicatorWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Text to show (defaults to "Autosaving…"). Set before AddToViewport. */
    FText Message;

    /** Seconds on screen before it removes itself. */
    float Lifetime = 2.0f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    FTimerHandle DismissTimer;
    void Dismiss();
};
