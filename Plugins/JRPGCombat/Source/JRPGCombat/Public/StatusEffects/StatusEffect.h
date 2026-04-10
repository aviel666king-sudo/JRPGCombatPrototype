#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CombatTypes.h"
#include "StatusEffect.generated.h"

class ACombatantBase;

UENUM(BlueprintType)
enum class EEffectStackBehavior : uint8
{
    RefreshDuration       UMETA(DisplayName = "Refresh Duration"),
    StackAndRefresh       UMETA(DisplayName = "Stack And Refresh"),
    IgnoreIfAlreadyActive UMETA(DisplayName = "Ignore If Already Active"),
    Replace               UMETA(DisplayName = "Replace"),
};

UCLASS(Blueprintable, BlueprintType, EditInlineNew, Abstract)
class JRPGCOMBAT_API UStatusEffect : public UObject
{
    GENERATED_BODY()

public:

    // --- Config ---

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect", meta = (ClampMin = "1"))
    int32 BaseDuration = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect", meta = (ClampMin = "1"))
    int32 MaxStacks = 1;

    /** Higher priority effects run first within the same notify pass. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect")
    int32 Priority = 0;

    /** Controls what happens when this effect is applied while already active. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect")
    EEffectStackBehavior StackBehavior = EEffectStackBehavior::RefreshDuration;

    /**
     * If true, this effect's lifetime is controlled entirely by StackCount, not Duration.
     * Duration will NOT be auto-decremented each turn end by StatusEffectManagerComponent.
     * The effect expires only when StackCount reaches 0 and the effect sets Duration = 0.
     *
     * Use this for DoT effects like Burn and VirusSpread where each stack = one remaining tick.
     * Do NOT use for standard buffs/debuffs that expire after N turns.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Status Effect")
    bool bStackDrivenDuration = false;

    // --- Runtime state ---

    /** The combatant carrying this effect. */
    UPROPERTY(BlueprintReadOnly, Category = "Status Effect")
    TObjectPtr<ACombatantBase> Owner;

    /** The combatant who applied this effect. May be null for environmental effects. */
    UPROPERTY(BlueprintReadOnly, Category = "Status Effect")
    TObjectPtr<ACombatantBase> Source;

    UPROPERTY(BlueprintReadOnly, Category = "Status Effect")
    int32 Duration = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Status Effect")
    int32 StackCount = 1;

    // --- Lifecycle callbacks ---

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnApply();
    virtual void OnApply_Implementation();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnTurnStart();
    virtual void OnTurnStart_Implementation();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnTurnEnd();
    virtual void OnTurnEnd_Implementation();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnExpire();
    virtual void OnExpire_Implementation();

    /**
     * Called when an effect is re-applied while already active, under
     * RefreshDuration stack behavior. Override to reset runtime counters
     * (e.g. BurnEffect resets TicksRemaining here).
     */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnReapply();
    virtual void OnReapply_Implementation();

    // --- Damage hooks ---

    /** Called on the attacker before they deal damage. Modify Payload.BaseDamage to alter outgoing damage. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnBeforeDealDamage(UPARAM(ref) FDamagePayload& Payload);
    virtual void OnBeforeDealDamage_Implementation(FDamagePayload& Payload);

    /** Called on the target before incoming damage is resolved. Modify Payload.BaseDamage to alter damage received. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnBeforeTakeDamage(UPARAM(ref) FDamagePayload& Payload);
    virtual void OnBeforeTakeDamage_Implementation(FDamagePayload& Payload);

    /** Called on the target after incoming damage is resolved. Payload.ResolvedDamage is set. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnAfterTakeDamage(const FDamagePayload& Payload);
    virtual void OnAfterTakeDamage_Implementation(const FDamagePayload& Payload);

    /** Called on the attacker after they deal damage. Payload.ResolvedDamage is set. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnDealDamage(const FDamagePayload& Payload);
    virtual void OnDealDamage_Implementation(const FDamagePayload& Payload);

    // --- Healing hooks ---

    /** Called on the healer before healing is applied. Modify Amount to alter outgoing healing. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnBeforeDealHealing(UPARAM(ref) float& Amount);
    virtual void OnBeforeDealHealing_Implementation(float& Amount);

    /** Called on the healer after healing is committed. FinalAmount is the value actually restored. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnAfterDealHealing(float FinalAmount);
    virtual void OnAfterDealHealing_Implementation(float FinalAmount);

    /** Called on the target before healing is applied. Modify Amount to alter incoming healing. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnBeforeReceiveHealing(UPARAM(ref) float& Amount);
    virtual void OnBeforeReceiveHealing_Implementation(float& Amount);

    /** Called on the target after healing is committed. FinalAmount is the value actually restored. */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Status Effect")
    void OnAfterReceiveHealing(float FinalAmount);
    virtual void OnAfterReceiveHealing_Implementation(float FinalAmount);

    // --- Helpers ---

    UFUNCTION(BlueprintCallable, Category = "Status Effect")
    bool IsExpired() const { return Duration <= 0; }

    void Init(ACombatantBase* InOwner, ACombatantBase* InSource);

    /**
     * Speed modifier contributed by this effect for turn order calculation.
     * Default = 0 (no change). Return positive to increase speed, negative to reduce.
     * Aggregated multiplicatively by StatusEffectManagerComponent::GetSpeedMultiplier().
     *
     * Example:  AgilityUpEffect returns  0.35f → speed × 1.35
     *           AgilityDownEffect returns -0.35f → speed × 0.65
     */
    virtual float GetSpeedModifier() const { return 0.f; }

    /** If true, this effect prevents the owner from taking their turn action. */
    virtual bool BlocksTurnAction() const { return false; }

    /** If true, this effect causes the owner to attack allies instead of enemies. */
    virtual bool ConfusesTarget() const { return false; }

    /** If true, this effect prevents the owner from receiving healing. */
    virtual bool BlocksHealing() const { return false; }

    /** If true, this effect prevents the owner from gaining AP. */
    virtual bool BlocksAPGain() const { return false; }

    /** If true, this effect prevents the owner from using skill-category abilities. */
    virtual bool BlocksAbilityUse() const { return false; }

    /** If true, this effect grants the owner an extra immediate turn after their current one ends. */
    virtual bool GrantsExtraTurn() const { return false; }

    /** Multiplier applied to enemy targeting weight for this combatant. Default = 1. */
    virtual float GetTargetWeight() const { return 1.f; }

    /** Called by StatusEffectManagerComponent::ConsumeExtraTurn to reset the grant. */
    virtual void ConsumeExtraTurnGrant() {}
};
