#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CombatTypes.h"
#include "UI/Minigames/AbilityMinigameWidget.h"
#include "CombatAbility.generated.h"

class ACombatantBase;
class UStatusEffect;

UCLASS(Blueprintable, BlueprintType, EditInlineNew, Abstract)
class JRPGCOMBAT_API UCombatAbility : public UObject
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  Config
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Ability")
    TArray<FAbilityCost> Costs;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability", meta = (ClampMin = "0"))
    int32 MaxCooldown = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    ETargetScope TargetScope = ETargetScope::SingleEnemy;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    bool bEndsTurn = true;

    // -------------------------------------------------------------------------
    //  Category — drives which bottom-panel menu slot this ability appears in.
    //  Set this in every C++ ability constructor.
    //
    //  Melee  → shown when the player presses the "Melee" button (one per char)
    //  Gun    → shown when the player presses the "Gun" button   (one per char)
    //  Skill  → listed in the Skill submenu
    //  None   → internal / enemy ability, never shown in the player menu
    // -------------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability")
    EAbilityCategory AbilityCategory = EAbilityCategory::None;

    // -------------------------------------------------------------------------
    //  Minigame — skill-only interactive timing challenge
    //
    //  Set MinigameClass on any Skill-category ability to trigger a minigame
    //  overlay after the player confirms a target.  Leave null for instant
    //  execution (no minigame).
    //
    //  bIsSupportAbility controls the minigame result mapping:
    //    false (default) → multiplier applied to BaseDamage (damage skills).
    //                       Blue strip available (1.10×).
    //    true            → multiplier maps to buff/effect duration.
    //                       Blue strip suppressed; max result is 1.0 (4 turns).
    //                       Skills that use this must read ActiveMultiplier and
    //                       map: 0.50→2 turns, 0.75→3 turns, 1.00→4 turns.
    // -------------------------------------------------------------------------

    /**
     * Optional minigame that fires after target confirmation for this skill.
     * Only consulted when AbilityCategory == Skill.
     * Assign UDiamondTimingMinigame::StaticClass() (or a future subclass) here.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Minigame")
    TSubclassOf<UAbilityMinigameWidget> MinigameClass;

    /**
     * True if this is a buff, heal, or otherwise non-damaging skill.
     * Propagated to the minigame widget — suppresses the blue strip and clamps
     * the result multiplier to 1.0.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ability|Minigame")
    bool bIsSupportAbility = false;

    /**
     * Set by BattleManager before TryActivateAbility is called.
     * MakeDamagePayload multiplies BaseDamage by this automatically.
     * Support skills read this directly to determine effect duration.
     * Always 1.0 when no minigame ran.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Ability|Minigame")
    float ActiveMultiplier = 1.0f;

    // -------------------------------------------------------------------------
    //  Runtime state
    // -------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "Ability")
    int32 CurrentCooldown = 0;

    // -------------------------------------------------------------------------
    //  Execution
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Ability")
    void Execute(ACombatantBase* Instigator, const TArray<ACombatantBase*>& Targets);

    UFUNCTION(BlueprintImplementableEvent, Category = "Ability", meta = (DisplayName = "On Execute"))
    void BP_OnExecute(ACombatantBase* Instigator, const TArray<ACombatantBase*>& Targets);

    // -------------------------------------------------------------------------
    //  Activation check
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Ability")
    bool CanActivate(const ACombatantBase* Instigator) const;
    virtual bool CanActivate_Implementation(const ACombatantBase* Instigator) const;

    // -------------------------------------------------------------------------
    //  Cooldown
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Ability")
    void TickCooldown();

    UFUNCTION(BlueprintCallable, Category = "Ability")
    void StartCooldown();

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Ability")
    bool IsReady() const { return CurrentCooldown <= 0; }

    // -------------------------------------------------------------------------
    //  Initialisation
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, Category = "Ability")
    void Initialize(ACombatantBase* OwningCombatant);
    virtual void Initialize_Implementation(ACombatantBase* OwningCombatant);

protected:

    virtual void Execute_Implementation(ACombatantBase* Instigator, const TArray<ACombatantBase*>& Targets);

    UFUNCTION(BlueprintCallable, Category = "Ability")
    FDamagePayload MakeDamagePayload(ACombatantBase* Instigator, float BaseDamage, EDamageType DamageType) const;

    UFUNCTION(BlueprintCallable, Category = "Ability")
    void ApplyEffectToTarget(ACombatantBase* Instigator,
                             ACombatantBase* Target,
                             TSubclassOf<UStatusEffect> EffectClass) const;

    UFUNCTION(BlueprintCallable, Category = "Ability")
    void ApplyEffectToTargetWithDuration(ACombatantBase* Instigator,
                                          ACombatantBase* Target,
                                          TSubclassOf<UStatusEffect> EffectClass,
                                          int32 Duration) const;
};
