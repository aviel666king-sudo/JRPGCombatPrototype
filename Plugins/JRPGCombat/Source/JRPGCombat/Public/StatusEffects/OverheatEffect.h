#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "OverheatEffect.generated.h"

/** Overheat: Cannot use skill-category abilities for a limited number of turns. */
UCLASS()
class JRPGCOMBAT_API UOverheatEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UOverheatEffect()
    {
        DisplayName   = FText::FromString("Overheat");
        BaseDuration  = 2;
        MaxStacks     = 1;
        Priority      = 7;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    virtual bool BlocksAbilityUse() const override { return true; }
    virtual void OnApply_Implementation() override;
};
