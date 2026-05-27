#include "Exploration/EnemyEncounter.h"
#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyDetectionComponent.h"
#include "Core/DangerManager.h"
#include "Characters/Enemy/EnemyCombatant.h"
#include "Characters/Player/PlayerCombatant.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"
#include "NavigationSystem.h"
#include "NavigationPath.h"

AEnemyEncounter::AEnemyEncounter()
{
    // Tick to count down stun timer. Cheap — almost always early-outs.
    PrimaryActorTick.bCanEverTick = true;

    // -------------------------------------------------------------------------
    //  Mesh — visual placeholder, set the actual mesh in BP defaults
    // -------------------------------------------------------------------------

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionObjectType(ECC_WorldDynamic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // -------------------------------------------------------------------------
    //  Trigger sphere — fires when the exploration pawn overlaps
    // -------------------------------------------------------------------------

    TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
    TriggerSphere->SetupAttachment(RootComponent);
    TriggerSphere->InitSphereRadius(150.f);
    TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerSphere->SetGenerateOverlapEvents(true);

    TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemyEncounter::HandleTriggerOverlap);

    // -------------------------------------------------------------------------
    //  Detection (Phase B) — auto-attached so all encounters get vision
    //  without per-BP wiring. Tunables in BP defaults under Detection|Vision.
    // -------------------------------------------------------------------------

    Detection = CreateDefaultSubobject<UEnemyDetectionComponent>(TEXT("Detection"));
}

void AEnemyEncounter::BeginPlay()
{
    Super::BeginPlay();

    // Snapshot the spawn location — chase falls back to this when giving up.
    HomeLocation = GetActorLocation();

    // Hook the detection component so we know when to start chasing.
    if (Detection)
    {
        Detection->OnDetectionFull.AddDynamic(this, &AEnemyEncounter::HandleDetectionFull);
    }
}

void AEnemyEncounter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Cache the player's current world position any time we can see them.
    // The investigate state uses this as its "go-look-here" target.
    if (Detection && Detection->bPlayerVisible)
    {
        if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
        {
            LastSeenPlayerLocation = Player->GetActorLocation();
            bHasLastSeen = true;
        }
    }

#if !UE_BUILD_SHIPPING
    // Assassination viz draws EVERY tick regardless of AI state — otherwise
    // stunned enemies wouldn't show the Q-prompt (Tick used to early-return on
    // stun, hiding the icon mid-channel). Cheap on its own + culled by range
    // inside DrawAssassinationViz.
    DrawAssassinationViz();
#endif

    // If the player is currently channeling an assassination on US, freeze
    // every AI branch — no movement, no rotation, no stun countdown — so the
    // channel can't be aborted by a "Target turned around" caused by our own
    // wander/patrol logic ticking under it (or by stun expiring and us
    // resuming wander mid-strike).
    if (APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0))
    {
        if (const AExplorationPawn* PlayerPawn = Cast<AExplorationPawn>(Player))
        {
            if (PlayerPawn->IsAssassinating() && PlayerPawn->GetAssassinationTarget() == this)
            {
                return;
            }
        }
    }

    // Stun freezes the AI — the timer still counts down but movement stops.
    if (bIsStunned)
    {
        StunRemaining -= DeltaTime;
        if (StunRemaining <= 0.f)
        {
            bIsStunned    = false;
            StunRemaining = 0.f;
            UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s recovered from stun."), *GetName());
        }
        return;
    }

    if (bIsChasing)
    {
        TickChase(DeltaTime);
    }
    else if (bIsReturningToHome)
    {
        TickReturnToHome(DeltaTime);
    }
    else
    {
        // Investigate trigger — partial detection (above threshold, below full)
        // bumps the encounter into "go check it out" mode without committing to
        // a chase. Full meter still fires OnDetectionFull and enters chase.
        if (!bIsInvestigating && bHasLastSeen && Detection
            && Detection->DetectionMeter > InvestigateDetectionThreshold
            && Detection->DetectionMeter < 1.f)
        {
            StartInvestigation();
        }

        if (bIsInvestigating)
        {
            TickInvestigate(DeltaTime);
        }
        else if (HasPatrolRoute())
        {
            TickPatrol(DeltaTime);
        }
        else
        {
            TickWander(DeltaTime);
        }
    }
}

