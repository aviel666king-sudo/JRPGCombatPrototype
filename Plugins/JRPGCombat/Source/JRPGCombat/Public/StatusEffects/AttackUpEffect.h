#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "AttackUpEffect.generated.h"

/** Increases the owner's outgoing damage by 30%. Applied by Overclock. */
UCLASS()
class JRPGCOMBAT_API UAttackUpEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UAttackUpEffect()
    {
        DisplayName   = FText::FromString("Attack Up");
        // Duration = 3 turns. Ability is authoritative — this class is NOT.
        // TODO: Future — duration will be 2-4 turns based on minigame performance.
        //       Abilities should call ApplyEffectWithDuration(OverrideDuration) where
        //       OverrideDuration comes from the minigame result (Perfect=4, Normal=3, Fail=2).
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 10;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    /** +30% outgoing damage. TODO: Make configurable if balance requires it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Effect")
    float Multiplier = 0.30f;

    virtual void OnBeforeDealDamage_Implementation(FDamagePayload& Payload) override;
};
