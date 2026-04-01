#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "AgilityUpEffect.generated.h"

/**
 * Increases Speed by 35% for turn order calculation.
 * Applied by Acceleration Boost.
 *
 * Agility currently affects ONLY turn order.
 * Do NOT connect to dodge, parry, crit, or reaction window yet — those systems
 * are not implemented. When they are, extend GetSpeedModifier() or add new hooks.
 */
UCLASS()
class JRPGCOMBAT_API UAgilityUpEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UAgilityUpEffect()
    {
        DisplayName   = FText::FromString("Agility Up");
        // Duration = 3 turns. Ability is authoritative — this class is NOT.
        // TODO: Future — duration will be 2-4 turns based on minigame performance.
        //       Abilities should call ApplyEffectWithDuration(OverrideDuration) where
        //       OverrideDuration comes from the minigame result (Perfect=4, Normal=3, Fail=2).
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 5;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Effect")
    float SpeedBonus = 0.35f;

    virtual float GetSpeedModifier() const override { return SpeedBonus; }
};
