#pragma once

#include "CoreMinimal.h"
#include "StatusEffects/StatusEffect.h"
#include "BurnEffect.generated.h"

/**
 * UBurnEffect
 *
 * Damage-over-time effect. Stackable — each stack represents one remaining tick.
 *
 * Rules:
 *   - Triggers ONLY at turn start (not turn end).
 *   - Deals 3% of Owner's MaxHP as Magical damage per tick.
 *   - Each tick consumes 1 stack. Effect expires when StackCount reaches 0.
 *   - Can kill.
 *   - Stacks represent remaining ticks. Applying 5 stacks = 5 turn-start triggers.
 *
 * Stacking:
 *   StackBehavior = StackAndRefresh. Each ApplyEffect call adds 1 stack.
 *   The ability applying Burn decides how many stacks to add.
 *
 * Future scaling hooks (do not implement yet):
 *   - Damage percentage: currently 3% MaxHP. Prepare for Chip/Stat modifiers
 *     (e.g. a Chip that makes Burn deal 5%). Route through DamagePayload so
 *     type advantage and damage modifiers apply normally.
 *   - Stack application count: ability-driven, already works.
 */
UCLASS()
class JRPGCOMBAT_API UBurnEffect : public UStatusEffect
{
    GENERATED_BODY()

public:

    UBurnEffect()
    {
        DisplayName         = FText::FromString("Burn");
        // Stack-driven expiry: StackCount controls lifetime, not Duration.
        // Duration is only set to 0 internally when StackCount hits 0 (see OnTurnStart).
        // bStackDrivenDuration tells the manager to skip auto-decrementing Duration each turn.
        BaseDuration        = 1;
        bStackDrivenDuration = true;
        MaxStacks           = 99;
        Priority            = 5;
        StackBehavior       = EEffectStackBehavior::StackAndRefresh;
    }

    /**
     * Burn damage as a fraction of Owner's MaxHP.
     * Default: 0.03 = 3% per tick.
     * TODO: Allow Chips/Stats to modify this (pass through a future damage context).
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Burn",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DamagePercent = 0.03f;

    // Lifecycle
    virtual void OnApply_Implementation() override;
    virtual void OnTurnStart_Implementation() override;
    virtual void OnExpire_Implementation() override;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Burn")
    int32 GetTicksRemaining() const { return StackCount; }
};
