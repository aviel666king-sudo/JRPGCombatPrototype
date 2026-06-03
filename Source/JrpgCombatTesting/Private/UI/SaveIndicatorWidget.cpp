#include "UI/SaveIndicatorWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

TSharedRef<SWidget> USaveIndicatorWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Pill = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Pill"));
    Pill->SetBrushColor(FLinearColor(0.02f, 0.03f, 0.05f, 0.78f));
    Pill->SetPadding(FMargin(14.f, 8.f));

    UTextBlock* Text = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Text->SetText(Message.IsEmpty() ? FText::FromString(TEXT("Autosaving…")) : Message);
    Text->SetFont(FCoreStyle::GetDefaultFontStyle(TEXT("Italic"), 13));
    Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.85f, 0.92f, 1.f)));
    Pill->SetContent(Text);

    // Anchor bottom-left, offset a little in from the corner.
    UCanvasPanelSlot* PillSlot = Root->AddChildToCanvas(Pill);
    PillSlot->SetAnchors(FAnchors(0.f, 1.f, 0.f, 1.f));
    PillSlot->SetAlignment(FVector2D(0.f, 1.f));
    PillSlot->SetAutoSize(true);
    PillSlot->SetPosition(FVector2D(28.f, -28.f));

    return Super::RebuildWidget();
}

void USaveIndicatorWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(DismissTimer, this, &USaveIndicatorWidget::Dismiss,
            FMath::Max(0.2f, Lifetime), false);
    }
}

void USaveIndicatorWidget::Dismiss()
{
    RemoveFromParent();
}
