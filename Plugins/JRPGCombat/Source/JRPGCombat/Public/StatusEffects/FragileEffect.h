#pragma once

#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "FragileEffect.generated.h"

/**
 * UFragileEffect
 *
 * Debuff that makes the target take 15% more damage from all sources.
 * Applied by Fencer's Flurry. Works via OnBeforeTakeDamage which runs before
 * the defense calculation, so the bonus applies to every hit while active.
 *
 * Percée reads this effect to apply an additional Percée-specific damage bonus
 * so that its total effective advantage against a Fragile target is ~+35%.
 */
UCLASS()
class JRPGCOMBAT_API UFragileEffect : public UStatusEffect
{
    GENERATED_BODY()

public:

    UFragileEffect()
    {
        DisplayName   = FText::FromString("Fragile");
        BaseDuration  = 3;
        MaxStacks     = 1;
        Priority      = 10;   // Run before other damage hooks.
        StackBehavior = EEffectStackBehavior::RefreshDuration;
    }

    virtual void OnBeforeTakeDamage_Implementation(FDamagePayload& Payload) override;

    /**
     * Returns the number of turns remaining on this Fragile debuff.
     * Mirrors BurnEffect::TicksRemaining for consistent Blueprint UI patterns.
     * Use this from UMG after casting StatusEffect to UFragileEffect.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Fragile")
    int32 GetTurnsRemaining() const { return Duration; }
};
