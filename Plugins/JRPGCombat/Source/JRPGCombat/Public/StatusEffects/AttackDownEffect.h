#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "AttackDownEffect.generated.h"

/** Reduces the owner's outgoing damage by 30%. Applied by System Disrupt. */
UCLASS()
class JRPGCOMBAT_API UAttackDownEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UAttackDownEffect()
    {
        DisplayName   = FText::FromString("Attack Down");
        // Duration = 3 turns. Ability is authoritative — this class is NOT.
        // TODO: Future — duration will be 2-4 turns based on minigame performance.
        //       Abilities should call ApplyEffectWithDuration(OverrideDuration) where
        //       OverrideDuration comes from the minigame result (Perfect=4, Normal=3, Fail=2).
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 10;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stat Effect")
    float Reduction = 0.30f;

    virtual void OnBeforeDealDamage_Implementation(FDamagePayload& Payload) override;
};
