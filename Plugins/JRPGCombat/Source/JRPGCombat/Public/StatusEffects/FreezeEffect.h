#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "FreezeEffect.generated.h"

/** Freeze: Unable to act until hit. Removed when the owner takes any damage. */
UCLASS()
class JRPGCOMBAT_API UFreezeEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UFreezeEffect()
    {
        DisplayName   = FText::FromString("Freeze");
        BaseDuration  = 99; // Expires only when hit, not by time.
        MaxStacks     = 1;
        Priority      = 8;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    virtual bool BlocksTurnAction() const override { return true; }
    virtual void OnAfterTakeDamage_Implementation(const FDamagePayload& Payload) override;
    virtual void OnApply_Implementation() override;
};
