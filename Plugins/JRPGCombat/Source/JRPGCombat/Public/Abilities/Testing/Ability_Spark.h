#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_Spark.generated.h"

/**
 * UAbility_Spark
 *
 * Cheap Fire jab that lays Burn stacks and switches the fencer to Defensive.
 * Used while in Offensive stance, it lays extra Burn (offensive follow-up).
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_Spark : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_Spark()
    {
        DisplayName       = FText::FromString("Spark");
        MaxCooldown       = 0;
        bEndsTurn         = true;
        bIsSupportAbility = false;
        AbilityCategory   = EAbilityCategory::Skill;
        Element           = EElement::Fire;
        TargetScope       = ETargetScope::SingleEnemy;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        Costs.Add({ EResourceType::AP, 1.f });
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
