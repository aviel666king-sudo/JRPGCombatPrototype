#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "AgilityDownEffect.generated.h"

/**
 * Reduces Speed by 35% for turn order calculation.
 * Applied by Signal Jam.
 *
 * Agility currently affects ONLY turn order.
 */
UCLASS()
class JRPGCOMBAT_API UAgilityDownEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UAgilityDownEffect()
    {
        DisplayName   = FText::FromString("Agility Down");
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
    float SpeedPenalty = 0.35f;

    virtual float GetSpeedModifier() const override { return -SpeedPenalty; }
};
