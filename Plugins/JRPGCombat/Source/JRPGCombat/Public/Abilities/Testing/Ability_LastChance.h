#pragma once

#include "CoreMinimal.h"
#include "Abilities/CombatAbility.h"
#include "UI/Minigames/DiamondTimingMinigame.h"
#include "Ability_LastChance.generated.h"

UCLASS(Blueprintable)
class JRPGCOMBAT_API UAbility_LastChance : public UCombatAbility
{
    GENERATED_BODY()

public:

    UAbility_LastChance()
    {
        DisplayName       = FText::FromString("Last Chance");
        MaxCooldown       = 0;
        bEndsTurn         = true;
        // Self-targeting utility — no damage dealt.
        // bIsSupportAbility = true suppresses the blue strip on the diamond
        // and maps ActiveMultiplier to a turn-duration range (unused here,
        // but makes the minigame visually consistent with other support skills).
        bIsSupportAbility = true;
        AbilityCategory   = EAbilityCategory::Skill;
        TargetScope       = ETargetScope::Self;
        MinigameClass     = UDiamondTimingMinigame::StaticClass();
        // No AP cost — the cost is the HP sacrifice.
    }

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator,
                                        const TArray<ACombatantBase*>& Targets) override;
};
