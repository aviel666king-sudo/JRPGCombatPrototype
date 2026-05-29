#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_Combustion.generated.h"

/**
 * UAbility_Combustion
 *
 * Fire detonation that consumes every Burn stack on the target for bonus
 * damage (the more Burn, the bigger the blast), then switches to Offensive.
 * Pairs with Spark / Rain of Fire which stack the Burn beforehand.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_Combustion : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_Combustion()
    {
        DisplayName       = FText::FromString("Combustion");
        MaxCooldown       = 0;
        bEndsTurn         = true;
        bIsSupportAbility = false;
        AbilityCategory   = EAbilityCategory::Skill;
        Element           = EElement::Fire;
        TargetScope       = ETargetScope::SingleEnemy;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        Costs.Add({ EResourceType::AP, 2.f });
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
