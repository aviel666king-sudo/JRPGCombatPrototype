#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "VisitedCheckpointRegistry.generated.h"

/**
 * UVisitedCheckpointRegistry
 *
 * Per-game-instance memory of which checkpoints the player has rested at,
 * keyed by level. ACheckpoint::Rest calls RegisterVisited; the fast-travel
 * widget calls GetVisitedInLevel to populate its list.
 *
 * "Visited" persists across level transitions (it lives on the GameInstance,
 * not the world). When a save/load system is added, this is the structure to
 * serialize.
 *
 * Identification:
 *   - LevelName  = FName of the level (UWorld::GetMapName, stripped of any
 *     /Game/Levels/ prefix by the caller; canonical form is just the level's
 *     short name, e.g. "L_Overworld" or "GroundZero").
 *   - CheckpointId = FName authored on each ACheckpoint actor; must be
 *     unique within its level. Defaults to the actor's name if left blank.
 */
UCLASS()
class JRPGCOMBAT_API UVisitedCheckpointRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    /** Mark CheckpointId in LevelName as visited. Idempotent. */
    UFUNCTION(BlueprintCallable, Category = "Travel|Checkpoints")
    void RegisterVisited(FName LevelName, FName CheckpointId);

    /** True if the player has ever rested at the given checkpoint. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Travel|Checkpoints")
    bool HasVisited(FName LevelName, FName CheckpointId) const;

    /** All checkpoint ids the player has rested at in LevelName. Order is
     *  insertion order so the fast-travel list stays stable across opens. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Travel|Checkpoints")
    void GetVisitedInLevel(FName LevelName, TArray<FName>& OutCheckpointIds) const;

    /** Debug / new-game reset. */
    UFUNCTION(BlueprintCallable, Category = "Travel|Checkpoints")
    void ClearAll();

    // Save/load round-trip.
    const TMap<FName, TArray<FName>>& GetAllVisited() const { return Visited; }
    void SetAllVisited(const TMap<FName, TArray<FName>>& In) { Visited = In; }

private:

    /** LevelName -> ordered list of visited checkpoint ids. TArray (not TSet)
     *  so the fast-travel widget displays them in a deterministic order. */
    TMap<FName, TArray<FName>> Visited;
};
