#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "WorldStateSubsystem.generated.h"

/**
 * UWorldStateSubsystem
 *
 * The "this already happened" registry — a single persistent set of done-keys.
 * Anything that should only ever fire once (a played cutscene, a collected
 * world pickup, a defeated boss) records its key here when it fires, and checks
 * the set on spawn to decide whether to skip itself.
 *
 * Keys are namespaced FNames built by MakeKey(): "<category>/<level>/<id>", so
 * the same designer-id is distinct per level and per category.
 *
 * The set lives on the GameInstance (survives level travel) and is serialized
 * into the save file, so it also survives quitting the game.
 */
UCLASS()
class JRPGCOMBAT_API UWorldStateSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    /** Record a key as done. Idempotent. */
    UFUNCTION(BlueprintCallable, Category = "WorldState")
    void MarkDone(FName Key);

    /** True if the key has been recorded. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldState")
    bool IsDone(FName Key) const;

    /** Build a namespaced key: "<Category>/<canonicalLevel>/<Id>". If Id is
     *  None the caller should fall back to the actor name before calling. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "WorldState",
              meta = (WorldContext = "WorldContextObject"))
    static FName MakeKey(const UObject* WorldContextObject, FName Category, FName Id);

    /** Reset everything (new game). */
    UFUNCTION(BlueprintCallable, Category = "WorldState")
    void ClearAll();

    // Save/load plumbing (used by the save system to round-trip the set).
    const TSet<FName>& GetDoneKeys() const { return DoneKeys; }
    void SetDoneKeys(const TSet<FName>& In) { DoneKeys = In; }

private:

    UPROPERTY()
    TSet<FName> DoneKeys;
};