#if !UE_BUILD_SHIPPING
void AEnemyEncounter::DrawAssassinationViz()
{
    if (AssassinationVizDistanceMult <= 0.f || AssassinationRange <= 0.f) { return; }

    APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Player) { return; }

    const FVector MyLoc     = GetActorLocation();
    const FVector PlayerLoc = Player->GetActorLocation();
    const float DistToPlayer = FVector::Dist(MyLoc, PlayerLoc);

    // Cull when player is too far away to care about this encounter.
    if (DistToPlayer > AssassinationRange * AssassinationVizDistanceMult) { return; }

    const EAssassinationStatus Status = GetAssassinationStatus(Player);

    // Pick colour from current status. Bright green = press Q now.
    FColor RingColor;
    switch (Status)
    {
    case EAssassinationStatus::Ready:      RingColor = FColor(50, 255, 80);  break; // green
    case EAssassinationStatus::NotBehind:  RingColor = FColor(255, 220, 40); break; // yellow
    case EAssassinationStatus::OutOfRange: RingColor = FColor(255, 140, 30); break; // orange
    case EAssassinationStatus::Alerted:    RingColor = FColor(255, 40, 40);  break; // red
    case EAssassinationStatus::Chasing:    return;                                 // chase has its own viz
    default:                               RingColor = FColor(180,180,180);  break;
    }

    // Ground ring at AssassinationRange, drawn slightly below the encounter's
    // origin so it lies near the floor. The last two FVector args are the
    // X/Y axes of the circle's plane — feeding world X/Y keeps it flat.
    const FVector RingCenter(MyLoc.X, MyLoc.Y, MyLoc.Z - 40.f);
    DrawDebugCircle(GetWorld(), RingCenter, AssassinationRange, 48, RingColor,
                    /*bPersistent=*/false, /*LifeTime=*/-1.f, /*DepthPriority=*/0,
                    /*Thickness=*/3.f, FVector(1,0,0), FVector(0,1,0),
                    /*bDrawAxis=*/false);

    // Behind-hemisphere arc — draw a filled-ish wedge by stepping line segments
    // from -HalfAngle to +HalfAngle on the BACK of the actor (yaw + 180).
    const float HalfRad     = FMath::DegreesToRadians(AssassinationBehindHalfAngleDeg);
    const float YawRad      = FMath::DegreesToRadians(GetActorRotation().Yaw + 180.f);
    const int32 Segments    = 24;
    FVector PrevPoint = RingCenter
        + FVector(FMath::Cos(YawRad - HalfRad), FMath::Sin(YawRad - HalfRad), 0.f) * AssassinationRange;
    for (int32 i = 1; i <= Segments; ++i)
    {
        const float T     = static_cast<float>(i) / static_cast<float>(Segments);
        const float Angle = YawRad - HalfRad + (2.f * HalfRad) * T;
        const FVector P   = RingCenter
            + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * AssassinationRange;

        // Line from center to perimeter every other step — gives the wedge a
        // "filled" look without the cost of a real triangle fill.
        if (i % 2 == 0)
        {
            DrawDebugLine(GetWorld(), RingCenter, P, RingColor, false, -1.f, 0, 1.5f);
        }
        // Arc edge.
        DrawDebugLine(GetWorld(), PrevPoint, P, RingColor, false, -1.f, 0, 2.5f);
        PrevPoint = P;
    }

    // ----- Q-prompt + channel bar -------------------------------------------
    //  - Q icon: bright green when Ready, dim grey when not.
    //  - Progress bar: appears below the icon while the player is channeling
    //    THIS encounter, fills left → right.
    //  Both use FULL camera-facing rotation (yaw + pitch) so the plates always
    //  show face-on to the camera — no visible "3D box" from steep angles.
    //  Vertical placement is relative to the actor's bounding-box top, so the
    //  UI sits just above the encounter regardless of mesh size.

    // Top-of-mesh Z in world space. Falls back to actor origin if no bounds.
    float TopZ = MyLoc.Z + 100.f;
    {
        FVector ActorOrigin, ActorBoxExtent;
        GetActorBounds(/*bOnlyCollidingComponents=*/false, ActorOrigin, ActorBoxExtent);
        if (!ActorBoxExtent.IsNearlyZero())
        {
            TopZ = ActorOrigin.Z + ActorBoxExtent.Z;
        }
    }

    const FVector IconCenter(MyLoc.X, MyLoc.Y, TopZ + 55.f);
    const FVector BarCenter (MyLoc.X, MyLoc.Y, TopZ + 22.f);

    // Camera-facing rotation — bar's local X axis points AT the camera so the
    // Y-Z face is always perpendicular to view = reads as a flat 2D rectangle.
    FQuat BillboardQuat = FQuat::Identity;
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        FVector CamLoc;
        FRotator CamRot;
        PC->GetPlayerViewPoint(CamLoc, CamRot);
        const FVector ToCam = CamLoc - IconCenter;
        if (!ToCam.IsNearlyZero())
        {
            BillboardQuat = ToCam.Rotation().Quaternion();
        }
    }

    const bool bIsReady = (Status == EAssassinationStatus::Ready);

    const FColor IconFillColor = bIsReady ? FColor(60, 240, 90) : FColor( 90,  90,  90);
    const FColor IconEdgeColor = bIsReady ? FColor(220,255,220) : FColor(160, 160, 160);

    // Local axes (after billboard rotation):
    //   X = depth   (tiny — invisible to camera)
    //   Y = width
    //   Z = height
    const FVector IconOutlineExtent(0.5f, 16.f, 16.f);
    const FVector IconFillExtent   (1.0f, 14.f, 14.f);  // slightly thicker so it pokes through outline

    DrawDebugBox     (GetWorld(), IconCenter, IconOutlineExtent, BillboardQuat,
                      IconEdgeColor, false, -1.f, 0, 1.5f);
    DrawDebugSolidBox(GetWorld(), IconCenter, IconFillExtent,    BillboardQuat,
                      IconFillColor, false, -1.f, 0);

    // "Q" letter, DrawDebugString billboards automatically.
    DrawDebugString(GetWorld(), IconCenter, TEXT("Q"),
                    nullptr, FColor::White, 0.f, true, 1.4f);

    // Progress bar — only while the player is channeling THIS encounter.
    if (const AExplorationPawn* PlayerPawn = Cast<AExplorationPawn>(Player))
    {
        if (PlayerPawn->IsAssassinating() && PlayerPawn->GetAssassinationTarget() == this)
        {
            const float Pct = PlayerPawn->GetAssassinationProgress();

            const float   BarMaxHalfLen = 22.f;
            const float   BarHalfLen    = BarMaxHalfLen * Pct;
            const FVector BarFullExtent (0.5f, BarMaxHalfLen, 4.f);
            const FVector BarFillExtent (1.0f, BarHalfLen,    4.f);

            // Left-anchored fill — shift filled plate so its LEFT edge sits at
            // the outline's left edge regardless of fill amount.
            const FVector LocalLeftOffset(0.f, -(BarMaxHalfLen - BarHalfLen), 0.f);
            const FVector WorldLeftOffset = BillboardQuat.RotateVector(LocalLeftOffset);
            const FVector FilledCenter    = BarCenter + WorldLeftOffset;

            DrawDebugBox     (GetWorld(), BarCenter,    BarFullExtent, BillboardQuat,
                              FColor::White, false, -1.f, 0, 1.f);
            DrawDebugSolidBox(GetWorld(), FilledCenter, BarFillExtent, BillboardQuat,
                              FColor(60, 240, 90), false, -1.f, 0);
        }
    }
}
#endif

