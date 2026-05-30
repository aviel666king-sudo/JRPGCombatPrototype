#pragma once

#include "CoreMinimal.h"
#include "Characters/Player/PlayerCombatant.h"
#include "CombatantFencer.generated.h"

/**
 * EFencerStance
 *
 * The four stances available to ACombatantFencer.
 * Stance is a dedicated combatant state — it is NOT a status effect.
 */
UENUM(BlueprintType)
enum class EFencerStance : uint8
{
    Stanceless UMETA(DisplayName = "Stanceless"),
    Defensive  UMETA(DisplayName = "Defensive"),
    Offensive  UMETA(DisplayName = "Offensive"),
    Virtuose   UMETA(DisplayName = "Virtuose"),
};

/**
 * ACombatantFencer
 *
 * A specific player character built on top of APlayerCombatant.
 * Adds the stance system and stance-based damage multipliers.
 * This class is character-specific — no stance logic leaks into base classes.
 *
 * Hierarchy:
 *   ACombatantBase → APlayerCombatant → ACombatantFencer
 *
 * Default stats (override in Blueprint or editor as needed):
 *   HP: 195 | Attack: 105 | Speed: 212 | Defense: 0
 *   MaxAP: 10 | StartingAP: 5 | CritChance: 5%
 *
 * Stance multipliers (applied in the damage pipeline via virtual overrides):
 *   Offensive outgoing:  ×1.5
 *   Offensive incoming:  ×1.5
 *   Defensive incoming:  ×0.5
 *   Virtuose outgoing:   ×3.0
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API ACombatantFencer : public APlayerCombatant
{
    GENERATED_BODY()

public:

    ACombatantFencer();

    // -------------------------------------------------------------------------
    //  Stance state
    // -------------------------------------------------------------------------

    // Current stance. Read-only from Blueprint — write only through SetStance().
    UPROPERTY(BlueprintReadOnly, Category = "Fencer|Stance")
    EFencerStance CurrentStance = EFencerStance::Stanceless;

    /**
     * Change stance according to the following rules:
     *
     *   - NewStance != CurrentStance  →  switch, grant +1 AP.
     *   - NewStance == CurrentStance (non-Stanceless)  →  switch to Stanceless, grant +1 AP.
     *   - NewStance == Stanceless and already Stanceless  →  no-op (no AP, no change).
     *
     * AP gain is capped at MaxAP by RestoreResource.
     * Sets bStanceChangedThisTurn = true so the turn-end reset is suppressed.
     */
    UFUNCTION(BlueprintCallable, Category = "Fencer|Stance")
    void SetStance(EFencerStance NewStance);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fencer|Stance")
    EFencerStance GetCurrentStance() const { return CurrentStance; }

    /** Combat-card subtitle = current stance name. */
    virtual FText GetCombatSubtitle() const override;

    // -------------------------------------------------------------------------
    //  Damage multiplier overrides
    // -------------------------------------------------------------------------

    // Called by ACombatantBase::ApplyDamage when this fencer is the attacker.
    virtual float GetOutgoingDamageMultiplier() const override;

    // Called by ACombatantBase::ApplyDamage when this fencer is the target.
    virtual float GetIncomingDamageMultiplier() const override;

    // -------------------------------------------------------------------------
    //  Lifecycle overrides
    // -------------------------------------------------------------------------

    // Resets stance to Stanceless and clears the change-flag at battle start.
    virtual void InitializeForBattle() override;

    // Clears bStanceChangedThisTurn so the turn-end reset logic works correctly.
    virtual void OnTurnStart_Implementation() override;

    // If no stance change happened this turn, revert to Stanceless.
    virtual void OnTurnEnd_Implementation() override;

    // Builds the fencer's branching skill tree in C++ (no editor asset needed).
    virtual void PopulateDefaultSkillTree(USkillTreeDataAsset* OutTree) const override;

    // The fencer starts a run with her two weakest (root) skills.
    virtual void GetStartingSkillNodes(TArray<FName>& Out) const override;

private:

    // Set to true whenever SetStance() makes a real change this turn.
    // Cleared at OnTurnStart. Checked at OnTurnEnd to decide whether to reset.
    bool bStanceChangedThisTurn = false;
};
