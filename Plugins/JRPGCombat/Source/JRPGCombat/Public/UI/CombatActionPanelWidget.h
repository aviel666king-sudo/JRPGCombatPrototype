#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CombatTypes.h"
#include "UI/Minigames/AbilityMinigameWidget.h"
#include "CombatActionPanelWidget.generated.h"

class ABattleManager;
class ACombatantBase;
class UWidgetSwitcher;
class UTextBlock;
class UButton;
class UVerticalBox;
class UHorizontalBox;
class UCombatHUDWidget;         // forward declare (avoid circular include)

/**
 * UCombatActionPanelWidget
 *
 * The bottom action panel — the primary interaction zone during a player turn.
 * Displays as a persistent rectangle at the bottom of the screen.
 * Faded and non-interactive during enemy turns; active during player turns.
 *
 * INTERNAL STATE MACHINE
 * ──────────────────────
 *  Inactive        → panel faded, all input blocked
 *  MainMenu        → 5 buttons: Melee, Gun, Skill, Protocol, Skip Turn
 *  SkillMenu       → list of Skill-category abilities for acting character
 *  ProtocolMenu    → 3 protocols with charge counts
 *  SelectingTarget → panel stays visible but locked while BM is in target phase
 *
 * MENU FLOW
 * ─────────
 *  MainMenu:
 *    1 / Melee     → BeginTargetSelection(MeleeAbilityIndex)
 *    2 / Gun       → BeginTargetSelection(GunAbilityIndex)
 *    3 / Skill     → switch to SkillMenu
 *    4 / Protocol  → switch to ProtocolMenu
 *    T             → BM->RequestSkipTurn()
 *
 *  SkillMenu:
 *    1–5           → BeginTargetSelection(SkillAbilityIndex)
 *    Esc / Back    → return to MainMenu
 *
 *  ProtocolMenu:
 *    1 / Healing   → BM->BeginProtocolTargetSelection(Healing)
 *    2 / Revival   → BM->BeginProtocolTargetSelection(Revival)
 *    3 / AP        → BM->BeginProtocolTargetSelection(AP)
 *    Esc / Back    → return to MainMenu
 *
 *  SelectingTarget:
 *    A / D         → BM->NavigateTargets
 *    Space/Enter   → BM->ConfirmTargetSelection
 *    Esc           → BM->CancelTargetSelection → back to whichever menu started it
 *
 * UMG SETUP  (WBP_CombatActionPanel)
 * ─────────────────────────────────────────────────────────────────────────────
 *  Parent class : UCombatActionPanelWidget
 *  Placement    : Canvas Panel, bottom-center
 *                 Anchors (0.5, 1.0), Alignment (0.5, 1.0), Y offset -10
 *                 Size: ~900 × 160
 *
 *  Root: [Border] (dark background, full panel size)
 *    [WidgetSwitcher]  Name="MenuSwitcher"
 *      Slot 0 — Main Menu panel
 *      Slot 1 — Skill Menu panel
 *      Slot 2 — Protocol Menu panel
 *      Slot 3 — Target Selection info panel
 *
 *  ── Slot 0: Main Menu ──────────────────────────────────────────────────────
 *  [HorizontalBox]  (5 equally spaced buttons)
 *    [Button] Name="MeleeButton"    [VBox: TextBlock "Melee" + TextBlock "[1]"]
 *    [Button] Name="GunButton"      [VBox: TextBlock "Gun"   + TextBlock "[2]"]
 *    [Button] Name="SkillButton"    [VBox: TextBlock "Skill" + TextBlock "[3]"]
 *    [Button] Name="ProtocolButton" [VBox: TextBlock "Protocol" + TextBlock "[4]"]
 *    [Button] Name="SkipTurnButton" [VBox: TextBlock "Skip Turn" + TextBlock "[T]"]
 *
 *  ── Slot 1: Skill Menu ─────────────────────────────────────────────────────
 *  [HorizontalBox]
 *    [Button] Name="BackButtonSkill"   "← Back [Esc]"
 *    [VerticalBox] Name="SkillListBox"  ← populated dynamically from acting char
 *      (each skill: Button with TextBlock for name + AP cost)
 *
 *  ── Slot 2: Protocol Menu ──────────────────────────────────────────────────
 *  [HorizontalBox]
 *    [Button] Name="BackButtonProtocol"  "← Back [Esc]"
 *    [Button] Name="HealingButton"   [VBox: "Healing Protocol" + "Charges: X"]
 *    [Button] Name="RevivalButton"   [VBox: "Revival Protocol" + "Charges: X"]
 *    [Button] Name="APButton"        [VBox: "AP Protocol"      + "Charges: X"]
 *
 *  ── Slot 3: Target Selection ───────────────────────────────────────────────
 *  [HorizontalBox]
 *    [TextBlock] Name="TargetHintText"      "Select Target — A/D  Space  Backspace"
 *    [Button]    Name="BackButtonTarget"    "← Cancel"    (always visible; recovers focus after alt-tab)
 *
 *  Hotkey text color: set to a distinct color (e.g. yellow) in Blueprint.
 *  Action name color: white.
 * ─────────────────────────────────────────────────────────────────────────────
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API UCombatActionPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  UMG bound widgets
    // -------------------------------------------------------------------------

    /** Switches between Main / Skill / Protocol / TargetInfo panels. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> MenuSwitcher;

    // ── Main menu buttons ─────────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> MeleeButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> GunButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SkillButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> ProtocolButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> SkipTurnButton;

    // ── Skill submenu ─────────────────────────────────────────────────────────
    /** Back button in the skill submenu. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButtonSkill;

    /** Populated at runtime with one button per Skill-category ability.
     *  Horizontal row so a full 6-skill loadout never overflows vertically. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> SkillListBox;

    // ── Protocol submenu ──────────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButtonProtocol;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> HealingButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> RevivalButton;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> APButton;

    // ── Charge labels (updated when protocol menu opens) ──────────────────────
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> HealingChargesText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> RevivalChargesText;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> APChargesText;

    // ── Target selection info ─────────────────────────────────────────────────
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TargetHintText;

    /** Cancel button shown during target selection — gives the player a
     *  clickable escape route when keyboard focus has been lost (e.g. alt-tab). */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BackButtonTarget;

    // -------------------------------------------------------------------------
    //  API
    // -------------------------------------------------------------------------

    /**
     * Bind the panel to BattleManager and its owner HUD.
     * Called by UCombatHUDWidget::InitializeHUD().
     * OwnerHUD is stored weakly and used to restore keyboard focus
     * after a minigame overlay closes.
     */
    UFUNCTION(BlueprintCallable, Category = "Action Panel")
    void InitializePanel(ABattleManager* InBattleManager, UCombatHUDWidget* InOwnerHUD);

    /**
     * Transition to a new menu state.
     * Called by CombatHUDWidget when BattleManager phase changes,
     * and internally on Back navigation.
     */
    UFUNCTION(BlueprintCallable, Category = "Action Panel")
    void SetMenuState(ECombatMenuState NewState);

    /** Read the current panel state. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Action Panel")
    ECombatMenuState GetMenuState() const { return CurrentState; }

    /**
     * Handle a keyboard key press forwarded from CombatHUDWidget.
     * Returns true if the key was consumed, false if it should be passed along.
     */
    UFUNCTION(BlueprintCallable, Category = "Action Panel")
    bool HandleKeyDown(const FKey& Key);

    /**
     * Refresh protocol charge labels and button enabled states.
     * Call whenever a protocol is spent.
     */
    UFUNCTION(BlueprintCallable, Category = "Action Panel")
    void RefreshProtocolMenu();

    /**
     * Rebuild the dynamic skill list from the acting character's abilities.
     * Called when entering SkillMenu.
     */
    UFUNCTION(BlueprintCallable, Category = "Action Panel")
    void RefreshSkillMenu();