// -----------------------------------------------------------------------------
//  Chase
// -----------------------------------------------------------------------------

void AEnemyEncounter::HandleDetectionFull(AEnemyEncounter* /*DetectingEncounter*/)
{
    if (bIsChasing) { return; }  // already chasing — ignore re-trigger

    bIsChasing           = true;
    bIsReturningToHome   = false;
    LostSightTimer       = 0.f;

    // Chase supersedes investigate — clear those flags so we don't try to
    // resume looking-around when the chase ends.
    bIsInvestigating     = false;
    InvestigateStage     = 0;
    InvestigateLookTimer = 0.f;

    UE_LOG(LogTemp, Warning, TEXT("[EnemyEncounter] %s started CHASING the player."),
        *GetName());

    // Push global chase flag → DangerManager pauses the decay timer while any
    // encounter is actively chasing.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
        {
            DM->SetChaseActive(true);
        }
    }
}

void AEnemyEncounter::StopChase()
{
    if (!bIsChasing) { return; }

    bIsChasing         = false;
    bIsReturningToHome = true;   // walk back to spawn
    LostSightTimer     = 0.f;

    // Detection component re-arms its meter — drop to zero so the enemy has
    // to see the player fresh again before another chase starts.
    if (Detection)
    {
        Detection->DetectionMeter   = 0.f;
        Detection->bAlertedThisLife = false;
        Detection->bPlayerVisible   = false;
    }

    UE_LOG(LogTemp, Warning, TEXT("[EnemyEncounter] %s gave up chasing — returning home."),
        *GetName());

    // Clear global chase flag so DangerManager resumes its decay timer.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
        {
            DM->SetChaseActive(false);
        }
    }
}

