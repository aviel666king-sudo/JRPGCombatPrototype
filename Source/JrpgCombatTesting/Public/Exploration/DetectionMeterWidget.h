#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DetectionMeterWidget.generated.h"

class UProgressBar;
class UTextBlock;
class UEnemyDetectionComponent;

/**
 * UDetectionMeterWidget
 *
 * Floating bar above each AEnemyEncounter. Reads DetectionMeter (0..1) from
 * the encounter's UEnemyDetectionComponent each tick, fills the progress bar,
 * lerps color green → yellow → red, hides itself when meter is empty.
 *
 * The UMG asset (WBP_DetectionMeter) must contain:
 *   - a UProgressBar named "DetectionBar"
 *   - (optional) a UTextBlock named "DetectionLabel" — shows "XX%"
 * BindWidget connects them to the C++ pointers below; if the names don't match
 * exactly, the Blueprint compile will fail with a clear error.
 *
 * Spawned by AEnemyEncounter via a UWidgetComponent in world-space (screen-
 * aligned billboard), so the bar always faces the camera.
 */
UCLASS()
class JRPGCOMBATTESTING_API UDetectionMeterWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /** Called by the encounter on BeginPlay so we know which component to read from. */
    UFUNCTION(BlueprintCallable, Category = "Detection")
    void BindToDetection(UEnemyDetectionComponent* InDetection);

protected:

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // -------------------------------------------------------------------------
    //  Required UMG widgets — bound by name in WBP_DetectionMeter
    // -------------------------------------------------------------------------

    /** Required. Add a ProgressBar named "DetectionBar" to the UMG asset. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> DetectionBar;

    /** Optional. Add a TextBlock named "DetectionLabel" if you want a "XX%" readout. */
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> DetectionLabel;

    // -------------------------------------------------------------------------
    //  Tunables
    // -------------------------------------------------------------------------

    /** Below this meter value the widget hides entirely (avoids clutter on patrolling enemies). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Detection|Display",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float HideBelowMeter = 0.02f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Detection|Display")
    FLinearColor ColorIdle  = FLinearColor(0.10f, 0.85f, 0.10f, 1.f);  // green

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Detection|Display")
    FLinearColor ColorAlert = FLinearColor(1.00f, 0.85f, 0.05f, 1.f);  // yellow

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Detection|Display")
    FLinearColor ColorFull  = FLinearColor(1.00f, 0.10f, 0.10f, 1.f);  // red

private:

    /** Set by the owning encounter on spawn. Weak ptr so a destroyed encounter doesn't leave a dangling read. */
    UPROPERTY()
    TWeakObjectPtr<UEnemyDetectionComponent> DetectionRef;
};
