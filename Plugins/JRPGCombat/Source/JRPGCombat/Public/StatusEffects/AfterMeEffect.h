#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "AfterMeEffect.generated.h"

/** After Me: Increases the chance that enemies will target this character. */
UCLASS()
class JRPGCOMBAT_API UAfterMeEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    UAfterMeEffect()
    {
        DisplayName   = FText::FromString("After Me");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 5;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    /** Targeting weight multiplier. Default 3x means this character is 3x more likely to be targeted. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "After Me", meta = (ClampMin = "1.0"))
    float TargetingMultiplier = 3.f;

    virtual float GetTargetWeight() const override { return TargetingMultiplier; }
    virtual void OnApply_Implementation() override;
};
