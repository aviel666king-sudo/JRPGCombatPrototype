#include "UI/Minigames/AbilityMinigameWidget.h"

void UAbilityMinigameWidget::StartMinigame_Implementation()
{
    bIsActive = true;
}

void UAbilityMinigameWidget::BroadcastResult(float RawMultiplier)
{
    bIsActive = false;

    // Support abilities cap at 1.0 — no blue-strip bonus.
    const float Final = bIsSupportAbility
        ? FMath::Min(RawMultiplier, 1.0f)
        : RawMultiplier;

    OnMinigameCompleted.Broadcast(Final);
    RemoveFromParent();
}

void UAbilityMinigameWidget::CancelMinigame()
{
    if (!bIsActive) { return; }
    bIsActive = false;

    // Multiplier = 0.0 is the cancel sentinel.
    // OnMinigameCompleted in the panel checks for 0.0 and aborts execution.
    OnMinigameCompleted.Broadcast(0.0f);
    RemoveFromParent();
}
