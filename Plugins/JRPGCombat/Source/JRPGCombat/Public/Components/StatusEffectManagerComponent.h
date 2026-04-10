#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "StatusEffectManagerComponent.generated.h"

class UStatusEffect;
class ACombatantBase;

UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class JRPGCOMBAT_API UStatusEffectManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UStatusEffectManagerComponent();

    // --- Apply / Remove ---

    /**
     * Apply an effect using the default BaseDuration defined in the effect class.
     * Re-application behavior is controlled by UStatusEffect::StackBehavior.
     */
    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void ApplyEffect(TSubclassOf<UStatusEffect> EffectClass, ACombatantBase* Source);

    /**
     * Apply an effect with an explicit duration override.
     * Used for future minigame-based scaling (Normal=3, Perfect=4, Fail=2).
     * TODO: Wire to minigame performance result when that system is implemented.
     */
    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void ApplyEffectWithDuration(TSubclassOf<UStatusEffect> EffectClass,
                                  ACombatantBase* Source,
                                  int32 OverrideDuration);

    /** Remove all instances of the given class. OnExpire is NOT called. */
    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void RemoveEffect(TSubclassOf<UStatusEffect> EffectClass);

    /** Remove every active effect immediately. Called by CombatantBase::InitializeForBattle. */
    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void ClearAllEffects();

    // --- Turn hooks ---

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyTurnStart();

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyTurnEnd();

    // --- Damage hooks ---

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyBeforeDealDamage(UPARAM(ref) FDamagePayload& Payload);

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyBeforeTakeDamage(UPARAM(ref) FDamagePayload& Payload);

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyAfterTakeDamage(const FDamagePayload& Payload);

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyDealDamage(const FDamagePayload& Payload);

    // --- Healing hooks ---

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyBeforeDealHealing(UPARAM(ref) float& Amount);

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyAfterDealHealing(float FinalAmount);

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyBeforeReceiveHealing(UPARAM(ref) float& Amount);

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    void NotifyAfterReceiveHealing(float FinalAmount);

    // --- Queries ---

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    bool HasEffect(TSubclassOf<UStatusEffect> EffectClass) const;

    UFUNCTION(BlueprintCallable, Category = "Status Effects")
    const TArray<UStatusEffect*>& GetActiveEffects() const { return ActiveEffects; }

    /**
     * Returns the combined speed multiplier from all active effects.
     * Each effect contributes via GetSpeedModifier() (0 = no change).
     * Used by CombatantBase::GetEffectiveSpeed() for turn order calculation.
     */
    float GetSpeedMultiplier() const;

    /** True if any active effect blocks the owner from acting this turn. */
    bool ShouldSkipTurn() const;

    /** True if any active effect confuses the owner (attacks allies). */
    bool ShouldConfuseAttack() const;

    /** True if any active effect blocks healing. */
    bool CanReceiveHealing() const;

    /** True if any active effect blocks AP gain. */
    bool CanGainAP() const;

    /** True if all active effects allow ability use (no Overheat). */
    bool CanUseAbilities() const;

    /** True if any active effect grants an extra turn (Berserk). Consumes the extra turn grant. */
    bool ConsumeExtraTurn();

    /** Returns the combined enemy targeting weight multiplier from all active effects. */
    float GetEnemyTargetWeight() const;

private:

    UPROPERTY()
    TArray<TObjectPtr<UStatusEffect>> ActiveEffects;

    UStatusEffect* FindEffect(TSubclassOf<UStatusEffect> EffectClass) const;
    void PurgeExpired();

    /** Returns a priority-sorted (descending) snapshot copy of ActiveEffects for safe iteration. */
    TArray<TObjectPtr<UStatusEffect>> GetSortedCopy() const;
};