void AEnemyEncounter::TickChase(float DeltaTime)
{
    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!PlayerPawn) { return; }

    const FVector MyLoc     = GetActorLocation();
    const FVector PlayerLoc = PlayerPawn->GetActorLocation();

    // Hard distance cap — escaped too far from home → give up.
    if (FVector::Dist(HomeLocation, PlayerLoc) > ChaseMaxRange)
    {
        StopChase();
        return;
    }

    // Lost-sight timer — if Detection can't currently see the player, accumulate
    // toward giving up. Reset on any LOS contact.
    if (Detection && !Detection->bPlayerVisible)
    {
        LostSightTimer += DeltaTime;
        if (LostSightTimer >= ChaseGiveUpSeconds)
        {
            StopChase();
            return;
        }
    }
    else
    {
        LostSightTimer = 0.f;
    }

    // Chase speed scales with danger level so high danger = harder to outrun.
    float SpeedMult = 1.f;
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
        {
            SpeedMult = DM->GetChaseSpeedMultiplier();
        }
    }

    // Path-following — falls back to direct translate if no nav mesh.
    MoveActorTowardTarget(PlayerLoc, ChaseSpeed * SpeedMult, DeltaTime);

#if !UE_BUILD_SHIPPING
    // Red marker above the encounter while chasing — gives an at-a-glance read
    // separate from the detection meter (which can be hidden during the chase).
    DrawDebugString(GetWorld(), MyLoc + FVector(0.f, 0.f, 280.f),
        TEXT("CHASE"), nullptr, FColor::Red, 0.f, true, 1.4f);
#endif
}

void AEnemyEncounter::TickReturnToHome(float DeltaTime)
{
    const FVector MyLoc = GetActorLocation();

    // Pick target: if we have a patrol route, head back to the nearest waypoint
    // and resume patrolling from there. Otherwise just go back to HomeLocation.
    FVector ReturnTarget = HomeLocation;
    int32   SnapToIndex  = -1;

    if (HasPatrolRoute())
    {
        float BestDistSq = TNumericLimits<float>::Max();
        for (int32 i = 0; i < PatrolOffsets.Num(); ++i)
        {
            const FVector WP = GetPatrolTargetWorld(i);
            const float DSq = FVector::DistSquared(MyLoc, WP);
            if (DSq < BestDistSq) { BestDistSq = DSq; SnapToIndex = i; ReturnTarget = WP; }
        }
    }

    // Path-following toward the home/waypoint target.
    if (MoveActorTowardTarget(ReturnTarget, ReturnSpeed, DeltaTime))
    {
        bIsReturningToHome = false;
        InvalidateNavPath();

        if (SnapToIndex >= 0)
        {
            CurrentPatrolIndex = SnapToIndex;
            PatrolWaitTimer    = PatrolWaitSecondsAtWaypoint;
            UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s resumed patrol at waypoint %d."),
                *GetName(), SnapToIndex);
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s returned to patrol home."),
                *GetName());
        }
        return;
    }

#if !UE_BUILD_SHIPPING
    DrawDebugString(GetWorld(), MyLoc + FVector(0.f, 0.f, 280.f),
        TEXT("RETURNING"), nullptr, FColor::Yellow, 0.f, true, 1.2f);
#endif
}

// -----------------------------------------------------------------------------
//  Patrol
// -----------------------------------------------------------------------------

FVector AEnemyEncounter::GetPatrolTargetWorld(int32 Index) const
{
    if (!PatrolOffsets.IsValidIndex(Index)) { return HomeLocation; }
    return HomeLocation + PatrolOffsets[Index];
}

void AEnemyEncounter::TickPatrol(float DeltaTime)
{
    if (!HasPatrolRoute()) { return; }

    // Pause at waypoint
    if (PatrolWaitTimer > 0.f)
    {
        PatrolWaitTimer -= DeltaTime;
#if !UE_BUILD_SHIPPING
        DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 260.f),
            TEXT("WAITING"), nullptr, FColor(80, 200, 255), 0.f, true, 1.1f);
#endif
        return;
    }

    const FVector MyLoc  = GetActorLocation();
    const FVector Target = GetPatrolTargetWorld(CurrentPatrolIndex);

    if (MoveActorTowardTarget(Target, PatrolSpeed, DeltaTime))
    {
        // Arrived at waypoint — pause, advance index, drop the path.
        PatrolWaitTimer    = PatrolWaitSecondsAtWaypoint;
        CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolOffsets.Num();
        InvalidateNavPath();
        return;
    }

#if !UE_BUILD_SHIPPING
    DrawDebugString(GetWorld(), MyLoc + FVector(0.f, 0.f, 260.f),
        FString::Printf(TEXT("PATROL → WP %d"), CurrentPatrolIndex),
        nullptr, FColor(80, 200, 255), 0.f, true, 1.1f);
    // Draw a faint line route between waypoints so designers can see the path
    for (int32 i = 0; i < PatrolOffsets.Num(); ++i)
    {
        const FVector A = GetPatrolTargetWorld(i);
        const FVector B = GetPatrolTargetWorld((i + 1) % PatrolOffsets.Num());
        DrawDebugLine(GetWorld(), A + FVector(0.f, 0.f, 10.f), B + FVector(0.f, 0.f, 10.f),
                      FColor(40, 120, 200), false, -1.f, 0, 1.f);
        DrawDebugSphere(GetWorld(), A, 30.f, 12, FColor(40, 120, 200), false, -1.f, 0, 1.f);
    }
