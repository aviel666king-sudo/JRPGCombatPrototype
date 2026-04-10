#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "BarrierEffect.generated.h"

/** Barrier: Negates one instance of damage, then expires. */
UCLASS()
class JRPGCOMBAT_API UBarrierEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UBarrierEffect()
    {
        DisplayName   = FText::FromString("Barrier");
        BaseDuration  = 99; // Expires when hit, not by time.
        MaxStacks     = 99; // No practical cap — stack as many as needed.
        Priority      = 15; // High priority — runs before other damage hooks.
        StackBehavior = EEffectStackBehavior::StackAndRefresh;
    }

    virtual void OnBeforeTakeDamage_Implementation(FDamagePayload& Payload) override;
    virtual void OnApply_Implementation() override;
};
