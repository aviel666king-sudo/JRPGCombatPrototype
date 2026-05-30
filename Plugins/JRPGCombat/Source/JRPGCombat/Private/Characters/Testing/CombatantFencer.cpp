#include "Characters/Testing/CombatantFencer.h"
#include "Progression/SkillTreeDataAsset.h"
#include "Abilities/Testing/Ability_OffensiveSwitch.h"
#include "Abilities/Testing/Ability_Spark.h"
#include "Abilities/Testing/Ability_Combustion.h"
#include "Abilities/Testing/Ability_GuardDown.h"
#include "Abilities/Testing/Ability_FencersFlurry.h"
#include "Abilities/Testing/Ability_RainOfFire.h"
#include "Abilities/Testing/Ability_Percee.h"

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

// -----------------------------------------------------------------------------
//  Skill tree (C++-defined — no editor data asset required)
// -----------------------------------------------------------------------------

void ACombatantFencer::PopulateDefaultSkillTree(USkillTreeDataAsset* OutTree) const
{
    if (!OutTree) { return; }

    auto AddNode = [OutTree](FName Id, const FString& Name, const FString& Desc,
                             TSubclassOf<UCombatAbility> Ability, int32 Cost,
                             const TArray<FName>& Prereqs, int32 Col, int32 Row)
    {
        FSkillNode Node;
        Node.NodeId        = Id;
        Node.DisplayName   = FText::FromString(Name);
        Node.Description   = FText::FromString(Desc);
        Node.AbilityClass  = Ability;
        Node.SkillCoinCost = Cost;
        Node.Prerequisites = Prereqs;
        Node.GridPos       = FIntPoint(Col, Row);
        OutTree->Nodes.Add(Node);
    };

    // Column 0 — roots (no prerequisites).
    AddNode("OffensiveSwitch", TEXT("Offensive Switch"),
            TEXT("Strike + Fragile, enter Offensive stance."),
            UAbility_OffensiveSwitch::StaticClass(), 1, {}, 0, 0);
    AddNode("Spark", TEXT("Spark"),
            TEXT("Fire jab that lays Burn, enter Defensive stance."),
            UAbility_Spark::StaticClass(), 1, {}, 0, 2);

    // Column 1 — branch off the roots.
    AddNode("FencersFlurry", TEXT("Fencer's Flurry"),
            TEXT("Physical hit that applies Fragile."),
            UAbility_FencersFlurry::StaticClass(), 2, { "OffensiveSwitch" }, 1, 0);
    AddNode("GuardDown", TEXT("Guard Down"),
            TEXT("Apply Fragile to ALL enemies."),
            UAbility_GuardDown::StaticClass(), 2, { "OffensiveSwitch" }, 1, 1);
    AddNode("RainOfFire", TEXT("Rain of Fire"),
            TEXT("Two Fire hits stacking Burn (more in Defensive)."),
            UAbility_RainOfFire::StaticClass(), 2, { "Spark" }, 1, 2);

    // Column 2 — capstones.
    AddNode("Percee", TEXT("Percee"),
            TEXT("Strong strike, bonus vs Fragile targets."),
            UAbility_Percee::StaticClass(), 2, { "FencersFlurry" }, 2, 0);
    AddNode("Combustion", TEXT("Combustion"),
            TEXT("Detonate all Burn on the target for big Fire damage."),
            UAbility_Combustion::StaticClass(), 3, { "RainOfFire" }, 2, 2);
}

FText ACombatantFencer::GetCombatSubtitle() const
{
    switch (CurrentStance)
    {
        case EFencerStance::Offensive: return FText::FromString(TEXT("Offensive"));
        case EFencerStance::Defensive: return FText::FromString(TEXT("Defensive"));
        case EFencerStance::Virtuose:  return FText::FromString(TEXT("Virtuose"));
        default:                       return FText::FromString(TEXT("Stanceless"));
    }
}

void ACombatantFencer::GetStartingSkillNodes(TArray<FName>& Out) const
{
    // The two root skills — her weakest options — come pre-unlocked.
    Out.Add("Spark");
    Out.Add("OffensiveSwitch");
}
