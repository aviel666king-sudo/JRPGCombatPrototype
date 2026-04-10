#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "DespairEffect.generated.h"

/** Despair: Cannot gain AP. All AP restoration is blocked. */
UCLASS()
class JRPGCOMBAT_API UDespairEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UDespairEffect()
    {
        DisplayName   = FText::FromString("Despair");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 9;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    virtual bool BlocksAPGain() const override { return true; }
    virtual void OnApply_Implementation() override;
};
