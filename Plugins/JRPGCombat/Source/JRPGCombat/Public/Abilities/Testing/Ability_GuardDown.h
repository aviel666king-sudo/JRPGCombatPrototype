#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "Ability_GuardDown.generated.h"

/**
 * UAbility_GuardDown
 *
 * Support skill: applies Fragile (+15% damage taken) to ALL enemies and
 * switches the fencer to Offensive. No minigame — instant party-wide setup.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_GuardDown : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_GuardDown()
    {
        DisplayName       = FText::FromString("Guard Down");
        MaxCooldown       = 1;
        bEndsTurn         = true;
        bIsSupportAbility = true;
        AbilityCategory   = EAbilityCategory::Skill;
        Element           = EElement::None;
        TargetScope       = ETargetScope::AllEnemies;
        MinigameClass     = nullptr;
        Costs.Add({ EResourceType::AP, 2.f });
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
