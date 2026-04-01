#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_SystemDisrupt.generated.h"

/**
 * Applies Attack Down (-30% outgoing damage) to the target for 3 turns.
 * TODO: Duration 2–4 based on minigame performance.
 * Cost: 2 AP. Ends the turn.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_SystemDisrupt : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_SystemDisrupt()
    {
        DisplayName = FText::FromString("System Disrupt");
        bEndsTurn   = true;
        Costs.Add({ EResourceType::AP, 2.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
