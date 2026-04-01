#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "CombatTypes.h"
#include "TurnOrderManager.generated.h"

class ACombatantBase;

/**
 * UTurnOrderManager
 *
 * Owns the turn queue for a single battle. ABattleManager creates one instance
 * and calls it directly to advance turns and query upcoming order.
 *
 * The queue is rebuilt each round by sorting all living combatants by Speed.
 * Mid-round manipulation (skip, extra turn) modifies the live queue array.
 */
UCLASS(BlueprintType)
class JRPGCOMBAT_API UTurnOrderManager : public UObject
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  Setup
    // -------------------------------------------------------------------------

    /**
     * Build the initial queue from all battle participants.
     * Called once by ABattleManager during Initialization.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    void BuildTurnOrder(const TArray<ACombatantBase*>& AllCombatants);

    // -------------------------------------------------------------------------
    //  Advancing turns
    // -------------------------------------------------------------------------

    /**
     * Returns the combatant at the front of the queue without removing them.
     * Returns nullptr if the queue is empty.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    ACombatantBase* PeekNextCombatant() const;

    /**
     * Pop the front entry and return that combatant.
     * If the queue empties, rebuilds it for the next round (excluding dead combatants).
     * Returns nullptr if there are no living combatants.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    ACombatantBase* PopNextCombatant();

    /**
     * Rebuild and re-sort the queue for a new round.
     * Called automatically by PopNextCombatant when the queue drains.
     * Can also be called manually after a combatant dies mid-round.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    void RebuildForNewRound();

    // -------------------------------------------------------------------------
    //  Manipulation
    // -------------------------------------------------------------------------

    /**
     * Remove the given combatant's next entry from the queue (skip turn).
     * No-op if they are not in the queue.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    void SkipNextTurn(ACombatantBase* Combatant);

    /**
     * Insert an extra immediate turn for the given combatant at the front
     * of the queue. Used by abilities that grant additional actions.
     */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    void InsertExtraTurn(ACombatantBase* Combatant);

    // -------------------------------------------------------------------------
    //  Queries
    // -------------------------------------------------------------------------

    /** Ordered snapshot of upcoming turns for UI display. */
    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    TArray<FTurnEntry> GetCurrentQueue() const { return Queue; }

    UFUNCTION(BlueprintCallable, Category = "Turn Order")
    bool IsQueueEmpty() const { return Queue.IsEmpty(); }

    // 1-based counter incremented each time PopNextCombatant fires.
    UPROPERTY(BlueprintReadOnly, Category = "Turn Order")
    int32 TurnNumber = 0;

private:

    /** The live queue. Front of array = acts next. */
    UPROPERTY()
    TArray<FTurnEntry> Queue;

    /** Master list of all combatants registered at battle start. */
    UPROPERTY()
    TArray<TObjectPtr<ACombatantBase>> AllCombatants;

    /** Build a sorted FTurnEntry list from AllCombatants (living only). */
    TArray<FTurnEntry> BuildSortedEntries() const;
};
