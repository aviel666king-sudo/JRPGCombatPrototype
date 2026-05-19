#include "Exploration/EnemyDetectionComponent.h"
#include "Exploration/EnemyEncounter.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

// -----------------------------------------------------------------------------
//  Console var: r.JRPG.ShowEnemyVision <0|1>
// -----------------------------------------------------------------------------
static TAutoConsoleVariable<int32> CVarShowEnemyVision(
    TEXT("r.JRPG.ShowEnemyVision"),
    0,
    TEXT("Draw enemy vision cones, max-distance arcs, and detection meters in PIE.\n")
    TEXT("0 = off (default), 1 = on."),
    ECVF_Cheat);

// -----------------------------------------------------------------------------
//  Static stealth multiplier — one global value per world
// -----------------------------------------------------------------------------
//  Stored in a tiny per-world map so coexisting PIE worlds don't clobber each
//  other. Exploration pawn writes (1.0 standing, 0.5 crouched), every detection
//  component reads.

namespace
{
    static TMap<TWeakObjectPtr<UWorld>, float> GStealthByWorld;

    float GetStealthForWorld(UWorld* World)
    {
        if (!World) { return 1.f; }
        if (float* Found = GStealthByWorld.Find(World)) { return *Found; }
        return 1.f;
    }

    void SetStealthForWorld(UWorld* World, float Mult)
    {
        if (!World) { return; }
        GStealthByWorld.FindOrAdd(World) = FMath::Clamp(Mult, 0.f, 4.f);
    }
}

void UEnemyDetectionComponent::SetPlayerStealthMultiplier(UObject* WorldContextObject, float Multiplier)
{
    if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
    {
        SetStealthForWorld(World, Multiplier);
    }
}

float UEnemyDetectionComponent::GetPlayerStealthMultiplier(UObject* WorldContextObject)
{
    if (UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr)
    {
        return GetStealthForWorld(World);
    }
    return 1.f;
}

// -----------------------------------------------------------------------------
//  Component lifecycle
// -----------------------------------------------------------------------------

UEnemyDetectionComponent::UEnemyDetectionComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UEnemyDetectionComponent::BeginPlay()
{
    Super::BeginPlay();
    DetectionMeter   = 0.f;
    bPlayerVisible   = false;
    bAlertedThisLife = false;
}

// -----------------------------------------------------------------------------
//  Tick — fill / decay the meter, fire OnDetectionFull, debug draw
// -----------------------------------------------------------------------------

void UEnemyDetectionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (bAlertedThisLife) { return; }  // already fully detected — no point re-checking

    APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!PlayerPawn) { return; }

    const float StealthMult = GetStealthForWorld(GetWorld());

    bPlayerVisible = TestPlayerVisibility(PlayerPawn, StealthMult);

    const float FillRate    = 1.f / FMath::Max(0.1f, DetectionTimeSeconds);
    const float DecayRate   = FillRate * MeterDecayMultiplier;
    const float Delta       = bPlayerVisible ? FillRate * DeltaTime
                                             : -DecayRate * DeltaTime;

    DetectionMeter = FMath::Clamp(DetectionMeter + Delta, 0.f, 1.f);

    if (DetectionMeter >= 1.f && !bAlertedThisLife)
    {
        bAlertedThisLife = true;
        AEnemyEncounter* Owner = Cast<AEnemyEncounter>(GetOwner());
        UE_LOG(LogTemp, Log, TEXT("[Detection] %s detected the player — broadcasting alert."),
            Owner ? *Owner->GetName() : TEXT("?"));
        BroadcastAlert();
        OnDetectionFull.Broadcast(Owner);
    }

    if (CVarShowEnemyVision.GetValueOnGameThread() > 0)
    {
        DrawDebug(PlayerPawn, StealthMult);
    }

    // -------------------------------------------------------------------------
    //  ALWAYS-ON detection bar — drawn as a flat 3D plate that yaw-billboards
    //  toward the player camera so it always reads as a horizontal 2D bar.
    //  Bypasses UMG entirely. Length scales with meter, color shifts
    //  green → yellow → red.
    // -------------------------------------------------------------------------
    if (DetectionMeter > 0.01f && GetOwner())
    {
        const FVector OwnerLoc  = GetOwner()->GetActorLocation();
        const FVector BarCenter = OwnerLoc + FVector(0.f, 0.f, 220.f);

        // Bar dimensions in LOCAL space (before billboard rotation):
        //   X = depth (thin, facing camera)
        //   Y = length (this is the axis that fills)
        //   Z = height (thin, bar thickness)
        const float BarMaxHalfLen = 60.f;
        const float BarHalfLen    = BarMaxHalfLen * DetectionMeter;
        const FVector FullExtent  (3.f, BarMaxHalfLen, 8.f);
        const FVector FillExtent  (3.5f, BarHalfLen,   8.f);  // slightly thicker so it pokes through

        // Color: green → yellow → red
        FColor BarColor;
        if (DetectionMeter < 0.5f)
        {
            const float T = DetectionMeter / 0.5f;
            BarColor = FColor(uint8(255 * T), 220, 0);
        }
        else
        {
            const float T = (DetectionMeter - 0.5f) / 0.5f;
            BarColor = FColor(255, uint8(220 * (1.f - T)), 0);
        }

        // Billboard rotation — yaw only so the bar stays horizontal. Its local
        // X-axis points toward the player camera, so the Y-axis (bar length)
        // sits perpendicular to view = always readable.
        FQuat BarQuat = FQuat::Identity;
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            FVector CamLoc;
            FRotator CamRot;
            PC->GetPlayerViewPoint(CamLoc, CamRot);
            FVector ToCam = CamLoc - BarCenter;
            ToCam.Z = 0.f;
            if (!ToCam.IsNearlyZero())
            {
                BarQuat = ToCam.Rotation().Quaternion();
            }
        }

        // Left-anchored fill: shift the filled box so its left edge sits at the
        // outline's left edge regardless of fill amount. Offset is along the
        // bar's LOCAL Y axis, then rotated into world space by BarQuat.
        const FVector LocalLeftOffset(0.f, -(BarMaxHalfLen - BarHalfLen), 0.f);
        const FVector WorldLeftOffset = BarQuat.RotateVector(LocalLeftOffset);
        const FVector FilledCenter    = BarCenter + WorldLeftOffset;

        // Outline (white wireframe — full bar width, shows the max)
        DrawDebugBox(GetWorld(), BarCenter, FullExtent, BarQuat,
                     FColor::White, false, -1.f, 0, 1.f);

        // Filled portion (solid colored — grows left → right as meter fills)
        DrawDebugSolidBox(GetWorld(), FilledCenter, FillExtent, BarQuat,
                          BarColor, false, -1.f, 0);
    }
}

// -----------------------------------------------------------------------------
//  Visibility test — distance + cone + LOS
// -----------------------------------------------------------------------------

bool UEnemyDetectionComponent::TestPlayerVisibility(APawn* PlayerPawn, float StealthMult) const
{
    if (!PlayerPawn || !GetOwner()) { return false; }

    const FVector EyeLoc    = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const FVector PlayerLoc = PlayerPawn->GetActorLocation();
    const FVector ToPlayer  = PlayerLoc - EyeLoc;

    // 1. Distance check (scaled by player stealth)
    const float MaxDist = DetectionDistance * StealthMult;
    const float DistSq  = ToPlayer.SizeSquared();
    if (DistSq > MaxDist * MaxDist) { return false; }

    // 2. Cone check — angle between forward and direction-to-player
    const FVector Forward = GetOwner()->GetActorForwardVector();
    const FVector Dir     = ToPlayer.GetSafeNormal();
    if (Dir.IsNearlyZero()) { return false; }

    const float Cos       = FVector::DotProduct(Forward, Dir);
    const float CosLimit  = FMath::Cos(FMath::DegreesToRadians(DetectionConeHalfAngleDeg));
    if (Cos < CosLimit) { return false; }

    // 3. LOS trace — visibility channel, ignore self + player
    FCollisionQueryParams Params(SCENE_QUERY_STAT(EnemyDetectionLOS), false);
    Params.AddIgnoredActor(GetOwner());
    Params.AddIgnoredActor(PlayerPawn);

    FHitResult Hit;
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
        Hit, EyeLoc, PlayerLoc, ECC_Visibility, Params);

    return !bBlocked;  // no blocker between us → player is visible
}

