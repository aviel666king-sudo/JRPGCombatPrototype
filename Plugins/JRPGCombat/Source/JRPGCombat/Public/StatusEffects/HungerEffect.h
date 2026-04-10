#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "HungerEffect.generated.h"

/** Hunger: Cannot be healed. Blocks all incoming healing. */
UCLASS()
class JRPGCOMBAT_API UHungerEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UHungerEffect()
    {
        DisplayName   = FText::FromString("Hunger");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 9;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    virtual bool BlocksHealing() const override { return true; }
    virtual void OnApply_Implementation() override;
};
