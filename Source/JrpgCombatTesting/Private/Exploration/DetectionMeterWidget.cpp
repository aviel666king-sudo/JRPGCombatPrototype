#include "Exploration/DetectionMeterWidget.h"
#include "Exploration/EnemyDetectionComponent.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

void UDetectionMeterWidget::BindToDetection(UEnemyDetectionComponent* InDetection)
{
    DetectionRef = InDetection;
}

void UDetectionMeterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // DEBUG: start visible at construction so we can see if the widget is
    // even rendered. The tick will override this once Meter < HideBelowMeter,
    // but at least we'll see a one-frame flash on spawn that confirms wiring.
    SetVisibility(ESlateVisibility::HitTestInvisible);

    UE_LOG(LogTemp, Warning, TEXT("[DetectionMeter] NativeConstruct fired — widget is alive."));
}

void UDetectionMeterWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!DetectionRef.IsValid() || !DetectionBar)
    {
        return;
    }

    const float Meter = FMath::Clamp(DetectionRef->DetectionMeter, 0.f, 1.f);

    // Hide entirely if the enemy isn't paying attention. Avoids 20 idle meters
    // cluttering the screen on a populated level.
    if (Meter < HideBelowMeter)
    {
        if (GetVisibility() != ESlateVisibility::Hidden)
        {
            SetVisibility(ESlateVisibility::Hidden);
        }
        return;
    }

    if (GetVisibility() != ESlateVisibility::HitTestInvisible)
    {
        // HitTestInvisible: visible but doesn't eat mouse clicks (it's a world
        // widget — the player shouldn't be able to click it).
        SetVisibility(ESlateVisibility::HitTestInvisible);
    }

    DetectionBar->SetPercent(Meter);

    // Color lerp: green → yellow at 0.5, yellow → red at 1.0
    FLinearColor BarColor;
    if (Meter < 0.5f)
    {
        BarColor = FMath::Lerp(ColorIdle, ColorAlert, Meter / 0.5f);
    }
    else
    {
        BarColor = FMath::Lerp(ColorAlert, ColorFull, (Meter - 0.5f) / 0.5f);
    }
    DetectionBar->SetFillColorAndOpacity(BarColor);

    if (DetectionLabel)
    {
        DetectionLabel->SetText(FText::FromString(FString::Printf(TEXT("%.0f%%"), Meter * 100.f)));
        DetectionLabel->SetColorAndOpacity(FSlateColor(BarColor));
    }
}
