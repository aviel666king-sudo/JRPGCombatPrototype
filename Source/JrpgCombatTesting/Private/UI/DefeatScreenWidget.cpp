#include "UI/DefeatScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Styling/CoreStyle.h"

namespace DefeatUI {}
using namespace DefeatUI;
namespace DefeatUI
{
    const FLinearColor ColScreenBG(0.02f, 0.0f, 0.0f, 0.92f);
    const FLinearColor ColTitle   (0.85f, 0.20f, 0.20f, 1.0f);
    const FLinearColor ColText    (0.92f, 0.94f, 0.98f, 1.0f);
    const FLinearColor ColSubtle  (0.62f, 0.66f, 0.72f, 1.0f);
    const FLinearColor ColRetry   (0.15f, 0.30f, 0.20f, 1.0f);
    const FLinearColor ColGiveUp  (0.28f, 0.12f, 0.12f, 1.0f);

    FSlateFontInfo Font(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
    }
}

TSharedRef<SWidget> UDefeatScreenWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
    Screen->SetBrushColor(ColScreenBG);
    Screen->SetHorizontalAlignment(HAlign_Center);
    Screen->SetVerticalAlignment(VAlign_Center);
    UCanvasPanelSlot* ScreenSlot = Root->AddChildToCanvas(Screen);
    ScreenSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ScreenSlot->SetOffsets(FMargin(0.f));

    UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Box"));
    Screen->SetContent(Box);

    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(TEXT("DEFEATED")));
    Title->SetFont(Font(48, true));
    Title->SetColorAndOpacity(FSlateColor(ColTitle));
    Title->SetJustification(ETextJustify::Center);
    UVerticalBoxSlot* TS = Box->AddChildToVerticalBox(Title);
    TS->SetHorizontalAlignment(HAlign_Center);
    TS->SetPadding(FMargin(0.f, 0.f, 0.f, 28.f));

    auto AddButton = [&](const FString& Label, const FString& Sub, const FLinearColor& BG,
                         bool bIsRetry)
    {
        UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
        Btn->SetBackgroundColor(BG);
        if (bIsRetry) { Btn->OnClicked.AddDynamic(this, &UDefeatScreenWidget::HandleRetry); }
        else          { Btn->OnClicked.AddDynamic(this, &UDefeatScreenWidget::HandleGiveUp); }

        UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

        UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        L->SetText(FText::FromString(Label));
        L->SetFont(Font(22, true));
        L->SetColorAndOpacity(FSlateColor(ColText));
        L->SetJustification(ETextJustify::Center);
        Inner->AddChildToVerticalBox(L);

        if (!Sub.IsEmpty())
        {
            UTextBlock* S = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            S->SetText(FText::FromString(Sub));
            S->SetFont(Font(13, false));
            S->SetColorAndOpacity(FSlateColor(ColSubtle));
            S->SetJustification(ETextJustify::Center);
            Inner->AddChildToVerticalBox(S);
        }

        Btn->SetContent(Inner);

        UVerticalBoxSlot* BS = Box->AddChildToVerticalBox(Btn);
        BS->SetHorizontalAlignment(HAlign_Center);
        BS->SetPadding(FMargin(0.f, 6.f));
    };

    AddButton(TEXT("Retry"),  TEXT("Restart this fight, same conditions"), ColRetry, /*bIsRetry=*/true);
    AddButton(TEXT("Give Up"),
              GiveUpLabel.IsEmpty() ? FString(TEXT("Return to last checkpoint")) : GiveUpLabel.ToString(),
              ColGiveUp, /*bIsRetry=*/false);

    return Super::RebuildWidget();
}

void UDefeatScreenWidget::HandleRetry()
{
    if (OnRetryRequested) { OnRetryRequested(); }
}

void UDefeatScreenWidget::HandleGiveUp()
{
    if (OnGiveUpRequested) { OnGiveUpRequested(); }
}