protected:

    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:

    TWeakObjectPtr<ABattleManager> BattleManager;

    ECombatMenuState CurrentState = ECombatMenuState::Inactive;

    // Stores the ability index for each dynamically-created skill button.
    TArray<int32> SkillButtonAbilityIndices;

    // Which menu state was active before entering SelectingTarget —
    // used to restore the correct menu on Cancel.
    ECombatMenuState PreTargetState = ECombatMenuState::MainMenu;

    // ─── Gun mode ─────────────────────────────────────────────────────────────

    /**
     * True while the player is in the persistent Gun targeting sub-mode.
     * Set when the player activates Gun. Cleared any time the panel transitions
     * to a state other than SelectingTarget (i.e. the player backs out, the turn
     * ends, or an enemy turn starts).
     *
     * While true, CombatHUDWidget::OnPhaseChanged(AwaitingInput) re-enters
     * BeginTargetSelection instead of returning to MainMenu, keeping the player
     * in the Gun targeting flow after each shot.
     */
    bool bInGunMode = false;

    /** The ability index of the Gun ability — stored when gun mode is entered. */
    int32 StoredGunAbilityIndex = -1;

public:
    /** Returns true while the player is in the persistent Gun targeting sub-mode. */
    bool IsInGunMode() const { return bInGunMode; }

    /** The Gun ability index to re-target after a shot. Only valid when IsInGunMode(). */
    int32 GetGunAbilityIndex() const { return StoredGunAbilityIndex; }

