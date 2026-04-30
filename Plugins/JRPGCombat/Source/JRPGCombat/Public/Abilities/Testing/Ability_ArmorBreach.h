#pragma once
#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_ArmorBreach.generated.h"

/**
 * Applies Defense Down (+25% incoming damage) to the target for 3 turns.
 * TODO: Duration 2–4 based on minigame performance.
 * Cost: 2 AP. Ends the turn.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_ArmorBreach : public UCombatAbility
{
    GENERATED_BODY()
public:
    UAbility_ArmorBreach()
    {
        DisplayName = FText::FromString("Armor Breach");
        Element     = EElement::Smash;
        bEndsTurn   = true;
        Costs.Add({ EResourceType::AP, 2.f });
    }
protected:
    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
