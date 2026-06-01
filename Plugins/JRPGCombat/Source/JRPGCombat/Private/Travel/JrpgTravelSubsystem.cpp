#include "Travel/JrpgTravelSubsystem.h"
#include "Travel/TravelArrivalPoint.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"

const FName UJrpgTravelSubsystem::CampArrivalTag = TEXT("CampArrival");

// -----------------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------------

void UJrpgTravelSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this, &UJrpgTravelSubsystem::HandlePostLoadMap);
}

void UJrpgTravelSubsystem::Deinitialize()
{
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }
    Super::Deinitialize();
}

// -----------------------------------------------------------------------------
//  Public travel API
// -----------------------------------------------------------------------------

void UJrpgTravelSubsystem::TravelToLevel(FName TargetLevel, FName ArrivalTag)
{
    if (TargetLevel == NAME_None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Travel] TravelToLevel called with NAME_None target"));
        return;
    }

    PendingArrivalTag = ArrivalTag;
    bConsumeReturnTransformOnArrival = false;

    UE_LOG(LogTemp, Log, TEXT("[Travel] Opening level '%s' (arrival tag '%s')"),
        *TargetLevel.ToString(), *ArrivalTag.ToString());

    UGameplayStatics::OpenLevel(this, TargetLevel);
}

void UJrpgTravelSubsystem::EnterCampFromOpenWorld(APawn* OWPlayerPawn)
{
    if (OWPlayerPawn)
    {
        CampReturnTransform = OWPlayerPawn->GetActorTransform();
        UE_LOG(LogTemp, Log, TEXT("[Travel] Saved camp-return transform at %s"),
            *CampReturnTransform->GetLocation().ToString());
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Travel] EnterCampFromOpenWorld called without a pawn — no return transform will be saved"));
        CampReturnTransform.Reset();
    }

    PendingArrivalTag = CampArrivalTag;
    bConsumeReturnTransformOnArrival = false;
    UGameplayStatics::OpenLevel(this, CampLevelName);
}

void UJrpgTravelSubsystem::LeaveCamp()
{
    if (!CampReturnTransform.IsSet())
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Travel] LeaveCamp called but no camp-return transform is saved — falling back to default OW arrival"));
    }

    PendingArrivalTag = NAME_None;             // overridden by saved transform on arrival
    bConsumeReturnTransformOnArrival = CampReturnTransform.IsSet();
    UGameplayStatics::OpenLevel(this, OpenWorldLevelName);
}

void UJrpgTravelSubsystem::TeleportPawnTo(APawn* Pawn, const FTransform& Where)
{
    if (!Pawn) { return; }

    const FVector Location = Where.GetLocation();
    const FRotator Rotation = Where.GetRotation().Rotator();

    Pawn->SetActorLocationAndRotation(Location, Rotation);

    // Snap the control rotation too so the camera doesn't keep its old yaw.
    if (AController* C = Pawn->GetController())
    {
        C->SetControlRotation(Rotation);
    }
}

// -----------------------------------------------------------------------------
//  Arrival routing
// -----------------------------------------------------------------------------

void UJrpgTravelSubsystem::HandlePostLoadMap(UWorld* World)
{
    if (!World) { return; }

    // Defer one tick so the game mode has spawned the player pawn.
    TWeakObjectPtr<UWorld> WeakWorld(World);
    World->GetTimerManager().SetTimerForNextTick([this, WeakWorld]()
    {
        if (WeakWorld.IsValid())
        {
            ApplyPendingArrival(WeakWorld.Get());
        }
    });
}

void UJrpgTravelSubsystem::ApplyPendingArrival(UWorld* World)
{
    APawn* Pawn = GetPlayerPawn(World);

    // Pawn might genuinely not exist yet on the very first frame of some
    // game modes — try once more on the following tick.
    if (!Pawn)
    {
        TWeakObjectPtr<UWorld> WeakWorld(World);
        World->GetTimerManager().SetTimerForNextTick([this, WeakWorld]()
        {
            if (WeakWorld.IsValid())
            {
                if (APawn* P = GetPlayerPawn(WeakWorld.Get()))
                {
                    // Same logic, but inlined to avoid re-deferring further.
                    if (bConsumeReturnTransformOnArrival && CampReturnTransform.IsSet())
                    {
                        TeleportPawnTo(P, CampReturnTransform.GetValue());
                        CampReturnTransform.Reset();
                        ClearPendingArrival();
                        return;
                    }
                    if (PendingArrivalTag != NAME_None)
                    {
                        for (TActorIterator<ATravelArrivalPoint> It(WeakWorld.Get()); It; ++It)
                        {
                            if (*It && (*It)->Tag == PendingArrivalTag)
                            {
                                TeleportPawnTo(P, (*It)->GetActorTransform());
                                ClearPendingArrival();
                                return;
                            }
                        }
                    }
                    ClearPendingArrival();
                }
            }
        });
        return;
    }

    // Camp-leave path: saved transform wins over any arrival tag, and is
    // consumed once used so the next camp-entry overwrites it cleanly.
    if (bConsumeReturnTransformOnArrival && CampReturnTransform.IsSet())
    {
        TeleportPawnTo(Pawn, CampReturnTransform.GetValue());
        UE_LOG(LogTemp, Log, TEXT("[Travel] Restored camp-return transform"));
        CampReturnTransform.Reset();
        ClearPendingArrival();
        return;
    }

    // Tag-based arrival.
    if (PendingArrivalTag != NAME_None)
    {
        for (TActorIterator<ATravelArrivalPoint> It(World); It; ++It)
        {
            if (*It && (*It)->Tag == PendingArrivalTag)
            {
                TeleportPawnTo(Pawn, (*It)->GetActorTransform());
                UE_LOG(LogTemp, Log,
                    TEXT("[Travel] Arrived at tag '%s' (%s)"),
                    *PendingArrivalTag.ToString(), *(*It)->GetName());
                ClearPendingArrival();
                return;
            }
        }

        UE_LOG(LogTemp, Warning,
            TEXT("[Travel] No ATravelArrivalPoint with tag '%s' found in level '%s' — pawn left at default spawn"),
            *PendingArrivalTag.ToString(), *World->GetMapName());
    }

    ClearPendingArrival();
}

APawn* UJrpgTravelSubsystem::GetPlayerPawn(UWorld* World) const
{
    if (!World) { return nullptr; }
    if (APlayerController* PC = World->GetFirstPlayerController())
    {
        return PC->GetPawn();
    }
    return nullptr;
}

void UJrpgTravelSubsystem::ClearPendingArrival()
{
    // CampReturnTransform is intentionally NOT cleared here — the camp-leave
    // path resets it inline after teleporting. Keeping it intact across
    // unrelated travels means a player who travels Level→OW→Camp→OW returns
    // to the OW transform they had when entering camp.
    PendingArrivalTag = NAME_None;
    bConsumeReturnTransformOnArrival = false;
}
