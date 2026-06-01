#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyEncounter.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class ACombatantBase;
class ABattleArena;
class AExplorationPawn;
class UEnemyDetectionComponent;

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

    /** Optional weapon this encounter drops on victory — added to the player's
     *  owned inventory so it becomes switchable. Bind a specific weapon to a
     *  specific enemy here (e.g. the Medalume sword). Leave null for no weapon
     *  drop (the enemy still gives the global gold + material drop). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Drops")
    TObjectPtr<class UCharacterWeaponDataAsset> WeaponDrop;

    /** Arena where this fight happens. Pick any ABattleArena placed in the level. */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Encounter")
    TObjectPtr<ABattleArena> AssignedArena;

    // -------------------------------------------------------------------------
    //  Encounter merging (Phase F)
    //
    //  Each encounter holds 3 base enemies (EnemyClasses) PLUS one "elite"
    //  enemy (EliteEnemyClass) that's reserved exclusively for merged fights.
    //  In a solo fight you only ever see the 3 base enemies — the elite is
    //  the encounter's hidden ace, only deployed if the encounter joins a
    //  larger ambush.
    //
    //  Merge rule (player-caught only, never on successful stealth):
    //    No merge        → 3 base enemies (elite stays hidden)
    //    +1 encounter    → triggerer's elite + neighbour's elite + 1 base
    //    +2 encounters   → all 3 elites (no base at all — full elite fight)
    //    +3+ encounters  → 3 elites (extras dropped, first 3 in iteration)
    //
    //  Use bAllowMerging = false on bosses / story fights to opt them out
    //  entirely.
    // -------------------------------------------------------------------------

    /** Whether this encounter participates in the merging system at all
     *  (both as a possible triggerer AND as a possible neighbour to consume).
     *  False = bosses / story fights that should always be solo. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Merging")
    bool bAllowMerging = true;

    /** The single elite enemy this encounter contributes when ANY merge happens
     *  (whether this encounter is the triggerer or a merged neighbour). Hidden
     *  in solo fights — the encounter shows 3 base enemies only. Leave null
     *  to skip contributing during merges; the encounter still gets consumed
     *  but doesn't add an enemy to the fight. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Merging")
    TSubclassOf<ACombatantBase> EliteEnemyClass;

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
    //  Patrol — explicit waypoints
    //
    //  When PatrolOffsets is populated, the encounter walks between them in
    //  order, pausing PatrolWaitSecondsAtWaypoint at each. Offsets are world-
    //  space deltas relative to HomeLocation, so moving the encounter in the
    //  level moves its whole patrol route with it.
    //
    //  Empty PatrolOffsets → falls through to the random Wander mode below
    //  (no setup needed for ambient patrolling).
    // -------------------------------------------------------------------------

    /** Waypoint offsets from HomeLocation. Empty = auto-wander mode. */
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
    //  Wander — automatic random patrol around spawn
    //
    //  Used when PatrolOffsets is empty. The encounter picks a random point
    //  within WanderRadius of HomeLocation, walks there at PatrolSpeed, pauses
    //  for a random interval in [WanderPauseMin, WanderPauseMax], picks a new
    //  point. Gives every placed enemy "life" without per-instance setup.
    // -------------------------------------------------------------------------

    /** Max distance from HomeLocation the encounter will wander to (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Wander",
              meta = (ClampMin = "0.0"))
    float WanderRadius = 400.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Wander",
              meta = (ClampMin = "0.0"))
    float WanderPauseMin = 2.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Wander",
              meta = (ClampMin = "0.0"))
    float WanderPauseMax = 4.f;

    // -------------------------------------------------------------------------
    //  Investigate — partial-detection wander toward the suspected spot
    //
    //  When detection meter exceeds InvestigateDetectionThreshold (but stays
    //  below 1.0 — full meter = chase, not investigate), the encounter walks
    //  to the last-seen player location at InvestigateSpeedScale × ChaseSpeed,
    //  stands and looks around for InvestigateLookDuration, then returns to
    //  whatever patrol/wander it was doing.
    // -------------------------------------------------------------------------

    /** Detection meter fraction above which the encounter starts investigating
     *  (0.15 = 15%). Must be < 1.0 (full = chase). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Investigate",
              meta = (ClampMin = "0.0", ClampMax = "0.99"))
    float InvestigateDetectionThreshold = 0.15f;

    /** Walk speed during the "go to last-seen" leg, as a fraction of ChaseSpeed.
     *  0.5 = half speed — distinct from a chase, faster than patrol. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Investigate",
              meta = (ClampMin = "0.0"))
    float InvestigateSpeedScale = 0.5f;

    /** Seconds spent looking around at the suspected location before giving up. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Investigate",
              meta = (ClampMin = "0.0"))
    float InvestigateLookDuration = 2.5f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Encounter|Investigate")
    bool IsInvestigating() const { return bIsInvestigating; }

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

    // -------------------------------------------------------------------------
    //  Combat-time freeze + visibility
    //
    //  While the player is fighting one encounter, the others stop ticking,
    //  hide their mesh, and turn off their trigger sphere — so wandering
    //  enemies don't render in the background of the arena and can't trigger
    //  a new encounter the instant combat ends.
    // -------------------------------------------------------------------------

    /** Toggle exploration behaviour. False = invisible + non-colliding + no tick
     *  (used during combat). True = restore normal state. */
    UFUNCTION(BlueprintCallable, Category = "Encounter")
    void SetExplorationActive(bool bActive);

    /** Teleport back to HomeLocation and clear ALL transient state (wander,
     *  patrol, investigate, detection meter, chase). Called by the GameMode
     *  after a battle ends so the player isn't immediately re-overlapped by
     *  whichever encounter happened to be wandering nearby. */
    UFUNCTION(BlueprintCallable, Category = "Encounter")
    void ResetToSpawn();

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

    /** Wander runtime state — current random destination + how long to pause once we reach it. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Wander")
    FVector WanderTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Wander")
    bool bHasWanderTarget = false;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Wander")
    float WanderPauseTimer = 0.f;

    /** Investigate runtime state. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Investigate")
    bool bIsInvestigating = false;

    /** 0 = walking to the suspected spot; 1 = arrived, looking around. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Investigate")
    int32 InvestigateStage = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Investigate")
    float InvestigateLookTimer = 0.f;

    /** Updated each tick the player is visible. Used as the target when the
     *  detection meter rises past InvestigateDetectionThreshold. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Investigate")
    FVector LastSeenPlayerLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Investigate")
    bool bHasLastSeen = false;

    void TickChase(float DeltaTime);
    void TickReturnToHome(float DeltaTime);
    void TickPatrol(float DeltaTime);
    void TickWander(float DeltaTime);
    void TickInvestigate(float DeltaTime);

    /** Resolve a patrol offset to a world-space target. */
    FVector GetPatrolTargetWorld(int32 Index) const;

    /** Pick a fresh random destination inside WanderRadius around HomeLocation.
     *  Prefers a navigation-system-validated reachable point; falls back to a
     *  raw random disc point if no nav mesh is set up. */
    void PickRandomWanderTarget();

    /** Begin an investigation toward LastSeenPlayerLocation. */
    void StartInvestigation();

    // -------------------------------------------------------------------------
    //  Navigation path-following
    //
    //  Every "move toward target" call routes through MoveActorTowardTarget.
    //  Internally it asks the nav system for a path, walks corners, and falls
    //  back to direct-translate if no nav mesh is available — so the AI works
    //  whether or not the level has a NavMeshBoundsVolume placed.
    // -------------------------------------------------------------------------

    /** How often (seconds) a chasing/investigating actor recomputes its path
     *  even when the target hasn't moved. Higher = cheaper but less responsive. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Nav",
              meta = (ClampMin = "0.05"))
    float NavPathRefreshInterval = 0.4f;

    /** Distance threshold (cm) — if the moving target drifts more than this
     *  from the cached destination, force a path recompute immediately. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Encounter|Nav",
              meta = (ClampMin = "1.0"))
    float NavPathTargetDriftThreshold = 80.f;

    /** Move toward Target at Speed cm/s along a nav path. Falls back to direct
     *  translate if no path can be found. Returns true when the actor is within
     *  ArriveDistance of Target on the XY plane. */
    bool MoveActorTowardTarget(const FVector& Target, float Speed, float DeltaTime,
                               float ArriveDistance = 50.f);

    /** Cached path state. Reset by ResetToSpawn / state transitions. */
    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Nav")
    TArray<FVector> CurrentPathPoints;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Nav")
    int32 CurrentPathIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Nav")
    FVector CachedPathTarget = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly, Category = "Encounter|Nav")
    float NavPathRefreshTimer = 0.f;

    /** Recompute the path if the target has drifted or the refresh timer
     *  expired. Returns false if no path exists at all (caller falls back). */
    bool RefreshNavPath(const FVector& Target, float DeltaTime);

    /** Clear cached path so the next MoveActorTowardTarget call recomputes. */
    void InvalidateNavPath();

#if !UE_BUILD_SHIPPING
    /** Draws the ground ring + behind arc + status label every Tick when the
     *  player is within AssassinationVizDistanceMult * AssassinationRange. */
    void DrawAssassinationViz();
#endif
};
