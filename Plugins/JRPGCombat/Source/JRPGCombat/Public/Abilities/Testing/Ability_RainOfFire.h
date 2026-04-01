#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_RainOfFire.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_RainOfFire : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_RainOfFire()
    {
        DisplayName       = FText::FromString("Rain of Fire");
        MaxCooldown       = 0;
        bEndsTurn         = true;
        bIsSupportAbility = false;  // damage skill — blue strip active
        AbilityCategory   = EAbilityCategory::Skill;
        TargetScope       = ETargetScope::SingleEnemy;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        Costs.Add({ EResourceType::AP, 2.f });
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
