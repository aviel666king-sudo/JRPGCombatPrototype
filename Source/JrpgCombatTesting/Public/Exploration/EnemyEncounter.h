#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyEncounter.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ACombatantBase;
class ABattleArena;
class AExplorationPawn;

/**
 * AEnemyEncounter
 *
 * A placeable world actor representing one fightable encounter.
 *
 * Configure in the level:
 *   - EnemyClasses: list of ACombatantBase subclasses to spawn for this fight
 *   - AssignedArena: which ABattleArena hosts the combat (each map can have several)
 *   - StaticMesh (set on root in BP defaults): the visual shown in the world
 *
 * When the player's AExplorationPawn overlaps the sphere trigger, this actor
 * tells AJrpgGameMode to start an encounter. On victory, GameMode destroys
 * this actor so the player can't refight it.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBATTESTING_API AEnemyEncounter : public AActor
{
    GENERATED_BODY()

public:

    AEnemyEncounter();

    // -------------------------------------------------------------------------
    //  Components
    // -------------------------------------------------------------------------

    /** Visual representation of the encounter in the overworld. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
    TObjectPtr<UStaticMeshComponent> Mesh;

    /** Player overlap trigger that fires the encounter. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
    TObjectPtr<USphereComponent> TriggerSphere;

    // -------------------------------------------------------------------------
    //  Configuration — set per-encounter in the level Details panel
    // -------------------------------------------------------------------------

    /** Enemies to spawn when this encounter starts. Order = enemy spawn-slot index. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
    TArray<TSubclassOf<ACombatantBase>> EnemyClasses;

    /** Arena where this fight happens. Pick any ABattleArena placed in the level. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Encounter")
    TObjectPtr<ABattleArena> AssignedArena;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter")
    const TArray<TSubclassOf<ACombatantBase>>& GetEnemyClasses() const { return EnemyClasses; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter")
    ABattleArena* GetAssignedArena() const { return AssignedArena; }

protected:

    UFUNCTION()
    void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent,
                              AActor* OtherActor,
                              UPrimitiveComponent* OtherComp,
                              int32 OtherBodyIndex,
                              bool bFromSweep,
                              const FHitResult& SweepResult);
};
