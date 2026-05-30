#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TurnInfoWidget.generated.h"

class ACombatantBase;
class UTextBlock;

/**
 * UTurnInfoWidget
 *
 * Top-center HUD panel.  Shows:
 *   - Current turn number   ("Turn 1", "Turn 2" …)
 *     NOTE: BattleManager's TurnNumber is 0-based; we display TurnNumber + 1.
 *   - Next actor in queue   ("Next: Player Fencer" / "Next: Enemy Archer")
 *     Shown regardless of whose turn it currently is.
 *
 * ─────────────────────────────────────────────────────────────
 *  UMG SETUP  (WBP_TurnInfo)
 * ─────────────────────────────────────────────────────────────
 *  Parent class : UTurnInfoWidget
 *  Root widget  : VerticalBox   (or Overlay with centred column)
 *
 *  Child 1 — TextBlock
 *    Name      : "TurnNumberText"
 *    Example   : "Turn 1"
 *    Alignment : centre-centre
 *    Font      : bold, ~28 pt
 *
 *  Child 2 — TextBlock
 *    Name      : "NextActorText"
 *    Example   : "Next: Player Fencer"
 *    Alignment : centre-centre
 *    Font      : regular, ~20 pt
 *
 *  Placement in WBP_CombatHUD:
 *    Anchors  : (0.5, 0.0)  — top-center
 *    Alignment: (0.5, 0.0)
 *    Offset Y : 20 px from top
 * ─────────────────────────────────────────────────────────────
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API UTurnInfoWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  UMG bound widgets (both optional — missing widgets are silently skipped)
    // -------------------------------------------------------------------------

    /** Displays "Turn N"  (N = InternalTurnNumber + 1). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TurnNumberText;

    /** Displays "Next: <Team> <Name>". */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> NextActorText;

    // -------------------------------------------------------------------------
    //  API
    // -------------------------------------------------------------------------

    /**
     * Repaint both labels.
     *
     * @param InternalTurnNumber   Raw value from BattleManager::GetTurnNumber() (0-based).
     *                             Shown as InternalTurnNumber + 1.
     * @param NextCombatant        First living unit in the turn queue, or nullptr.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Info")
    void Refresh(int32 InternalTurnNumber, ACombatantBase* NextCombatant);

protected:

    virtual TSharedRef<SWidget> RebuildWidget() override;

private:

    /** Builds the two-line layout in C++ (no WBP needed). */
    void BuildLayout();
};
