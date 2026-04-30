#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_SignalJam.generated.h"

/**
 * Applies Agility Down (-35% Speed for turn order) to the target for 3 turns.
 * Agility affects turn order only — not dodge/parry/crit.
 * TODO: Duration 2–4 based on minigame performance.
 * Cost: 2 AP. Ends the turn.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_SignalJam : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_SignalJam()
    {
        DisplayName = FText::FromString("Signal Jam");
        Element     = EElement::Electric;
        bEndsTurn   = true;
        Costs.Add({ EResourceType::AP, 2.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