#endif
}

// -----------------------------------------------------------------------------
//  Wander — automatic random patrol around HomeLocation
// -----------------------------------------------------------------------------

void AEnemyEncounter::PickRandomWanderTarget()
{
    if (WanderRadius <= 0.f)
    {
        WanderTarget     = HomeLocation;
        bHasWanderTarget = true;
        return;
    }

    // Prefer a navigation-validated reachable point — guarantees the target is
    // on the nav mesh and actually reachable from HomeLocation.
    if (UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld()))
    {
        FNavLocation Result;
        if (NavSys->GetRandomReachablePointInRadius(HomeLocation, WanderRadius, Result))
        {
            WanderTarget     = Result.Location;
            bHasWanderTarget = true;
            return;
        }
    }

    // Fallback — random point inside a disc around HomeLocation. May or may
    // not be reachable, but with no nav mesh the move helper translates
    // directly anyway.
    const float Angle  = FMath::FRandRange(0.f, 2.f * PI);
    const float Radius = FMath::FRandRange(WanderRadius * 0.25f, WanderRadius);
    WanderTarget = HomeLocation + FVector(FMath::Cos(Angle) * Radius,
                                          FMath::Sin(Angle) * Radius, 0.f);
    bHasWanderTarget = true;
}

void AEnemyEncounter::TickWander(float DeltaTime)
{
    // Pause between wander legs — sit still so the player has a stationary
    // window to assassinate.
    if (WanderPauseTimer > 0.f)
    {
        WanderPauseTimer -= DeltaTime;
#if !UE_BUILD_SHIPPING
        DrawDebugString(GetWorld(), GetActorLocation() + FVector(0.f, 0.f, 260.f),
            TEXT("IDLE"), nullptr, FColor(120, 160, 200), 0.f, true, 1.0f);
#endif
        return;
    }

    if (!bHasWanderTarget) { PickRandomWanderTarget(); }

    const FVector MyLoc = GetActorLocation();

    if (MoveActorTowardTarget(WanderTarget, PatrolSpeed, DeltaTime))
    {
        // Arrived — pause for a random interval before picking a new target.
        WanderPauseTimer = FMath::FRandRange(FMath::Max(WanderPauseMin, 0.f),
                                             FMath::Max(WanderPauseMax, WanderPauseMin));
        bHasWanderTarget = false;
        InvalidateNavPath();
        return;
    }

#if !UE_BUILD_SHIPPING
    DrawDebugString(GetWorld(), MyLoc + FVector(0.f, 0.f, 260.f),
        TEXT("WANDER"), nullptr, FColor(120, 160, 200), 0.f, true, 1.0f);
    DrawDebugSphere(GetWorld(), WanderTarget, 20.f, 8,
                    FColor(80, 200, 255), false, -1.f, 0, 1.f);
#endif
}

// -----------------------------------------------------------------------------
//  Investigate — partial-detection "go check it out" state
// -----------------------------------------------------------------------------

void AEnemyEncounter::StartInvestigation()
{
    bIsInvestigating     = true;
    InvestigateStage     = 0;          // walking toward last-seen
    InvestigateLookTimer = 0.f;

    // Reset any current wander/patrol pause so the encounter starts moving
    // immediately rather than waiting out a stale timer.
    WanderPauseTimer = 0.f;
    PatrolWaitTimer  = 0.f;

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s INVESTIGATING (detect %.0f%%)"),
        *GetName(), Detection ? Detection->DetectionMeter * 100.f : 0.f);
}

