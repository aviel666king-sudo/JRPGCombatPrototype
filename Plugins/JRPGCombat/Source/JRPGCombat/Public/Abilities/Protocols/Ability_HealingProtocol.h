// ─── Ability_HealingProtocol.h ───────────────────────────────────────────────
#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_HealingProtocol.generated.h"

/**
 * Healing Protocol
 * Restores 40% of the target's MaxHP.
 * Target scope: SingleAlly (living ally — the acting character can also target themselves).
 * Ends the turn.  Charges deducted by BattleManager before Execute is called.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_HealingProtocol : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_HealingProtocol()
    {
        DisplayName     = FText::FromString(TEXT("Healing Protocol"));
        bEndsTurn       = true;
        AbilityCategory = EAbilityCategory::None; // surfaced through Protocol menu, not ability slots
        TargetScope     = ETargetScope::SingleAlly;
        // No AP cost — protocols use charges, not AP.
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
