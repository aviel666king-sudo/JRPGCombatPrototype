#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "JrpgTravelSubsystem.generated.h"

class APawn;
class UWorld;

/**
 * UJrpgTravelSubsystem
 *
 * Single source of truth for "move the player to a different place." Every
 * portal, fast-travel button, and camp transition routes through here so the
 * "save where we came from / land at the right arrival point" logic is written
 * once.
 *
 * Two distinct kinds of travel:
 *
 *   1. Tag-based level travel — TravelToLevel(LevelName, ArrivalTag). The
 *      destination level must contain an ATravelArrivalPoint whose Tag matches
 *      ArrivalTag; on arrival the player pawn is teleported to that actor's
 *      transform. Used by portals (Level→OW, OW→Level) and by checkpoint
 *      "Leave to Open World".
 *
 *   2. Camp travel — EnterCampFromOpenWorld(Pawn) snapshots the pawn's current
 *      world transform, then opens L_Camp. LeaveCamp() opens L_Overworld and
 *      teleports the pawn back to that exact snapshot transform (overrides
 *      the arrival-tag flow). This is how the "Leave Camp" button on the camp
 *      checkpoint returns the player to the precise OW spot they entered from.
 *
 * Same-level teleport (TeleportPawn) is provided as a convenience for
 * checkpoint→checkpoint fast travel — no level open, just a SetActorLocation.
 *
 * Implementation note: the actual teleport happens on the next tick after
 * PostLoadMapWithWorld, because the game mode hasn't spawned the player pawn
 * yet at the moment the new world finishes loading.
 */
UCLASS()
class JRPGCOMBAT_API UJrpgTravelSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Open TargetLevel; once it finishes loading the player pawn is teleported
     *  to the ATravelArrivalPoint whose Tag matches ArrivalTag. NAME_None for
     *  ArrivalTag = leave the pawn wherever the game mode spawned it
     *  (PlayerStart fallback). */
    UFUNCTION(BlueprintCallable, Category = "Travel")
    void TravelToLevel(FName TargetLevel, FName ArrivalTag);

    /** Save the OW pawn's transform, then travel to the camp level. The "Leave
     *  Camp" button on the camp's checkpoint will return the player to this
     *  exact transform. */
    UFUNCTION(BlueprintCallable, Category = "Travel")
    void EnterCampFromOpenWorld(APawn* OWPlayerPawn);

    /** Travel back to the Open World level and teleport the player to the
     *  transform saved by the last EnterCampFromOpenWorld. No-op if no camp
     *  return is pending. */
    UFUNCTION(BlueprintCallable, Category = "Travel")
    void LeaveCamp();

    /** Same-level teleport — used by checkpoint→checkpoint fast travel. No
     *  OpenLevel, no fade. Pawn is reseated and the control rotation matches
     *  the destination yaw. */
    UFUNCTION(BlueprintCallable, Category = "Travel")
    void TeleportPawnTo(APawn* Pawn, const FTransform& Where);

    /** Name of the Open World level. Default matches the planned L_Overworld
     *  stub — adjust per project / set in BP defaults. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
    FName OpenWorldLevelName = TEXT("L_Overworld");

    /** Name of the single Camp level. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Travel")
    FName CampLevelName = TEXT("L_Camp");

    /** Default arrival tag used when entering Camp from OW. The camp level's
     *  ATravelArrivalPoint should match this. */
    static const FName CampArrivalTag;

private:

    /** Bound to FCoreUObjectDelegates::PostLoadMapWithWorld. Defers the
     *  arrival logic to the next tick so the pawn has time to spawn. */
    void HandlePostLoadMap(UWorld* World);

    /** Actually moves the pawn — either to the saved camp-return transform
     *  (if a camp return is pending) or to the arrival point matching the
     *  pending tag. Called once per arrival. */
    void ApplyPendingArrival(UWorld* World);

    /** Convenience — first local player's pawn in the given world, or null. */
    APawn* GetPlayerPawn(UWorld* World) const;

    /** Reset all pending-arrival fields. Called after every arrival. */
    void ClearPendingArrival();

    FName PendingArrivalTag = NAME_None;
    TOptional<FTransform> CampReturnTransform;
    bool bConsumeReturnTransformOnArrival = false;

    FDelegateHandle PostLoadMapHandle;
};
