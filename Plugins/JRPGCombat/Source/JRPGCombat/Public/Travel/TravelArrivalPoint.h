#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TravelArrivalPoint.generated.h"

class UBillboardComponent;
class UArrowComponent;

/**
 * ATravelArrivalPoint
 *
 * Designer-placeable spawn-destination marker. UJrpgTravelSubsystem looks up
 * the actor with a matching Tag at the moment a new level finishes loading
 * and teleports the player pawn to its transform (location + yaw).
 *
 * Convention for tag naming:
 *   - In a Level:       "FromOverworld"     — where the player lands when
 *                                              entering the Level from OW.
 *   - In Open World:    "Portal_<LevelName>" — where the player lands when
 *                                              leaving <LevelName> back to OW.
 *   - In Camp:          "CampArrival"        — where the player lands when
 *                                              entering camp via the OW keybind.
 *
 * Place one or more per level with unique tags. Editor-only billboard + arrow
 * make placement visible without affecting runtime cost.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API ATravelArrivalPoint : public AActor
{
    GENERATED_BODY()

public:

    ATravelArrivalPoint();

    /** Unique-per-level tag the travel subsystem matches against. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Travel")
    FName Tag;

#if WITH_EDITORONLY_DATA
    UPROPERTY()
    TObjectPtr<UBillboardComponent> Billboard;

    UPROPERTY()
    TObjectPtr<UArrowComponent> Arrow;
#endif
};
