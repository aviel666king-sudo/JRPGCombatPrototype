#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BattleArena.generated.h"

class UBillboardComponent;
class ACameraActor;

/**
 * ABattleArena
 *
 * A placeable marker actor that defines one combat location in a level.
 * Each arena holds:
 *   - A list of player spawn slot actors (combatants are teleported here on combat start)
 *   - A list of enemy spawn slot actors
 *   - References to camera actors used during combat
 *
 * Place one or more of these in a level. AEnemyEncounter actors reference an arena
 * to specify where their fight should happen.
 *
 * The simple visual root (BillboardComponent) is editor-only — the actor itself
 * has no in-game appearance. Place arenas in hidden parts of the level (off-camera
 * "combat stages") that the player teleports to when an encounter starts.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBAT_API ABattleArena : public AActor
{
    GENERATED_BODY()

public:

    ABattleArena();

    // -------------------------------------------------------------------------
    //  Spawn slots
    //  Place simple actors (TargetPoints, empty Actors, or grass pads) in the
    //  level and assign them here. Index 0 = first player/enemy, etc.
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Slots")
    TArray<TObjectPtr<AActor>> PlayerSpawnPoints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Slots")
    TArray<TObjectPtr<AActor>> EnemySpawnPoints;

    // -------------------------------------------------------------------------
    //  Cameras
    //  Same set of cameras BattleManager has used until now — but defined per
    //  arena so each combat location can have its own framing.
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Cameras")
    TObjectPtr<ACameraActor> BaseCameraActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Cameras")
    TObjectPtr<ACameraActor> CharacterFocusCameraActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Cameras")
    TObjectPtr<ACameraActor> EnemyCursorCameraActor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Arena|Cameras")
    TObjectPtr<ACameraActor> GunAimCameraActor;

protected:

    /** Editor-only sprite for selecting the arena in the level. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
    TObjectPtr<UBillboardComponent> Billboard;
};
