#include "Exploration/EnemyEncounter.h"
#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyDetectionComponent.h"
#include "Exploration/DetectionMeterWidget.h"
#include "Core/DangerManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameInstance.h"
#include "DrawDebugHelpers.h"

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

    // -------------------------------------------------------------------------
    //  Detection meter UI (Phase B2) — world-space widget floating overhead,
    //  billboarded toward the camera. Class is assigned in BP defaults
    //  (DetectionMeterWidgetClass = WBP_DetectionMeter).
    // -------------------------------------------------------------------------

    DetectionMeterComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DetectionMeter"));
    DetectionMeterComponent->SetupAttachment(RootComponent);
    DetectionMeterComponent->SetRelativeLocation(FVector(0.f, 0.f, DetectionMeterHeight));

    // World space is far more reliable than Screen — Screen mode has quirky
    // viewport/scale rendering issues that can leave the widget invisible.
    // World space renders as a 3D plane that we ticker-rotate to face the camera.
    DetectionMeterComponent->SetWidgetSpace(EWidgetSpace::World);
    DetectionMeterComponent->SetDrawSize(DetectionMeterDrawSize);

    // Scale down the 3D plane so 200px of widget ≈ 100cm wide in world space.
    DetectionMeterComponent->SetRelativeScale3D(FVector(0.5f));
    DetectionMeterComponent->SetTwoSided(true);  // visible from either side
    DetectionMeterComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DetectionMeterComponent->SetGenerateOverlapEvents(false);
    DetectionMeterComponent->SetVisibility(true);
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

    // Apply BP-tuned values to the widget component (constructor uses defaults
    // before BP overrides land).
    if (DetectionMeterComponent)
    {
        DetectionMeterComponent->SetRelativeLocation(FVector(0.f, 0.f, DetectionMeterHeight));
        DetectionMeterComponent->SetDrawSize(DetectionMeterDrawSize);

        // Assign the WBP class and bind the widget to our detection component.
        if (DetectionMeterWidgetClass)
        {
            DetectionMeterComponent->SetWidgetClass(DetectionMeterWidgetClass);

            // Force the user widget to spawn now so we can bind it. Without this,
            // GetUserWidgetObject() returns nullptr until the first tick.
            DetectionMeterComponent->InitWidget();

            if (UDetectionMeterWidget* Meter =
                    Cast<UDetectionMeterWidget>(DetectionMeterComponent->GetUserWidgetObject()))
            {
                Meter->BindToDetection(Detection);
                UE_LOG(LogTemp, Warning, TEXT("[EnemyEncounter] %s: meter widget spawned and bound."),
                    *GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[EnemyEncounter] %s: DetectionMeterWidgetClass is set but does not derive from UDetectionMeterWidget — meter will not update."),
                    *GetName());
            }
        }
        else
        {
            // No widget class assigned in BP — hide the component entirely so
            // we don't render an empty 2D plate.
            UE_LOG(LogTemp, Warning,
                TEXT("[EnemyEncounter] %s: DetectionMeterWidgetClass is NULL — assign WBP_DetectionMeter in BP defaults."),
                *GetName());
            DetectionMeterComponent->SetVisibility(false);
        }
    }
}

void AEnemyEncounter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Billboard the detection meter toward the player camera so the bar is
    // always legible regardless of where the camera is. World-space widgets
    // need this manually; Screen-space did it automatically but had other
    // rendering issues.
    if (DetectionMeterComponent && DetectionMeterComponent->IsVisible())
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            FVector CamLoc;
            FRotator CamRot;
            PC->GetPlayerViewPoint(CamLoc, CamRot);
            const FVector ToCam = CamLoc - DetectionMeterComponent->GetComponentLocation();
            const FRotator FaceRot = ToCam.Rotation() + FRotator(0.f, 180.f, 0.f);
            DetectionMeterComponent->SetWorldRotation(FaceRot);
        }
    }

    // Chase / return-to-home overrides patrol/stun — handled before stun decay
    // because a chasing encounter that gets stunned should still tick the stun
    // timer but not pursue. Stun cancels chase.
    if (bIsStunned)
    {
        StunRemaining -= DeltaTime;
        if (StunRemaining <= 0.f)
        {
            bIsStunned    = false;
            StunRemaining = 0.f;
            UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s recovered from stun."), *GetName());
        }
        return;  // stunned enemies don't chase
    }

    if (bIsChasing)
    {
        TickChase(DeltaTime);
    }
    else if (bIsReturningToHome)
    {
        TickReturnToHome(DeltaTime);
    }
    else if (HasPatrolRoute())
    {
        TickPatrol(DeltaTime);
    }

