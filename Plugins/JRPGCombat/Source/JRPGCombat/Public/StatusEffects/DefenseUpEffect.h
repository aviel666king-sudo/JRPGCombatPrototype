#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "DefenseUpEffect.generated.h"

/** Reduces incoming damage by 20%. Applied by Fortify Protocol. */
UCLASS()
class JRPGCOMBAT_API UDefenseUpEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UDefenseUpEffect()
    {
        DisplayName   = FText::FromString("Defense Up");
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
    float Reduction = 0.20f;

    virtual void OnBeforeTakeDamage_Implementation(FDamagePayload& Payload) override;
};
