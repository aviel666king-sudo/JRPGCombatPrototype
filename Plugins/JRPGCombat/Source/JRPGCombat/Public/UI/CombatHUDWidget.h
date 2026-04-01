#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatTypes.h"
#include "CombatHUDWidget.generated.h"

class ABattleManager;
class ACombatantBase;
class UUnitStatusWidget;
class UTurnInfoWidget;
class UCombatActionPanelWidget;
class UPanelWidget;

/**
 * UCombatHUDWidget
 *
 * Root combat HUD.  Coordinates all sub-widgets and drives all visual state.
 *
 * What changed in this version
 * ─────────────────────────────
 *  + Subscribes to OnEnemyActingChanged → orange border on acting enemy card.
 *  + Owns UCombatActionPanelWidget (bottom action panel).
 *  + Focus is re-captured after every mouse click via NativeOnMouseButtonDown,
 *    so keyboard input never gets lost.
 *  + All keyboard input for combat actions is now forwarded to the action panel
 *    (which owns the menu state machine).  The HUD itself only handles Left/Right
 *    Arrow for the free player-cursor.
 *
 * UMG SETUP  (WBP_CombatHUD)
 * ─────────────────────────────────────────────────────────────────────────────
 *  Parent class : UCombatHUDWidget
 *  Root         : Canvas Panel (full screen)
 *
 *  [UTurnInfoWidget]         Name="TurnInfoWidget"
 *    Anchors (0.5, 0.0), Alignment (0.5, 0.0), Y offset 20
 *
 *  [VerticalBox]             Name="PlayerPartyPanel"   (right-center)
 *    3 × WBP_UnitStatus (or WBP_FencerUnitStatus for slot 0)
 *
 *  [VerticalBox]             Name="EnemyPartyPanel"    (left-center)
 *    3 × WBP_EnemyUnitStatus
 *
 *  [UCombatActionPanelWidget] Name="ActionPanel"       (bottom-center)
 *    Anchors (0.5, 1.0), Alignment (0.5, 1.0), Y offset -10
 *    Size ~900 × 160
 * ─────────────────────────────────────────────────────────────────────────────
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API UCombatHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  UMG bound widgets
    // -------------------------------------------------------------------------

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTurnInfoWidget> TurnInfoWidget;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> PlayerPartyPanel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UPanelWidget> EnemyPartyPanel;

    /** Bottom action panel — main menu / skill / protocol / target info. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UCombatActionPanelWidget> ActionPanel;

    // -------------------------------------------------------------------------
    //  Runtime arrays
    // -------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Combat HUD")
    TArray<TObjectPtr<UUnitStatusWidget>> PlayerWidgets;

    UPROPERTY(BlueprintReadOnly, Category = "Combat HUD")
    TArray<TObjectPtr<UUnitStatusWidget>> EnemyWidgets;

    // -------------------------------------------------------------------------
    //  API
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Combat HUD")
    void InitializeHUD(ABattleManager* InBattleManager);

    UFUNCTION(BlueprintCallable, Category = "Combat HUD")
    void RebindUnits();

    UFUNCTION(BlueprintCallable, Category = "Combat HUD")
    void RefreshAll();

    // -------------------------------------------------------------------------
    //  BattleManager delegate callbacks
    // -------------------------------------------------------------------------

    UFUNCTION() void OnPhaseChanged(EBattlePhase NewPhase);
    UFUNCTION() void OnCombatantTurnStarted(ACombatantBase* ActiveCombatant);
    UFUNCTION() void OnActivePlayerChanged(ACombatantBase* NewActivePlayer);
    UFUNCTION() void OnTargetChanged(ACombatantBase* NewTarget);
    UFUNCTION() void OnEnemyActingChanged(ACombatantBase* NewActingEnemy);
    UFUNCTION() void OnBattleEnded(bool bVictory);

protected:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct()  override;

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry,
                                   const FKeyEvent&  InKeyEvent) override;

    /**
     * Re-capture keyboard focus whenever the player clicks anywhere on the HUD.
     * This prevents mouse interaction from permanently stealing focus away from
     * the combat keyboard controls.
     */
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                           const FPointerEvent& InMouseEvent) override;

    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:

    TWeakObjectPtr<ABattleManager> BattleManager;

    TWeakObjectPtr<UUnitStatusWidget> ActivePlayerWidget;
    TWeakObjectPtr<UUnitStatusWidget> TargetEnemyWidget;
    TWeakObjectPtr<UUnitStatusWidget> EnemyActingWidget;  // orange highlight

    void CollectUnitWidgets(UPanelWidget* Panel,
                            TArray<TObjectPtr<UUnitStatusWidget>>& OutWidgets);

    UUnitStatusWidget* FindWidgetFor(const ACombatantBase* Combatant) const;

    void ApplyActivePlayerHighlight(ACombatantBase* NewActive);
    void ApplyTargetHighlight(ACombatantBase* NewTarget);
    void ApplyEnemyActingHighlight(ACombatantBase* NewActingEnemy);
    void ClearAllHighlights();

    void RefreshTurnInfo();

    /** Reclaim Slate keyboard focus to this widget. Safe to call any time. */
    void RecaptureKeyboardFocus();
};