private:

    // ─── Minigame ─────────────────────────────────────────────────────────────

    /** Weak reference to the root HUD widget — used to restore keyboard focus. */
    TWeakObjectPtr<UCombatHUDWidget> OwnerHUD;

    /**
     * The currently running minigame widget.
     * Non-null only while ECombatMenuState::MinigameActive is set.
     * Kept for the fallback SPACE-key forwarding path.
     */
    UPROPERTY()
    TObjectPtr<UAbilityMinigameWidget> ActiveMinigameWidget;

    /**
     * Subscribed to BattleManager::OnSkillMinigameShouldStart.
     * Fires AFTER the player has already confirmed a target for a skill that
     * has a MinigameClass set. Creates and shows the appropriate minigame widget.
     * When the widget finishes, OnMinigameCompleted calls
     * BM->ExecutePendingSkillAfterMinigame() so BM handles the delay + execution.
     */
    UFUNCTION()
    void OnBMSkillMinigameShouldStart(int32 AbilityIndex);

    /** Called when the active minigame widget fires its completion delegate. */
    UFUNCTION()
    void OnMinigameCompleted(float OutcomeMultiplier);

    // ─── Button callbacks ─────────────────────────────────────────────────────

    UFUNCTION()
    void OnMeleeClicked();

    UFUNCTION()
    void OnGunClicked();

    UFUNCTION()
    void OnSkillMenuClicked();

    UFUNCTION()
    void OnProtocolMenuClicked();

    UFUNCTION()
    void OnSkipTurnClicked();

    UFUNCTION()
    void OnBackFromSkillClicked();

    UFUNCTION()
    void OnBackFromProtocolClicked();

    UFUNCTION()
    void OnBackFromTargetClicked();

    UFUNCTION()
    void OnHealingProtocolClicked();

    UFUNCTION()
    void OnRevivalProtocolClicked();

    UFUNCTION()
    void OnAPProtocolClicked();

    // Five UFUNCTION slot dispatchers for dynamically-created skill buttons.
    // Dynamic multicast delegates (FOnButtonClickedEvent) do not support
    // AddLambda — each slot needs its own named UFUNCTION bound via AddDynamic.
    UFUNCTION() void OnSkillSlot0Clicked() { OnSkillButtonClicked(0); }
    UFUNCTION() void OnSkillSlot1Clicked() { OnSkillButtonClicked(1); }
    UFUNCTION() void OnSkillSlot2Clicked() { OnSkillButtonClicked(2); }
    UFUNCTION() void OnSkillSlot3Clicked() { OnSkillButtonClicked(3); }
    UFUNCTION() void OnSkillSlot4Clicked() { OnSkillButtonClicked(4); }
    UFUNCTION() void OnSkillSlot5Clicked() { OnSkillButtonClicked(5); }

    // Dispatches the click for the given slot index into the skill list.
    void OnSkillButtonClicked(int32 SlotIndex);

    // ─── Helpers ──────────────────────────────────────────────────────────────

    /** Apply the correct WidgetSwitcher index for CurrentState. */
    void ApplySwitcherIndex();

    /** Builds the entire action-panel layout in C++ and assigns the bound
     *  members, so no WBP layout is needed (empty the WBP and reparent). */
    void BuildPanelLayout();

public:
    /** Enable/disable main-menu buttons based on what the acting character can do. */
    void RefreshMainMenuButtons();
private:

    /** Find the ability index tagged as Melee on the acting character. -1 if none. */
    int32 FindMeleeAbilityIndex() const;

    /** Find the ability index tagged as Gun on the acting character. -1 if none. */
    int32 FindGunAbilityIndex() const;

    /** Does the acting character have at least one Skill-category ability? */
    bool HasSkillAbilities() const;
};
