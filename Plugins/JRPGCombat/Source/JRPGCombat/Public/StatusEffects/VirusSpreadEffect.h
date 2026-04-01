#pragma once

#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "VirusSpreadEffect.generated.h"

/**
 * UVirusSpreadEffect
 *
 * Damage-over-time effect that can spread to other combatants.
 *
 * Rules (per tick at turn start):
 *   1. Deals 6% of Owner's MaxHP as Magical damage.
 *   2. 15% chance to spread 1 stack to another living combatant (random fallback
 *      if no adjacency system exists — replace with proper adjacency when added).
 *   3. 20% chance to NOT lose a stack this tick (stack retention).
 *   4. Otherwise removes 1 stack. Expires when StackCount reaches 0.
 *
 * Stacking:
 *   StackBehavior = StackAndRefresh. Each ApplyEffect call adds 1 stack.
 *   The applying ability decides the initial stack count.
 *
 * Future hooks (do not implement yet):
 *   - Adjacency system for directed spread
 *   - Chip/Stat modifiers to DamagePercent, SpreadChance, RetentionChance
 */
UCLASS()
class JRPGCOMBAT_API UVirusSpreadEffect : public UStatusEffect
{
    GENERATED_BODY()

public:

    UVirusSpreadEffect()
    {
        DisplayName          = FText::FromString("Virus Spread");
        // Stack-driven expiry: StackCount controls lifetime, not Duration.
        // Duration is only set to 0 internally when StackCount hits 0 (see OnTurnStart).
        // bStackDrivenDuration tells the manager to skip auto-decrementing Duration each turn.
        BaseDuration         = 1;
        bStackDrivenDuration = true;
        MaxStacks            = 99;
        Priority             = 4;   // Runs after Burn (Priority 5), before default hooks.
        StackBehavior        = EEffectStackBehavior::StackAndRefresh;
    }

    /** Damage per tick as a fraction of MaxHP. Default: 6%. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Virus Spread",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DamagePercent = 0.06f;

    /** Probability (0–1) of spreading 1 stack to another unit each tick. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Virus Spread",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SpreadChance = 0.15f;

    /** Probability (0–1) of NOT losing a stack this tick. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Virus Spread",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RetentionChance = 0.20f;

    virtual void OnApply_Implementation() override;
    virtual void OnTurnStart_Implementation() override;
    virtual void OnExpire_Implementation() override;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Virus Spread")
    int32 GetTicksRemaining() const { return StackCount; }
};
