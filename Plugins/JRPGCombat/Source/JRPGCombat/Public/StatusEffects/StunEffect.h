#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "StunEffect.generated.h"

/** Stun: Skips the character's turn. Can stack on players and regular enemies (bosses capped). */
UCLASS()
class JRPGCOMBAT_API UStunEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UStunEffect()
    {
        DisplayName   = FText::FromString("Stun");
        BaseDuration  = 1;
        MaxStacks     = 5;
        Priority      = 8;
        StackBehavior = EEffectStackBehavior::StackAndRefresh;
    }

    virtual bool BlocksTurnAction() const override { return true; }
    virtual void OnApply_Implementation() override;
    virtual void OnTurnStart_Implementation() override;
};
