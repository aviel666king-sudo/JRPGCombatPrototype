#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "BattleManager.generated.h"

class ACombatantBase;
class UTurnOrderManager;
class UCombatHUDWidget;
class UProtocolManagerComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseChanged,        EBattlePhase,    NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatantTurn,       ACombatantBase*, ActiveCombatant);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBattleEnded,         bool,            bVictory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTargetChanged,       ACombatantBase*, NewTarget);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActivePlayerChanged, ACombatantBase*, NewActivePlayer);

/**
 * Fired when an enemy unit begins or finishes acting.
 * The HUD subscribes to show / clear the orange "acting enemy" border.
 * NewActingEnemy = nullptr when the enemy turn ends.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyActingChanged,  ACombatantBase*, NewActingEnemy);

/**
 * Fired when target selection is confirmed for a Skill ability that has a MinigameClass.
 * AbilityIndex is the index of the skill that was selected.
 * CombatActionPanelWidget subscribes and launches the appropriate minigame widget.
 * After the minigame resolves, the panel MUST call ExecutePendingSkillAfterMinigame().
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillMinigameShouldStart, int32, AbilityIndex);

UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API ABattleManager : public AActor
{
    GENERATED_BODY()

public:

    ABattleManager();

    // -------------------------------------------------------------------------
    //  Components
    // -------------------------------------------------------------------------

    /**
     * Shared party protocol charge pool.
     * Created in the constructor; configure starting charges in the Details panel.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Battle|Protocols")
    TObjectPtr<UProtocolManagerComponent> ProtocolManager;

    // -------------------------------------------------------------------------
    //  Setup
    // -------------------------------------------------------------------------

    // -------------------------------------------------------------------------
    //  Spawn points
    //  Assign level actors (e.g. the grass pads) here.
    //  On battle start each combatant is teleported to their matching slot.
    //  Index 0 = first player/enemy, index 1 = second, etc.
    //  If an index has no spawn point the combatant stays where it is.
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Setup")
    TArray<TObjectPtr<AActor>> PlayerSpawnPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|Setup")
    TArray<TObjectPtr<AActor>> EnemySpawnPoints;

    UFUNCTION(BlueprintCallable, Category = "Battle")
    void StartBattle(const TArray<ACombatantBase*>& PlayerParty,
                     const TArray<ACombatantBase*>& EnemyParty);

    UFUNCTION(BlueprintCallable, Category = "Battle")
    void RequestPlayerAbility(int32 AbilityIndex, const TArray<ACombatantBase*>& Targets);

    UFUNCTION(BlueprintCallable, Category = "Battle")
    void RequestSkipTurn();

    // -------------------------------------------------------------------------
    //  Minigame bridge
    // -------------------------------------------------------------------------

    /**
     * Store the outcome multiplier produced by a skill minigame.
     * Phase_ExecutePlayerAction reads this once and resets it to 1.0.
     * Safe to call with 1.0 to clear a previous result.
     */
    UFUNCTION(BlueprintCallable, Category = "Battle")
    void SetPendingDamageMultiplier(float Multiplier) { PendingDamageMultiplier = Multiplier; }

    /**
     * Called by CombatActionPanelWidget when the player presses Backspace
     * during the minigame to cancel without executing the skill.
     * Clears stored targets and returns to AwaitingInput.
     */
    UFUNCTION(BlueprintCallable, Category = "Battle")
    void CancelPendingMinigameSkill();

    UFUNCTION(BlueprintCallable, Category = "Battle")
    void ExecutePendingSkillAfterMinigame(float Multiplier);

    // -------------------------------------------------------------------------
    //  Protocol execution  (called by CombatHUDWidget after charge/target confirmed)
    // -------------------------------------------------------------------------

    /**
     * Execute a protocol action.
     *
     * Validates:
     *   - Phase == AwaitingInput
     *   - ProtocolManager->CanSpend(Type)
     *
     * If valid:
     *   - Spends one charge from ProtocolManager.
     *   - Executes the appropriate protocol ability on Targets.
     *   - Advances the turn (protocols always end the turn).
     *
     * Targets must already be validated (living for Healing/AP, dead for Revival).
     * The HUD is responsible for building the correct target list after the player
     * selects their target in the Protocol targeting flow.
     */
    UFUNCTION(BlueprintCallable, Category = "Battle|Protocols")
    void ExecuteProtocol(EProtocolType Type, const TArray<ACombatantBase*>& Targets);

    // -------------------------------------------------------------------------
    //  Phase progression
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Battle|Phases")
    void Phase_Initialize();

    UFUNCTION(BlueprintCallable, Category = "Battle|Phases")
    void Phase_StartNextTurn();

    UFUNCTION(BlueprintCallable, Category = "Battle|Phases")
    void Phase_ExecutePlayerAction(int32 AbilityIndex, const TArray<ACombatantBase*>& Targets);

    UFUNCTION(BlueprintCallable, Category = "Battle|Phases")
    void Phase_ExecuteEnemyAction();

    UFUNCTION(BlueprintCallable, Category = "Battle|Phases")
    void Phase_EndTurn();

    // -------------------------------------------------------------------------
    //  Active-player cursor
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Battle|Cursor")
    void SwitchActivePlayer(int32 Direction);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Battle|Cursor")
    ACombatantBase* GetActivePlayerCombatant() const { return ActivePlayerCombatant.Get(); }

    // -------------------------------------------------------------------------
    //  Target selection  (player ability / protocol targeting)
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Battle|Targeting")
    void BeginTargetSelection(int32 AbilityIndex);

    /**
     * Begin targeting for a protocol (not an ability slot).
     * Scope is determined by Type:
     *   Healing / AP  → SingleAlly   (living allies, including acting character)
     *   Revival       → DeadAlly     (dead allies only)
     *
     * Transitions to SelectingTarget phase and fires OnTargetSelectionChanged.
     */
    UFUNCTION(BlueprintCallable, Category = "Battle|Targeting")
    void BeginProtocolTargetSelection(EProtocolType Type);

    UFUNCTION(BlueprintCallable, Category = "Battle|Targeting")
    void NavigateTargets(int32 Direction);

    UFUNCTION(BlueprintCallable, Category = "Battle|Targeting")
    void ConfirmTargetSelection();

    UFUNCTION(BlueprintCallable, Category = "Battle|Targeting")
    void CancelTargetSelection();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Battle|Targeting")
    ACombatantBase* GetCurrentTarget() const;

    UFUNCTION(BlueprintCallable, Category = "Battle|Targeting")
    TArray<ACombatantBase*> GetValidTargets(ETargetScope Scope, ACombatantBase* Actor) const;

    // -------------------------------------------------------------------------
    //  Win / loss
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Battle")
    bool CheckVictory() const;

    UFUNCTION(BlueprintCallable, Category = "Battle")
    bool CheckDefeat() const;

    // -------------------------------------------------------------------------
    //  Queries
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Battle")
    EBattlePhase GetCurrentPhase() const { return CurrentPhase; }

    UFUNCTION(BlueprintCallable, Category = "Battle")
    ACombatantBase* GetActiveCombatant() const { return ActiveCombatant; }

    UFUNCTION(BlueprintCallable, Category = "Battle")
    UTurnOrderManager* GetTurnOrderManager() const { return TurnOrderManager; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Battle")
    int32 GetTurnNumber() const { return TurnNumber; }

    UFUNCTION(BlueprintCallable, Category = "Battle")
    TArray<ACombatantBase*> GetAllCombatants() const;

    /** Which protocol this targeting session will execute (valid during SelectingTarget). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Battle|Targeting")
    EProtocolType GetPendingProtocolType() const { return PendingProtocolType; }

    /** True when the current SelectingTarget session is for a protocol (not an ability). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Battle|Targeting")
    bool IsTargetingForProtocol() const { return bTargetingProtocol; }

    // -------------------------------------------------------------------------
    //  Combat HUD
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Battle|UI")
    TSubclassOf<UCombatHUDWidget> HUDClass;

    UPROPERTY(BlueprintReadOnly, Category = "Battle|UI")
    TObjectPtr<UCombatHUDWidget> CombatHUD;

    UFUNCTION(BlueprintCallable, Category = "Battle|UI")
    UCombatHUDWidget* CreateAndShowHUD(APlayerController* PC);

    // -------------------------------------------------------------------------
    //  Observer delegates
    // -------------------------------------------------------------------------

    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnPhaseChanged OnPhaseChanged;

    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnCombatantTurn OnCombatantTurnStarted;

    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnBattleEnded OnBattleEnded;

    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnTargetChanged OnTargetSelectionChanged;

    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnActivePlayerChanged OnActivePlayerChanged;

    /** Fired when enemy begins acting (non-null) and when their turn ends (null). */
    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnEnemyActingChanged OnEnemyActingChanged;

    /**
     * Fired after the player confirms a target for a Skill that has a MinigameClass.
     * AbilityIndex identifies which skill was selected.
     * CombatActionPanelWidget subscribes here, creates the minigame widget, and runs it.
     * The panel MUST call ExecutePendingSkillAfterMinigame() when the minigame ends.
     */
    UPROPERTY(BlueprintAssignable, Category = "Battle|Events")
    FOnSkillMinigameShouldStart OnSkillMinigameShouldStart;

    UFUNCTION(BlueprintImplementableEvent, Category = "Battle|Events")
    void OnTurnOwnerChanged(bool bIsPlayerTurn);

protected:

    virtual void BeginPlay() override;

private:

    // ─── State ───────────────────────────────────────────────────────────────

    UPROPERTY()
    EBattlePhase CurrentPhase = EBattlePhase::Idle;

    UPROPERTY()
    TObjectPtr<ACombatantBase> ActiveCombatant;

    UPROPERTY()
    TObjectPtr<UTurnOrderManager> TurnOrderManager;

    UPROPERTY()
    TArray<TObjectPtr<ACombatantBase>> AllCombatants;

    UPROPERTY()
    int32 TurnNumber = 0;

    // ─── Active-player cursor ────────────────────────────────────────────────

    UPROPERTY()
    TObjectPtr<ACombatantBase> ActivePlayerCombatant;

    UPROPERTY()
    TArray<TObjectPtr<ACombatantBase>> PlayerCursorList;

    int32 PlayerCursorIndex = 0;

    // ─── Target selection ────────────────────────────────────────────────────

    int32 PendingAbilityIndex = -1;

    UPROPERTY()
    TArray<TObjectPtr<ACombatantBase>> TargetCandidates;

    int32 TargetCandidateIndex = 0;

    ETargetScope PendingScope = ETargetScope::SingleEnemy;

    /** True when the active SelectingTarget session is for a protocol, not an ability. */
    bool bTargetingProtocol = false;

    /** Which protocol is being targeted (valid only when bTargetingProtocol = true). */
    EProtocolType PendingProtocolType = EProtocolType::Healing;

    /**
     * Minigame outcome multiplier.  Written by SetPendingDamageMultiplier()
     * (called from CombatActionPanelWidget after the minigame completes).
     * Applied to UCombatAbility::ActiveMultiplier in Phase_ExecutePlayerAction,
     * then reset to 1.0f automatically.
     */
    float PendingDamageMultiplier = 1.0f;

    // ── Minigame pending execution ───────────────────────────────────────────

    /**
     * Targets confirmed by the player before the minigame started.
     * Populated in ConfirmTargetSelection when a minigame is required.
     * Consumed in ExecutePendingSkillAfterMinigame.
     */
    UPROPERTY()
    TArray<TObjectPtr<ACombatantBase>> PendingSkillTargets;

    /**
     * Ability index awaiting execution after the minigame.
     * Stored when we enter MinigameActive phase; consumed after the delay timer fires.
     */
    int32 PendingSkillAbilityIndex = -1;

    /** Fires ~0.4 s after the minigame result locks in, then calls Phase_ExecutePlayerAction. */
    FTimerHandle MinigamePostResultTimerHandle;

    // ─── Helpers ─────────────────────────────────────────────────────────────

    void SetPhase(EBattlePhase NewPhase);
    bool IsPlayerTurn() const;
    bool TryResolveBattleEnd();
    void RebuildPlayerCursorList(ACombatantBase* DefaultUnit);

    // Shared logic for entering SelectingTarget with a pre-built candidate list.
    void EnterTargetSelectionWithCandidates(ETargetScope Scope,
                                             TArray<ACombatantBase*>&& Candidates);

    FTimerHandle EnemyPreActionTimerHandle;
    FTimerHandle EnemyPostActionTimerHandle;

    static constexpr float EnemyPreActionDelay  = 0.8f;   // slightly longer for readability
    static constexpr float EnemyPostActionDelay = 1.0f;
};
