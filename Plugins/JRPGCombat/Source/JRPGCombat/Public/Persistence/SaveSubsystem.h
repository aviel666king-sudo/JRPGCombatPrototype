#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SaveSubsystem.generated.h"

class UJrpgSaveGame;

/**
 * USaveSubsystem
 *
 * Orchestrates disk save/load. Gathers state from the roster, world-state and
 * visited-checkpoint subsystems (plus the current level + pawn transform) into
 * a UJrpgSaveGame and writes it to a named slot; LoadFromSlot reverses it and
 * travels to the saved level/position.
 *
 * Save policy (driven from the game module): checkpoint Rest saves; the Open
 * World autosaves on a timer. Both write the ActiveSlot.
 */
UCLASS()
class JRPGCOMBAT_API USaveSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    /** Slot that autosave + checkpoint-save write to. The load menu can point
     *  this at the slot the player chose to continue. */
    UPROPERTY(BlueprintReadWrite, Category = "Save")
    FString ActiveSlot = TEXT("Save_Auto");

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveToSlot(const FString& Slot);

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool SaveToActiveSlot() { return SaveToSlot(ActiveSlot); }

    /** Load a slot: applies state to all subsystems, then travels to the saved
     *  level + pawn position. */
    UFUNCTION(BlueprintCallable, Category = "Save")
    bool LoadFromSlot(const FString& Slot);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Save")
    bool DoesSlotExist(const FString& Slot) const;

    UFUNCTION(BlueprintCallable, Category = "Save")
    bool DeleteSlot(const FString& Slot);

    /** Load a slot's object for read-only metadata (DisplayName / Timestamp /
     *  SavedLevel) without applying it — for the load menu. */
    UFUNCTION(BlueprintCallable, Category = "Save")
    UJrpgSaveGame* PeekSlot(const FString& Slot) const;

private:

    FName CanonicalLevelName() const;
    void GatherInto(UJrpgSaveGame* Save);
    void ApplyFrom(UJrpgSaveGame* Save);
};
