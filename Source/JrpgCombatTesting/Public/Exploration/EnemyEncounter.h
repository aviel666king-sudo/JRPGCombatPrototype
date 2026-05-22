#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyEncounter.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UWidgetComponent;
class ACombatantBase;
class ABattleArena;
class AExplorationPawn;
class UEnemyDetectionComponent;
class UDetectionMeterWidget;

/**
 * Why the player's Q-press did or didn't land on this encounter. Used by
 * AExplorationPawn::HandleAssassinate to print a specific on-screen reason
 * and by AEnemyEncounter::Tick to color-code the range circle.
 */
UENUM(BlueprintType)
enum class EAssassinationStatus : uint8
{
    Ready       UMETA(DisplayName = "Ready"),
    OutOfRange  UMETA(DisplayName = "Out of range"),
    NotBehind   UMETA(DisplayName = "Not behind enemy"),
    Alerted     UMETA(DisplayName = "Enemy is alerted"),
    Chasing     UMETA(DisplayName = "Enemy is chasing"),
    NoPlayer    UMETA(DisplayName = "No player")
};

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

    /** Vision / LOS detection (Phase B). Auto-created on every encounter so
     *  per-BP wiring isn't required. Tunables (radius, cone, distance, time)
     *  are editable in the BP defaults under Detection|Vision. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
    TObjectPtr<UEnemyDetectionComponent> Detection;

    /** Floating detection-meter UI (Phase B2). Hosts UDetectionMeterWidget /
     *  WBP_DetectionMeter in world-space, billboarded toward the camera.
     *  Set DetectionMeterWidgetClass below to the WBP asset in BP defaults. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Encounter")
    TObjectPtr<UWidgetComponent> DetectionMeterComponent;

    /** UMG asset that derives from UDetectionMeterWidget. Assign WBP_DetectionMeter
     *  in BP_EnemyEncounter defaults — if left null, no meter is shown. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Encounter|UI")
    TSubclassOf<UDetectionMeterWidget> DetectionMeterWidgetClass;

    /** How far above the encounter root the meter floats (cm). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Encounter|UI",
              meta = (ClampMin = "0.0"))
    float DetectionMeterHeight = 220.f;

    /** Draw size of the world-space meter widget in pixels. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Encounter|UI")
    FVector2D DetectionMeterDrawSize = FVector2D(200.f, 32.f);

    // -------------------------------------------------------------------------
    //  Configuration — set per-encounter in the level Details panel
    // -------------------------------------------------------------------------

    /** Enemies to spawn when this encounter starts. Order = enemy spawn-slot index. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter")
    TArray<TSubclassOf<ACombatantBase>> EnemyClasses;

    /** Effective level of this encounter — compared against
     *  DangerManager.PlayerEffectiveLevel to decide if a stealth approach
     *  qualifies as an assassination (overleveled by AssassinationLevelGap+). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter",
              meta = (ClampMin = "1"))
    int32 EncounterLevel = 1;

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

    // -------------------------------------------------------------------------
    //  Chase (Phase C)
    //
    //  When Detection->OnDetectionFull fires, the encounter enters chase
    //  state and starts moving toward the player. If the player breaks LOS
    //  for ChaseGiveUpSeconds, the encounter gives up and walks back to
    //  HomeLocation. If the player gets within ChaseAcceptanceDistance while
    //  chasing, the trigger sphere overlap will start combat with enemy
    //  initiative (caught from behind = bad spawn).
    //
    //  Movement is a simple Vector lerp on the actor's transform — no nav
    //  mesh, no character movement. Real patrol AI is later (Phase C2 / D).
    // -------------------------------------------------------------------------

    /** Base chase movement speed in cm/s. Scaled at runtime by danger level.
     *  EditAnywhere so each placed encounter can have a unique value (e.g.
     *  slow heavy-armor enemies vs fast scouts). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Chase",
              meta = (ClampMin = "0.0"))
    float ChaseSpeed = 400.f;

    /** How long the encounter chases without LOS before giving up (seconds).
     *  Per-instance: cautious enemies give up fast, persistent ones don't. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Chase",
              meta = (ClampMin = "0.0"))
    float ChaseGiveUpSeconds = 4.f;

    /** Hard distance cap — if player escapes beyond this from the encounter's
     *  HomeLocation, give up immediately even with LOS. cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Chase",
              meta = (ClampMin = "0.0"))
    float ChaseMaxRange = 4000.f;

    /** Movement speed when returning to home after giving up (cm/s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Chase",
              meta = (ClampMin = "0.0"))
    float ReturnSpeed = 250.f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Chase")
    bool IsChasing() const { return bIsChasing; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Chase")
    bool IsReturningToHome() const { return bIsReturningToHome; }

    /** Called when this encounter's detection meter fully fills (or a neighbor
     *  alerts it via the one-hop broadcast). Begins chase state. */
    UFUNCTION()
    void HandleDetectionFull(AEnemyEncounter* DetectingEncounter);

    /** Force-stops chase and returns the encounter to its HomeLocation. */
    UFUNCTION(BlueprintCallable, Category = "Encounter|Chase")
    void StopChase();

    // -------------------------------------------------------------------------
    //  Patrol
    //
    //  When idle (not chasing, not returning), the encounter walks between
    //  PatrolOffsets in order, pausing PatrolWaitSecondsAtWaypoint at each.
    //  Offsets are world-space deltas relative to HomeLocation, so moving the
    //  encounter in the level moves its whole patrol route with it.
    //
    //  Empty PatrolOffsets = stand still at HomeLocation (legacy behavior).
    // -------------------------------------------------------------------------

    /** Waypoint offsets from HomeLocation. Empty = no patrol, stand at home. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Patrol",
              meta = (MakeEditWidget = "true"))
    TArray<FVector> PatrolOffsets;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Patrol",
              meta = (ClampMin = "0.0"))
    float PatrolSpeed = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Patrol",
              meta = (ClampMin = "0.0"))
    float PatrolWaitSecondsAtWaypoint = 2.f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Patrol")
    bool HasPatrolRoute() const { return PatrolOffsets.Num() > 0; }

    // -------------------------------------------------------------------------
    //  Assassination (Phase E)
    //
    //  Player presses Q while close, behind the encounter, AND undetected.
    //  - If PlayerEffectiveLevel >= EncounterLevel + AssassinationLevelGap →
    //    encounter is destroyed instantly. No combat.
    //  - Otherwise → combat begins with player initiative (the "stealth
    //    turn-priority" branch from the GDD).
    // -------------------------------------------------------------------------

    /** Half-angle (degrees) within which "behind" counts. 90 = entire rear hemisphere. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Assassination",
              meta = (ClampMin = "1.0", ClampMax = "179.0"))
    float AssassinationBehindHalfAngleDeg = 90.f;

    /** Max distance for assassination attempt (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Assassination",
              meta = (ClampMin = "0.0"))
    float AssassinationRange = 250.f;

    /** Multiplier of AssassinationRange — within this distance the ground
     *  range-circle / behind-arc viz draws. 0 = never show, 2.0 = show when
     *  player is within 2x assassination range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Assassination",
              meta = (ClampMin = "0.0"))
    float AssassinationVizDistanceMult = 2.5f;

    /** Granular check — returns the SPECIFIC reason a Q-press would (not) land,
     *  so the caller can show "Out of range" vs "Not behind" etc. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Assassination")
    EAssassinationStatus GetAssassinationStatus(APawn* Player) const;

    /** Convenience — true iff GetAssassinationStatus == Ready. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Assassination")
    bool CanBeAssassinated(APawn* Player) const;

    /** True only if all CanBeAssassinated conditions pass AND the player is
     *  overleveled by AssassinationLevelGap+. Used to distinguish instant-kill
     *  from stealth-with-priority. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Assassination")
    bool IsOverleveledForAssassination() const;

    /** Execute the assassination — destroys this encounter, awards reduced XP. */
    UFUNCTION(BlueprintCallable, Category = "Encounter|Assassination")
    void Assassinate(APawn* Attacker);

    /**
     * Manually trigger combat with explicit initiative (used by the cone shot,
     * which doesn't rely on overlap). Pass true to grant the player first turn.
     */
    UFUNCTION(BlueprintCallable, Category = "Encounter")
    void TriggerCombat(bool bPlayerHasInitiative);

protected:

    virtual void BeginPlay() override;
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

    /** Chase runtime state. Driven by HandleDetectionFull + Tick. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Chase")
    bool bIsChasing = false;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Chase")
    bool bIsReturningToHome = false;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Chase")
    float LostSightTimer = 0.f;

    /** Captured in BeginPlay. The spot the encounter goes back to after giving up. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Chase")
    FVector HomeLocation = FVector::ZeroVector;

    /** Patrol runtime state. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Patrol")
    int32 CurrentPatrolIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Patrol")
    float PatrolWaitTimer = 0.f;

    void TickChase(float DeltaTime);
    void TickReturnToHome(float DeltaTime);
    void TickPatrol(float DeltaTime);

    /** Resolve a patrol offset to a world-space target. */
    FVector GetPatrolTargetWorld(int32 Index) const;

#if !UE_BUILD_SHIPPING
    /** Draws the ground ring + behind arc + status label every Tick when the
     *  player is within AssassinationVizDistanceMult * AssassinationRange. */
    void DrawAssassinationViz();
#endif
};
