#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_GroundSmash.generated.h"

/**
 * UAbility_GroundSmash
 *
 * Basic enemy single-target physical attack.
 * Deals 1.0x the enemy's Attack stat as Physical damage.
 *
 * No AP cost — enemies do not rely on AP as a gameplay constraint.
 * No cooldown. Ends the turn.
 *
 * Prints a debug message to screen and log when fired so it is
 * visible during testing without any UI in place.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_GroundSmash : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_GroundSmash()
    {
        DisplayName = FText::FromString("Ground Smash");
        Element     = EElement::Smash;
        MaxCooldown = 0;
        bEndsTurn   = true;
        // No AP cost — leave Costs empty.
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