// -----------------------------------------------------------------------------
//  One-hop alert broadcast
// -----------------------------------------------------------------------------

void UEnemyDetectionComponent::BroadcastAlert()
{
    if (!GetOwner() || !GetWorld()) { return; }

    const FVector MyLoc = GetOwner()->GetActorLocation();
    const float   R     = DetectionRadius;
    const float   RSq   = R * R;

    for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
    {
        AEnemyEncounter* Other = *It;
        if (!Other || Other == GetOwner()) { continue; }

        if (FVector::DistSquared(MyLoc, Other->GetActorLocation()) > RSq) { continue; }

        if (UEnemyDetectionComponent* Comp = Other->FindComponentByClass<UEnemyDetectionComponent>())
        {
            Comp->ReceiveAlert();
        }
    }
}

void UEnemyDetectionComponent::ReceiveAlert()
{
    if (bAlertedThisLife) { return; }   // ← one-hop guard: alerted enemies don't re-broadcast

    bAlertedThisLife = true;
    DetectionMeter   = 1.f;
    bPlayerVisible   = true;

    AEnemyEncounter* Owner = Cast<AEnemyEncounter>(GetOwner());
    UE_LOG(LogTemp, Log, TEXT("[Detection] %s alerted by neighbor."),
        Owner ? *Owner->GetName() : TEXT("?"));

    // Fire OnDetectionFull but DO NOT call BroadcastAlert — one hop only.
    OnDetectionFull.Broadcast(Owner);
}

// -----------------------------------------------------------------------------
//  Debug visualization
// -----------------------------------------------------------------------------

void UEnemyDetectionComponent::DrawDebug(APawn* PlayerPawn, float StealthMult) const
{
    if (!GetOwner() || !GetWorld()) { return; }

    const FVector EyeLoc  = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, EyeHeight);
    const FVector Forward = GetOwner()->GetActorForwardVector();
    const float   MaxDist = DetectionDistance * StealthMult;

    // Cone wedge — two edge rays + a few intermediates
    const FColor BaseCol  = bPlayerVisible ? FColor::Red
                          : (DetectionMeter > 0.f ? FColor::Yellow : FColor::Green);
    const float  HalfRad  = FMath::DegreesToRadians(DetectionConeHalfAngleDeg);

    constexpr int32 NumRays = 11;  // odd so center ray is included
    for (int32 i = 0; i < NumRays; ++i)
    {
        const float T = (NumRays > 1) ? (i / float(NumRays - 1)) * 2.f - 1.f : 0.f; // -1..+1
        const FRotator R(0.f, T * DetectionConeHalfAngleDeg, 0.f);
        const FVector  Edge = R.RotateVector(Forward) * MaxDist;
        DrawDebugLine(GetWorld(), EyeLoc, EyeLoc + Edge, BaseCol, false, -1.f, 0, 1.f);
    }

    // Detection radius (alert circle) — flat circle at owner Z
    DrawDebugCircle(GetWorld(),
        GetOwner()->GetActorLocation() + FVector(0.f, 0.f, 5.f),
        DetectionRadius,
        48,
        FColor(80, 80, 200),  // dim blue
        false, -1.f, 0, 1.f,
        FVector(1, 0, 0), FVector(0, 1, 0),
        false);

    // Meter fill — short bar above eye
    if (DetectionMeter > 0.f)
    {
        const FVector BarLoc = GetOwner()->GetActorLocation() + FVector(0.f, 0.f, EyeHeight + 50.f);
        DrawDebugString(GetWorld(), BarLoc,
            FString::Printf(TEXT("Detect %.0f%%"), DetectionMeter * 100.f),
            nullptr,
            FColor::White, 0.f, true, 1.2f);
    }
}
