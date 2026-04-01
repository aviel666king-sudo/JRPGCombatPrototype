#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "ProtocolManagerComponent.generated.h"

/**
 * UProtocolManagerComponent
 *
 * Attached to ABattleManager.  Owns the SHARED PARTY POOL of charges for the
 * three protocols: Healing, Revival, and AP.
 *
 * All three protocols share one pool — any player character can spend from it
 * on their turn.  This scales automatically when the party grows to 3 members
 * because the charges are global, not per-character.
 *
 * Default charge counts (set in constructor, override in Blueprint CDO):
 *   Healing  : 2 charges  (heals 40% MaxHP to a living ally)
 *   Revival  : 1 charge   (revives a dead ally at 35% HP)
 *   AP       : 2 charges  (grants +5 AP to a living ally)
 *
 * Usage:
 *   BattleManager creates this component and calls InitializeCharges() on
 *   battle start.  CombatHUDWidget reads GetProtocolInfo() to populate the
 *   Protocol submenu.  Protocol abilities call SpendCharge() and CanSpend().
 */
UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class JRPGCOMBAT_API UProtocolManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UProtocolManagerComponent();

    // -------------------------------------------------------------------------
    //  Config — edit in Blueprint CDO or Details panel on ABattleManager
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protocols", meta = (ClampMin = "0"))
    int32 HealingCharges = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protocols", meta = (ClampMin = "0"))
    int32 RevivalCharges = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Protocols", meta = (ClampMin = "0"))
    int32 APCharges = 2;

    // -------------------------------------------------------------------------
    //  Battle lifecycle
    // -------------------------------------------------------------------------

    /**
     * Reset current charges to their configured starting values.
     * Called by ABattleManager::Phase_Initialize().
     */
    UFUNCTION(BlueprintCallable, Category = "Protocols")
    void InitializeCharges();

    // -------------------------------------------------------------------------
    //  Charge management
    // -------------------------------------------------------------------------

    /** Returns true if at least one charge of Type remains. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Protocols")
    bool CanSpend(EProtocolType Type) const;

    /**
     * Spend one charge of Type.
     * Asserts CanSpend — call CanSpend first if there's any doubt.
     */
    UFUNCTION(BlueprintCallable, Category = "Protocols")
    void SpendCharge(EProtocolType Type);

    /** Returns the current charge count for Type. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Protocols")
    int32 GetCurrentCharges(EProtocolType Type) const;

    /** Returns the max charge count for Type (= the configured starting value). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Protocols")
    int32 GetMaxCharges(EProtocolType Type) const;

    /**
     * Build an array of FProtocolInfo snapshots — one per protocol.
     * Used by the Protocol submenu to populate its button list.
     * Order: Healing, Revival, AP.
     */
    UFUNCTION(BlueprintCallable, Category = "Protocols")
    TArray<FProtocolInfo> GetAllProtocolInfo() const;

private:

    // Live charge counts during battle.
    int32 CurrentHealingCharges = 0;
    int32 CurrentRevivalCharges = 0;
    int32 CurrentAPCharges      = 0;
};