void AEnemyEncounter::TickInvestigate(float DeltaTime)
{
    const FVector MyLoc = GetActorLocation();

    if (InvestigateStage == 0)  // walking to last-seen
    {
        if (MoveActorTowardTarget(LastSeenPlayerLocation,
                                  ChaseSpeed * InvestigateSpeedScale,
                                  DeltaTime,
                                  /*ArriveDistance=*/80.f))
        {
            // Arrived — switch to look-around stage.
            InvestigateStage     = 1;
            InvestigateLookTimer = 0.f;
            InvalidateNavPath();
        }
    }
    else  // stage 1 — looking around at the suspected spot
    {
        InvestigateLookTimer += DeltaTime;

        // Sweep the yaw back and forth so the detection cone covers ground —
        // a +/- 60° sinusoid around the original arrival yaw.
        const float SweepHz   = 1.0f / FMath::Max(InvestigateLookDuration, 0.5f);
        const float Phase     = InvestigateLookTimer * SweepHz * 2.f * PI;
        const float YawOffset = FMath::Sin(Phase) * 60.f;
        FRotator Rot = GetActorRotation();
        Rot.Yaw += YawOffset * DeltaTime * 4.f;  // light incremental sweep
        SetActorRotation(Rot);

        if (InvestigateLookTimer >= InvestigateLookDuration)
        {
            // Done — drop investigation flag. Patrol/Wander resumes next tick.
            bIsInvestigating     = false;
            InvestigateStage     = 0;
            InvestigateLookTimer = 0.f;

            // If detection has dropped, also clear last-seen so we don't keep
            // re-entering investigate on the same stale spot.
            if (Detection && Detection->DetectionMeter < InvestigateDetectionThreshold)
            {
                bHasLastSeen = false;
            }
        }
    }

#if !UE_BUILD_SHIPPING
    DrawDebugString(GetWorld(), MyLoc + FVector(0.f, 0.f, 260.f),
        InvestigateStage == 0 ? TEXT("INVESTIGATING") : TEXT("LOOKING..."),
        nullptr, FColor(255, 180, 60), 0.f, true, 1.1f);
    DrawDebugSphere(GetWorld(), LastSeenPlayerLocation, 30.f, 12,
                    FColor(255, 180, 60), false, -1.f, 0, 1.5f);
#endif
}

// -----------------------------------------------------------------------------
//  Assassination
// -----------------------------------------------------------------------------

EAssassinationStatus AEnemyEncounter::GetAssassinationStatus(APawn* Player) const
{
    if (!Player)        { return EAssassinationStatus::NoPlayer; }
    if (bIsChasing)     { return EAssassinationStatus::Chasing;  }

    // Any active suspicion disqualifies stealth — meter past 5% counts.
    if (Detection && (Detection->bPlayerVisible || Detection->DetectionMeter > 0.05f))
    {
        return EAssassinationStatus::Alerted;
    }

    const FVector MyLoc     = GetActorLocation();
    const FVector PlayerLoc = Player->GetActorLocation();
    if (FVector::Dist(MyLoc, PlayerLoc) > AssassinationRange)
    {
        return EAssassinationStatus::OutOfRange;
    }

    // Behind check — player must be in the rear hemisphere of the encounter.
    FVector ToPlayer = PlayerLoc - MyLoc;
    ToPlayer.Z = 0.f;
    if (ToPlayer.IsNearlyZero()) { return EAssassinationStatus::NotBehind; }
    ToPlayer.Normalize();

    const float Dot          = FVector::DotProduct(GetActorForwardVector(), ToPlayer);
    const float CosThreshold = -FMath::Cos(FMath::DegreesToRadians(AssassinationBehindHalfAngleDeg));
    return (Dot <= CosThreshold) ? EAssassinationStatus::Ready : EAssassinationStatus::NotBehind;
}

bool AEnemyEncounter::CanBeAssassinated(APawn* Player) const
{
    return GetAssassinationStatus(Player) == EAssassinationStatus::Ready;
}

bool AEnemyEncounter::IsOverleveledForAssassination() const
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
        {
            return DM->GetPlayerEffectiveLevel() >= EncounterLevel + DM->AssassinationLevelGap;
        }
    }
    return false;
}

void AEnemyEncounter::Assassinate(APawn* /*Attacker*/)
{
    UE_LOG(LogTemp, Warning,
        TEXT("[EnemyEncounter] %s ASSASSINATED — instant kill, no combat. (EncounterLvl=%d)"),
        *GetName(), EncounterLevel);

    // Per pitch: "XP severely reduced + only the attacker gets it".
    // We sum the would-be XP from this encounter's classes (read from each
    // CDO's default XPReward) and grant 25% to the lead party member (slot
    // 0). The rest of the party gets nothing. Recompute aggregate level so
    // the danger / overlevel systems stay in sync.
    int32 TotalXP = 0;
    for (TSubclassOf<ACombatantBase> EnemyClass : EnemyClasses)
    {
        if (!EnemyClass) { continue; }
        if (const AEnemyCombatant* CDO = EnemyClass->GetDefaultObject<AEnemyCombatant>())
        {
            TotalXP += FMath::Max(0, CDO->XPReward);
        }
    }

    const int32 ReducedXP = FMath::FloorToInt(TotalXP * 0.25f);

    if (AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        const TArray<ACombatantBase*>& Party = GM->GetPlayerParty();

        if (ReducedXP > 0 && Party.Num() > 0)
        {
            if (APlayerCombatant* Lead = Cast<APlayerCombatant>(Party[0]))
            {
                UE_LOG(LogTemp, Log,
                    TEXT("[EnemyEncounter] Assassination XP %d (25%% of %d) → %s only"),
                    ReducedXP, TotalXP,
                    Lead->DisplayName.IsEmpty() ? *Lead->GetName() : *Lead->DisplayName.ToString());
                Lead->GrantXP(ReducedXP);
            }
        }

        // Recompute aggregate level in case Lead leveled up.
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
            {
                int32 MaxLevel = 1;
                for (ACombatantBase* C : Party)
                {
                    if (APlayerCombatant* P = Cast<APlayerCombatant>(C))
                    {
                        MaxLevel = FMath::Max(MaxLevel, P->Level);
                    }
                }
                DM->SetPlayerEffectiveLevel(MaxLevel);
            }
        }
    }

    Destroy();
}

