#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_SwiftStride.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_SwiftStride : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_SwiftStride()
    {
        DisplayName       = FText::FromString("Swift Stride");
        MaxCooldown       = 0;
        bEndsTurn         = true;
        bIsSupportAbility = false;  // deals 0.75x physical damage — blue strip active
        AbilityCategory   = EAbilityCategory::Skill;
        TargetScope       = ETargetScope::SingleEnemy;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        Costs.Add({ EResourceType::AP, 1.f });
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
