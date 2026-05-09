#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ZoneInfo.generated.h"

/**
 * AZoneInfo
 *
 * Drop ONE of these into each overworld map. Holds the zone-wide level range
 * used by:
 *   - Assassination gating (weakest party member level >= ZoneLevelMin + 5)
 *   - Future encounter level rolls
 *
 * Convention: one AZoneInfo per map. JrpgGameMode caches the first one it
 * finds on BeginPlay.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBATTESTING_API AZoneInfo : public AActor
{
    GENERATED_BODY()

public:

    AZoneInfo();

    /** Lowest enemy level encountered in this zone. Drives assassination gating. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
    int32 ZoneLevelMin = 1;

    /** Highest enemy level in this zone. Encounter rolls clamp to [Min..Max]. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
    int32 ZoneLevelMax = 1;

    /** Display name shown on zone-entry banners (TODO: wire up later). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Zone")
    FText ZoneDisplayName;
};
