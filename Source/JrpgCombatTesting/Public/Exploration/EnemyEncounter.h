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

    // -------------------------------------------------------------------------
    //  Pre-combat stun
    //
    //  The player can shoot an encounter with the gun (LMB while aiming) to
    //  stun it. While stunned the encounter shouldn't chase the player (no
    //  patrol AI yet, but the flag is exposed for future use), and if the
    //  player walks into a stunned encounter combat starts with the player's
    //  fastest party member acting first.
    // -------------------------------------------------------------------------

    /** Stun this encounter for Duration seconds. Resets if already stunned. */
    UFUNCTION(BlueprintCallable, Category = "Encounter|Stun")
    void Stun(float Duration);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Stun")
    bool IsStunned() const { return bIsStunned; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Stun")
    float GetStunRemaining() const { return StunRemaining; }

    /**
     * Manually trigger combat with explicit initiative (used by the cone shot,
     * which doesn't rely on overlap). Pass true to grant the player first turn.
     */
    UFUNCTION(BlueprintCallable, Category = "Encounter")
    void TriggerCombat(bool bPlayerHasInitiative);

protected:

    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void HandleTriggerOverlap(UPrimitiveComponent* OverlappedComponent,
                              AActor* OtherActor,
                              UPrimitiveComponent* OtherComp,
                              int32 OtherBodyIndex,
                              bool bFromSweep,
                              const FHitResult& SweepResult);

    /** Stun runtime state. Counts down on Tick; clears bIsStunned at zero. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Stun")
    bool bIsStunned = false;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Stun")
    float StunRemaining = 0.f;
};
