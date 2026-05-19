#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "EnemyDetectionComponent.generated.h"

class APawn;
class AEnemyEncounter;

/**
 * UEnemyDetectionComponent
 *
 * Vision + detection-meter logic for one AEnemyEncounter (per design: each
 * encounter is represented by a single enemy in the overworld, so vision is
 * per-encounter, not per-enemy).
 *
 * Each tick, traces from the encounter's eye toward the player's pawn:
 *   1. Distance check       — DetectionDistance (max range, cm)
 *   2. Cone (FOV) check     — DetectionConeHalfAngleDeg
 *   3. Line-of-sight trace  — visibility channel, ignores self + player capsule
 *
 * If all three pass, fills DetectionMeter at 1 / DetectionTimeSeconds per second.
 * If LOS breaks, decays at 0.5 × that rate (partial detection is recoverable).
 * When DetectionMeter reaches 1.0 → fires OnDetectionFull and broadcasts a
 * one-hop alert to nearby AEnemyEncounters within AlertRadius (= DetectionRadius
 * per design — same value).
 *
 * Crouch reduces effective detection radius / distance by CrouchDetectionMultiplier
 * (default 0.5×). The exploration pawn flips the multiplier via SetPlayerStealthMultiplier.
 *
 * Debug visualization toggleable via the console var:
 *   r.JRPG.ShowEnemyVision 1
 * draws the cone wedge, max-distance arc, and current meter fill in the PIE viewport.
 */
UCLASS(ClassGroup = (JRPG), meta = (BlueprintSpawnableComponent))
class JRPGCOMBATTESTING_API UEnemyDetectionComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UEnemyDetectionComponent();

    // -------------------------------------------------------------------------
    //  Vision tunables — editable per-encounter in BP defaults
    // -------------------------------------------------------------------------

    /** Hard distance cap for vision, in cm. 1500 = 15m default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Vision",
              meta = (ClampMin = "0.0"))
    float DetectionDistance = 1500.f;

    /** Cone half-angle in degrees. 45° = 90° total FOV. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Vision",
              meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float DetectionConeHalfAngleDeg = 45.f;

    /** Detection radius — used for the alert one-hop and as the design's
     *  "alert radius is a circle in the radius of vision" value. cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Vision",
              meta = (ClampMin = "0.0"))
    float DetectionRadius = 1500.f;

    /** Seconds of continuous LOS required to fully detect. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Vision",
              meta = (ClampMin = "0.1"))
    float DetectionTimeSeconds = 3.f;

    /** Decay multiplier when LOS is broken (relative to fill rate). 0.2 = empties a full meter in ~15s. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Vision",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MeterDecayMultiplier = 0.2f;

    /** Z offset above the encounter's root for the eye-trace start point. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Detection|Vision")
    float EyeHeight = 80.f;

    // -------------------------------------------------------------------------
    //  Runtime state — read-only
    // -------------------------------------------------------------------------

    /** 0..1 fill of the detection meter. 1.0 = full detection, fires OnDetectionFull. */
    UPROPERTY(BlueprintReadOnly, Category = "Detection|State")
    float DetectionMeter = 0.f;

    /** True while the player is currently inside cone + distance + LOS. */
    UPROPERTY(BlueprintReadOnly, Category = "Detection|State")
    bool bPlayerVisible = false;

    /** Latched once the meter hits 1.0 — prevents re-firing OnDetectionFull every tick. */
    UPROPERTY(BlueprintReadOnly, Category = "Detection|State")
    bool bAlertedThisLife = false;

    // -------------------------------------------------------------------------
    //  Stealth API — exploration pawn calls this when crouching
    // -------------------------------------------------------------------------

    /** 1.0 = standard. 0.5 = crouched (halves effective radius/distance for this player). */
    UFUNCTION(BlueprintCallable, Category = "Detection|Stealth")
    static void SetPlayerStealthMultiplier(UObject* WorldContextObject, float Multiplier);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Detection|Stealth")
    static float GetPlayerStealthMultiplier(UObject* WorldContextObject);

    // -------------------------------------------------------------------------
    //  Events
    // -------------------------------------------------------------------------

    DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDetectionFull, AEnemyEncounter*, Encounter);

    /** Fires once when the meter fills. The encounter listens to this to start
     *  the chase / trigger combat in Phase C. */
    UPROPERTY(BlueprintAssignable, Category = "Detection|Events")
    FOnDetectionFull OnDetectionFull;

    /** Called by neighbors via the one-hop alert. Forces the meter to 1.0
     *  (instant detection) without re-broadcasting (one-hop only). */
    UFUNCTION(BlueprintCallable, Category = "Detection|Events")
    void ReceiveAlert();

protected:

    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /** Run the cone + distance + LOS check against the local player pawn. */
    bool TestPlayerVisibility(APawn* PlayerPawn, float StealthMult) const;

    /** Broadcast an alert to all encounters within AlertRadius (one hop only). */
    void BroadcastAlert();

    /** Toggleable debug draw (cone wedge, meter %). */
    void DrawDebug(APawn* PlayerPawn, float StealthMult) const;
};
