#pragma once
#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "RegenEffect.generated.h"

/** Regen: Heals a percentage of MaxHP at the beginning of each turn. */
UCLASS()
class JRPGCOMBAT_API URegenEffect : public UStatusEffect
{
    GENERATED_BODY()
public:
    URegenEffect()
    {
        DisplayName   = FText::FromString("Regen");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 6;
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    /** HP healed per turn as a fraction of MaxHP. Default: 5%. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Regen",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HealPercent = 0.05f;

    virtual void OnTurnStart_Implementation() override;
    virtual void OnApply_Implementation() override;
};
