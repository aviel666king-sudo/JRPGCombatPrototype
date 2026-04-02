#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "CombatAnimInstance.generated.h"

/**
 * ECombatAnimState
 *
 * Represents the animation the combatant should be showing.
 * Read by the Animation Blueprint to determine which montage to play.
 */
UENUM(BlueprintType)
enum class ECombatAnimState : uint8
{
    Idle        UMETA(DisplayName = "Idle"),
    Attack      UMETA(DisplayName = "Attack"),
    Casting     UMETA(DisplayName = "Casting"),
    Parry       UMETA(DisplayName = "Parry"),
    Gun         UMETA(DisplayName = "Gun"),
    HitReact    UMETA(DisplayName = "Hit React"),
    Death       UMETA(DisplayName = "Death"),
};

/**
 * UCombatAnimInstance
 *
 * Animation instance for JRPG combatants.
 *
 * Setup in editor:
 *   1. Create an Animation Blueprint based on this class (ABP_CombatCharacter).
 *   2. In the AnimGraph add a "DefaultSlot" slot node feeding into Output Pose,
 *      with a looping MM_Idle sequence as the background layer.
 *   3. Assign montages for each animation state in the Blueprint Class Defaults.
 *
 * Animation mapping used in this project:
 *   Idle      — MM_Idle          (looping, plays in AnimGraph background)
 *   Attack    — AM_Attack        (from MM_Attack_01)
 *   Casting   — AM_Cast          (from MM_ChargedAttack)
 *   Parry     — AM_Parry         (from MM_HitReact_Back_Med_01)
 *   Gun       — AM_Gun           (from MM_Pistol_Fire)
 *   HitReact  — AM_HitReact      (from MM_HitReact_Front_Lgt_01)
 *   Death     — AM_Death         (from MM_Death_Front_01)
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API UCombatAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  State (read by the Animation Blueprint)
    // -------------------------------------------------------------------------

    /** Current animation state — driven by PlayCombatMontage(). */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Animation")
    ECombatAnimState CombatState = ECombatAnimState::Idle;

    // -------------------------------------------------------------------------
    //  Montage references — assign these in the ABP Class Defaults
    // -------------------------------------------------------------------------

    /** Played when the combatant uses a Melee ability. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Montages")
    TObjectPtr<UAnimMontage> AttackMontage;

    /** Played when the combatant uses a Skill ability (cast/charge wind-up). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Montages")
    TObjectPtr<UAnimMontage> CastMontage;

    /** Played when the combatant parries or blocks. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Montages")
    TObjectPtr<UAnimMontage> ParryMontage;

    /** Played when the combatant fires a gun ability. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Montages")
    TObjectPtr<UAnimMontage> GunMontage;

    /** Played when the combatant receives damage (and survives). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Montages")
    TObjectPtr<UAnimMontage> HitReactMontage;

    /** Played when the combatant dies. Does not loop back to idle. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Montages")
    TObjectPtr<UAnimMontage> DeathMontage;

    // -------------------------------------------------------------------------
    //  API
    // -------------------------------------------------------------------------

    /**
     * Play the montage that matches the given action state.
     * Called by ACombatantBase::PlayAbilityAnimation / PlayReactionAnimation.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Animation")
    void PlayCombatMontage(ECombatAnimState Action);

    /** True while any action montage is playing (combatant is mid-animation). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Animation")
    bool IsPlayingActionMontage() const;

    // -------------------------------------------------------------------------
    //  UAnimInstance overrides
    // -------------------------------------------------------------------------

    virtual void NativeInitializeAnimation() override;
    virtual void NativeUninitializeAnimation() override;

private:

    UFUNCTION()
    void HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    UPROPERTY()
    TObjectPtr<class ACombatantBase> OwnerCombatant;
};
