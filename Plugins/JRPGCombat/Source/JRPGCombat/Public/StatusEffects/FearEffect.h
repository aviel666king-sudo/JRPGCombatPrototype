#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "FearEffect.generated.h"

/**
 * Fear: Cannot parry (handled by parry system when implemented).
 * Has a 40% chance to ignore commands (skips turn involuntarily).
 */
UCLASS()
class JRPGCOMBAT_API UFearEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UFearEffect()
    {
        DisplayName   = FText::FromString("Fear");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 7;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    /** Probability (0-1) of ignoring commands each turn. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fear")
    float CommandIgnoreChance = 0.4f;

    virtual bool BlocksTurnAction() const override;
    virtual void OnApply_Implementation() override;
};
