#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_OffensiveSwitch.generated.h"

/**
 * UAbility_OffensiveSwitch
 *
 * Physical strike that applies Fragile (target takes +15% damage) and switches
 * the fencer into Offensive stance. The fencer's stance-entry opener.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_OffensiveSwitch : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_OffensiveSwitch()
    {
        DisplayName       = FText::FromString("Offensive Switch");
        MaxCooldown       = 0;
        bEndsTurn         = true;
        bIsSupportAbility = false;
        AbilityCategory   = EAbilityCategory::Skill;
        Element           = EElement::None;
        TargetScope       = ETargetScope::SingleEnemy;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        Costs.Add({ EResourceType::AP, 1.f });
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