void AEnemyEncounter::Stun(float Duration)
{
    if (Duration <= 0.f) { return; }

    // Refresh — if already stunned just extend if the new duration is longer.
    StunRemaining = FMath::Max(StunRemaining, Duration);
    bIsStunned    = true;

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s stunned for %.1fs"),
        *GetName(), StunRemaining);
}

void AEnemyEncounter::TriggerCombat(bool bPlayerHasInitiative)
{
    AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM)
    {
        UE_LOG(LogTemp, Error, TEXT("[EnemyEncounter] No AJrpgGameMode — can't start combat."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s triggering combat. PlayerInitiative=%s"),
        *GetName(), bPlayerHasInitiative ? TEXT("true") : TEXT("false"));

    GM->BeginEncounter(this, bPlayerHasInitiative);
}

// -----------------------------------------------------------------------------
//  Navigation path-following
// -----------------------------------------------------------------------------

void AEnemyEncounter::InvalidateNavPath()
{
    CurrentPathPoints.Reset();
    CurrentPathIndex      = 0;
    CachedPathTarget      = FVector::ZeroVector;
    NavPathRefreshTimer   = 0.f;
}

bool AEnemyEncounter::RefreshNavPath(const FVector& Target, float DeltaTime)
{
    NavPathRefreshTimer -= DeltaTime;

    const bool bTargetDrifted =
        CurrentPathPoints.Num() == 0
        || FVector::DistSquared(Target, CachedPathTarget)
               > NavPathTargetDriftThreshold * NavPathTargetDriftThreshold;
    const bool bTimedOut = NavPathRefreshTimer <= 0.f;

    if (!bTargetDrifted && !bTimedOut) { return CurrentPathPoints.Num() > 0; }

    UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys)
    {
        // No nav system at all — let caller fall back to direct movement.
        return false;
    }

    UNavigationPath* Path = NavSys->FindPathToLocationSynchronously(
        this, GetActorLocation(), Target, /*PathfindingContext=*/this);

    if (!Path || !Path->IsValid() || Path->IsPartial() && Path->PathPoints.Num() < 2)
    {
        // Couldn't find any usable path — bail. Caller falls back to direct.
        CurrentPathPoints.Reset();
        return false;
    }

    CurrentPathPoints   = Path->PathPoints;
    CurrentPathIndex    = 1;   // [0] is our current location
    CachedPathTarget    = Target;
    NavPathRefreshTimer = NavPathRefreshInterval;
    return true;
}

bool AEnemyEncounter::MoveActorTowardTarget(const FVector& Target, float Speed,
                                            float DeltaTime, float ArriveDistance)
{
    const FVector MyLoc = GetActorLocation();

    // Try the nav system first — if it succeeds, walk corner by corner.
    if (RefreshNavPath(Target, DeltaTime) && CurrentPathPoints.IsValidIndex(CurrentPathIndex))
    {
        FVector NextCorner = CurrentPathPoints[CurrentPathIndex];
        FVector ToCorner   = NextCorner - MyLoc;
        ToCorner.Z = 0.f;
        float DistXY = ToCorner.Size();

        // Reached the corner — advance to the next one.
        if (DistXY < 50.f && CurrentPathIndex < CurrentPathPoints.Num() - 1)
        {
            ++CurrentPathIndex;
            NextCorner = CurrentPathPoints[CurrentPathIndex];
            ToCorner   = NextCorner - MyLoc;
            ToCorner.Z = 0.f;
            DistXY     = ToCorner.Size();
        }

        if (DistXY > KINDA_SMALL_NUMBER)
        {
            const FVector Dir  = ToCorner / DistXY;
            const float   Step = FMath::Min(Speed * DeltaTime, DistXY);
            SetActorLocation(FVector(MyLoc.X + Dir.X * Step,
                                     MyLoc.Y + Dir.Y * Step, MyLoc.Z));
            SetActorRotation(Dir.Rotation());
        }

        // Final-arrival check uses the actual Target, not the last corner —
        // the path endpoint is sometimes slightly inside the nav surface.
        FVector ToFinal = Target - GetActorLocation();
        ToFinal.Z = 0.f;
        return ToFinal.SizeSquared() < ArriveDistance * ArriveDistance;
    }

    // ---- Fallback: direct translate (no nav mesh) --------------------------
    FVector ToTarget = Target - MyLoc;
    ToTarget.Z = 0.f;
    const float DistXY = ToTarget.Size();

    if (DistXY < ArriveDistance) { return true; }

    const FVector Dir  = ToTarget / DistXY;
    const float   Step = FMath::Min(Speed * DeltaTime, DistXY);
    SetActorLocation(FVector(MyLoc.X + Dir.X * Step, MyLoc.Y + Dir.Y * Step, MyLoc.Z));
    SetActorRotation(Dir.Rotation());
    return false;
}