#if !UE_BUILD_SHIPPING
    // Assassination range/behind viz — only shown when player is close, so
    // distant encounters don't clutter the screen. Drawn every tick (1-frame
    // lifetime) so it color-changes live as the player moves.
    DrawAssassinationViz();
#endif
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

    bIsChasing         = true;
    bIsReturningToHome = false;
    LostSightTimer     = 0.f;

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

    // Move toward the player on the XY plane (preserve our Z so we don't
    // sink into the ground or fly up at the player's eye height).
    FVector ToPlayer = PlayerLoc - MyLoc;
    ToPlayer.Z = 0.f;
    const float DistXY = ToPlayer.Size();
    if (DistXY > KINDA_SMALL_NUMBER)
    {
        const FVector Dir = ToPlayer / DistXY;

        // Chase speed scales with danger level so high danger = harder to outrun.
        float SpeedMult = 1.f;
        if (UGameInstance* GI = GetGameInstance())
        {
            if (UDangerManager* DM = GI->GetSubsystem<UDangerManager>())
            {
                SpeedMult = DM->GetChaseSpeedMultiplier();
            }
        }

        const float Step = ChaseSpeed * SpeedMult * DeltaTime;
        const FVector NewLoc(MyLoc.X + Dir.X * Step, MyLoc.Y + Dir.Y * Step, MyLoc.Z);
        SetActorLocation(NewLoc);

        // Face the direction of travel so the static mesh visually points
        // toward the player. Yaw only — keeps the actor upright.
        SetActorRotation(Dir.Rotation());
    }

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

    FVector ToTarget = ReturnTarget - MyLoc;
    ToTarget.Z = 0.f;
    const float DistXY = ToTarget.Size();

    // Arrived (within 50cm)
    if (DistXY < 50.f)
    {
        SetActorLocation(FVector(ReturnTarget.X, ReturnTarget.Y, MyLoc.Z));
        bIsReturningToHome = false;

        if (SnapToIndex >= 0)
        {
            // Resume patrol from the waypoint we just landed on
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

    const FVector Dir = ToTarget / DistXY;
    const float Step  = ReturnSpeed * DeltaTime;
    const FVector NewLoc(MyLoc.X + Dir.X * Step, MyLoc.Y + Dir.Y * Step, MyLoc.Z);
    SetActorLocation(NewLoc);
    SetActorRotation(Dir.Rotation());

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
    FVector ToTarget = Target - MyLoc;
    ToTarget.Z = 0.f;
    const float DistXY = ToTarget.Size();

    if (DistXY < 50.f)
    {
        // Arrived at waypoint — pause, then advance index
        SetActorLocation(FVector(Target.X, Target.Y, MyLoc.Z));
        PatrolWaitTimer    = PatrolWaitSecondsAtWaypoint;
        CurrentPatrolIndex = (CurrentPatrolIndex + 1) % PatrolOffsets.Num();
        return;
    }

    const FVector Dir = ToTarget / DistXY;
    const float Step  = PatrolSpeed * DeltaTime;
    SetActorLocation(FVector(MyLoc.X + Dir.X * Step, MyLoc.Y + Dir.Y * Step, MyLoc.Z));
    SetActorRotation(Dir.Rotation());

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

    // TODO: award reduced XP to attacker only — needs XP system first.
    // For now we just destroy the encounter, matching the pitch's "resolves
    // the encounter immediately without entering combat".
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
