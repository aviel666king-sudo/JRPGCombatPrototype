#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AbilityMinigameWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMinigameCompleted, float, OutcomeMultiplier);

/**
 * UAbilityMinigameWidget — abstract base for all skill minigame overlays.
 *
 * LIFETIME
 *  1. Panel creates via CreateWidget.
 *  2. Panel calls AddToPlayerScreen + SetUserFocus.
 *  3. Panel calls StartMinigame().
 *  4. Subclass calls BroadcastResult(multiplier)  OR  CancelMinigame().
 *     Both fire OnMinigameCompleted and call RemoveFromParent().
 *  5. Panel receives the delegate and continues combat flow.
 */
UCLASS(Abstract, Blueprintable, BlueprintType)
class JRPGCOMBAT_API UAbilityMinigameWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /** Fires when the minigame resolves (confirm OR cancel). */
    UPROPERTY(BlueprintAssignable, Category = "Minigame")
    FOnMinigameCompleted OnMinigameCompleted;

    /** Set by the panel from Ability->bIsSupportAbility before StartMinigame(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigame")
    bool bIsSupportAbility = false;

    // ── API ──────────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Minigame")
    void StartMinigame();
    virtual void StartMinigame_Implementation();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Minigame")
    bool IsMinigameActive() const { return bIsActive; }

    /**
     * Fallback SPACE path — panel forwards confirm here when focus may have slipped.
     * Primary path: NativeOnKeyDown on the widget itself.
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Minigame")
    void OnConfirmPressed();
    virtual void OnConfirmPressed_Implementation() {}

    /**
     * Cancel the minigame without executing the skill.
     * Broadcasts multiplier = 0.0 (sentinel) and removes the widget.
     * The panel receives this via OnMinigameCompleted and aborts execution.
     */
    UFUNCTION(BlueprintCallable, Category = "Minigame")
    void CancelMinigame();

protected:

    bool bIsActive = false;

    /** Confirm path — clamps for support, fires delegate, removes widget. */
    void BroadcastResult(float RawMultiplier);
};
