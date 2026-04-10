#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "SleepEffect.generated.h"

/**
 * Sleep: Unable to act for several turns. Recovers AP and heals at the beginning of each turn.
 * Broken by any incoming damage.
 */
UCLASS()
class JRPGCOMBAT_API USleepEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    USleepEffect()
    {
        DisplayName   = FText::FromString("Sleep");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 8;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    /** HP healed per turn as a fraction of MaxHP. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sleep")
    float HealPercent = 0.05f;

    /** AP recovered per turn. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sleep")
    float APRecover = 2.f;

    virtual bool BlocksTurnAction() const override { return true; }
    virtual void OnApply_Implementation() override;
    virtual void OnTurnStart_Implementation() override;
    virtual void OnAfterTakeDamage_Implementation(const FDamagePayload& Payload) override;
};