// -----------------------------------------------------------------------------
//  Combat-time freeze + visibility
// -----------------------------------------------------------------------------

void AEnemyEncounter::SetExplorationActive(bool bActive)
{
    // Stop running AI, hide visuals, kill collisions so the player can't trigger
    // a new fight by walking through where this encounter happens to be.
    SetActorTickEnabled(bActive);

    // SetActorHiddenInGame hides ALL primitive components (mesh + any future
    // visible children) and also short-circuits debug-draw calls — broader
    // than Mesh->SetVisibility, which only touches the static mesh.
    SetActorHiddenInGame(!bActive);

    if (TriggerSphere)
    {
        TriggerSphere->SetGenerateOverlapEvents(bActive);
        TriggerSphere->SetCollisionEnabled(bActive
            ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
    }
    if (Detection)
    {
        // Pause detection ticking entirely — no sense filling a meter we can't see.
        Detection->SetComponentTickEnabled(bActive);
    }

    // If this encounter was mid-chase/mid-investigate when combat started,
    // clear those flags too — so when ResetToSpawn runs on battle end, the
    // encounter is fully clean (otherwise a leftover bIsChasing could push
    // SetChaseActive(true) on the danger manager via a stale tick).
    if (!bActive)
    {
        bIsChasing           = false;
        bIsReturningToHome   = false;
        bIsInvestigating     = false;
        bHasWanderTarget     = false;
    }
}

void AEnemyEncounter::ResetToSpawn()
{
    // Teleport back to spawn and clear EVERY transient AI flag — chase, return,
    // patrol, wander, investigate, alert. The encounter should look like it
    // just spawned for the first time.
    SetActorLocation(HomeLocation);

    bIsChasing           = false;
    bIsReturningToHome   = false;
    LostSightTimer       = 0.f;

    bIsInvestigating     = false;
    InvestigateStage     = 0;
    InvestigateLookTimer = 0.f;
    bHasLastSeen         = false;

    bHasWanderTarget     = false;
    WanderPauseTimer     = 0.f;
    CurrentPatrolIndex   = 0;
    PatrolWaitTimer      = 0.f;

    bIsStunned           = false;
    StunRemaining        = 0.f;

    InvalidateNavPath();

    if (Detection)
    {
        Detection->DetectionMeter   = 0.f;
        Detection->bAlertedThisLife = false;
        Detection->bPlayerVisible   = false;
    }

    // Clear any global chase flag this encounter might have set on the danger
    // manager — defensive, since chase should already be over by now.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
        {
            DM->SetChaseActive(false);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s reset to spawn."), *GetName());
}

void AEnemyEncounter::HandleTriggerOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
                                            AActor* OtherActor,
                                            UPrimitiveComponent* /*OtherComp*/,
                                            int32 /*OtherBodyIndex*/,
                                            bool /*bFromSweep*/,
                                            const FHitResult& /*SweepResult*/)
{
    // Only react to the player's exploration pawn, not arbitrary actors.
    if (!Cast<AExplorationPawn>(OtherActor)) { return; }

    // Initiative rules per pitch (Outside Combat → Stealth):
    //   - Walked into a STUNNED encounter → player initiative (gun setup)
    //   - Caught while being CHASED       → enemy initiative (alerted ambush)
    //   - Default contact (no flags)      → enemy initiative (standard ambush)
    // The only way to get player initiative is the cone-shot path (handled
    // separately) or stunning the encounter with a gun shot first.
    const bool bPlayerInitiative = bIsStunned && !bIsChasing;

    UE_LOG(LogTemp, Log,
        TEXT("[EnemyEncounter] %s triggered by %s (stunned=%s chasing=%s) → playerInit=%s"),
        *GetName(), *OtherActor->GetName(),
        bIsStunned  ? TEXT("true") : TEXT("false"),
        bIsChasing  ? TEXT("true") : TEXT("false"),
        bPlayerInitiative ? TEXT("true") : TEXT("false"));

    TriggerCombat(bPlayerInitiative);
}
