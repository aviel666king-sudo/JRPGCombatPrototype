#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "ConfuseEffect.generated.h"

/** Confuse: Attacks allied characters instead of enemies. */
UCLASS()
class JRPGCOMBAT_API UConfuseEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UConfuseEffect()
    {
        DisplayName   = FText::FromString("Confuse");
        BaseDuration  = 2;
        MaxStacks     = 1;
        Priority      = 7;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    virtual bool ConfusesTarget() const override { return true; }
    virtual void OnApply_Implementation() override;
};
