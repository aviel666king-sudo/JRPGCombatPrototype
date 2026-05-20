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
}

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
    const FVector MyLoc  = GetActorLocation();
    FVector ToHome = HomeLocation - MyLoc;
    ToHome.Z = 0.f;
    const float DistXY = ToHome.Size();

    // Arrived (within 50cm)
    if (DistXY < 50.f)
    {
        SetActorLocation(FVector(HomeLocation.X, HomeLocation.Y, MyLoc.Z));
        bIsReturningToHome = false;
        UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s returned to patrol home."),
            *GetName());
        return;
    }

    const FVector Dir = ToHome / DistXY;
    const float Step  = ReturnSpeed * DeltaTime;
    const FVector NewLoc(MyLoc.X + Dir.X * Step, MyLoc.Y + Dir.Y * Step, MyLoc.Z);
    SetActorLocation(NewLoc);
    SetActorRotation(Dir.Rotation());

#if !UE_BUILD_SHIPPING
    DrawDebugString(GetWorld(), MyLoc + FVector(0.f, 0.f, 280.f),
        TEXT("RETURNING"), nullptr, FColor::Yellow, 0.f, true, 1.2f);
#endif
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
