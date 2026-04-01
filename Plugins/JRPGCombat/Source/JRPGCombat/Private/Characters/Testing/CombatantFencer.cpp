#include "Characters/Testing/CombatantFencer.h"

ACombatantFencer::ACombatantFencer()
{
    // Default stats for this character. All values can be overridden in the
    // Blueprint subclass (BP_PlayerCombatant) via the BaseStats property.
    BaseStats.MaxHP      = 195.f;
    BaseStats.Attack     = 105.f;
    BaseStats.Speed      = 212.f;
    BaseStats.Defense    = 0.f;
    BaseStats.MaxAP      = 10.f;
    BaseStats.StartingAP = 5.f;
    BaseStats.CritChance = 0.05f;  // Stored for future crit system; not yet active.
}

// -----------------------------------------------------------------------------
//  Stance management
// -----------------------------------------------------------------------------

void ACombatantFencer::SetStance(EFencerStance NewStance)
{
    EFencerStance EffectiveStance = NewStance;

    if (NewStance == CurrentStance)
    {
        // Trying to enter the same stance we're already in.
        if (CurrentStance == EFencerStance::Stanceless)
        {
            // Already Stanceless — no action, no AP.
            return;
        }
        // Any other repeated stance → drop to Stanceless.
        EffectiveStance = EFencerStance::Stanceless;
    }

    CurrentStance          = EffectiveStance;
    bStanceChangedThisTurn = true;

    // +1 AP on every real stance change; RestoreResource clamps to MaxAP.
    RestoreResource(EResourceType::AP, 1.f);
}

// -----------------------------------------------------------------------------
//  Damage multiplier overrides
// -----------------------------------------------------------------------------

float ACombatantFencer::GetOutgoingDamageMultiplier() const
{
    switch (CurrentStance)
    {
        case EFencerStance::Offensive: return 1.5f;
        case EFencerStance::Virtuose:  return 3.0f;
        default:                       return 1.0f;
    }
}

float ACombatantFencer::GetIncomingDamageMultiplier() const
{
    switch (CurrentStance)
    {
        case EFencerStance::Offensive: return 1.5f;
        case EFencerStance::Defensive: return 0.5f;
        default:                       return 1.0f;
    }
}

// -----------------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------------

void ACombatantFencer::InitializeForBattle()
{
    Super::InitializeForBattle();
    CurrentStance          = EFencerStance::Stanceless;
    bStanceChangedThisTurn = false;
}

void ACombatantFencer::OnTurnStart_Implementation()
{
    // Clear the change-flag before status effects tick (burns etc.) so that
    // passive effects occurring at turn start do not accidentally count as
    // a player stance change.
    bStanceChangedThisTurn = false;
    Super::OnTurnStart_Implementation();
}

void ACombatantFencer::OnTurnEnd_Implementation()
{
    // If no ability changed stance this turn, return to Stanceless.
    // Direct assignment bypasses the AP grant — this is a passive reset, not
    // a player choice.
    if (!bStanceChangedThisTurn)
    {
        CurrentStance = EFencerStance::Stanceless;
    }

    Super::OnTurnEnd_Implementation();
}
