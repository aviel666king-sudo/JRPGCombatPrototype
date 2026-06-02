#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DefeatScreenWidget.generated.h"

class UTextBlock;

/**
 * UDefeatScreenWidget
 *
 * Shown by AJrpgGameMode when a battle is lost. Two choices:
 *   - Retry    → restart the same fight with the exact entry conditions
 *   - Give Up  → travel to the last rested checkpoint, fully healed
 *
 * Pure-C++ UMG (no WBP needed). The game mode wires OnRetryRequested /
 * OnGiveUpRequested before AddToViewport and feeds GiveUpLabel for the button.
 */
UCLASS()
class JRPGCOMBATTESTING_API UDefeatScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    TFunction<void()> OnRetryRequested;
    TFunction<void()> OnGiveUpRequested;

    /** Sub-label under the Give Up button (e.g. "→ Last Checkpoint"). */
    FText GiveUpLabel;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UFUNCTION()
    void HandleRetry();

    UFUNCTION()
    void HandleGiveUp();
};
