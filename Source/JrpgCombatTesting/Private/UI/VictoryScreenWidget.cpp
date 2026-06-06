#include "UI/VictoryScreenWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "TimerManager.h"

namespace VictoryUI {}
using namespace VictoryUI;
namespace VictoryUI
{
    const FLinearColor ColScreenBG(0.01f, 0.02f, 0.04f, 0.94f);
    const FLinearColor ColTitle   (0.95f, 0.82f, 0.30f, 1.0f);   // gold
    const FLinearColor ColText    (0.92f, 0.94f, 0.98f, 1.0f);
    const FLinearColor ColSubtle  (0.62f, 0.66f, 0.72f, 1.0f);
    const FLinearColor ColBarFill (0.35f, 0.70f, 1.00f, 1.0f);
    const FLinearColor ColBarBG   (0.10f, 0.13f, 0.20f, 1.0f);
    const FLinearColor ColLevelUp (0.55f, 0.95f, 0.45f, 1.0f);
    const FLinearColor ColContinue(0.15f, 0.28f, 0.40f, 1.0f);

    FSlateFontInfo Font(int32 Size, bool bBold = false)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
    }
}

TSharedRef<SWidget> UVictoryScreenWidget::RebuildWidget()
{
    Bars.Reset();
    LevelLabels.Reset();
    PopupLabels.Reset();

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
    Title->SetText(FText::FromString(TEXT("VICTORY")));
    Title->SetFont(Font(46, true));
    Title->SetColorAndOpacity(FSlateColor(ColTitle));
    Title->SetJustification(ETextJustify::Center);
    UVerticalBoxSlot* TS = Box->AddChildToVerticalBox(Title);
    TS->SetHorizontalAlignment(HAlign_Center);
    TS->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

    // ── Animated XP rows ────────────────────────────────────────────────────
    for (const FVictoryMemberXP& M : Members)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

        // Name.
        UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Name->SetText(M.Name.IsEmpty() ? FText::FromString(TEXT("Member")) : M.Name);
        Name->SetFont(Font(18, true));
        Name->SetColorAndOpacity(FSlateColor(ColText));
        UHorizontalBoxSlot* NS = Row->AddChildToHorizontalBox(Name);
        NS->SetVerticalAlignment(VAlign_Center);
        NS->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));

        // Level label.
        UTextBlock* Lv = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Lv->SetText(FText::FromString(FString::Printf(TEXT("Lv %d"), M.StartLevel)));
        Lv->SetFont(Font(15, false));
        Lv->SetColorAndOpacity(FSlateColor(ColSubtle));
        UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(Lv);
        LS->SetVerticalAlignment(VAlign_Center);
        LS->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

        // XP bar.
        UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
        Bar->SetPercent(M.Thresholds.IsValidIndex(0) && M.Thresholds[0] > 0
            ? (float)M.StartXP / M.Thresholds[0] : 0.f);
        Bar->SetFillColorAndOpacity(ColBarFill);
        Bar->WidgetStyle.BackgroundImage.TintColor = FSlateColor(ColBarBG);
        UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(Bar);
        BS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        BS->SetVerticalAlignment(VAlign_Center);
        BS->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

        // Level-up popup tag.
        UTextBlock* Pop = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Pop->SetText(FText::GetEmpty());
        Pop->SetFont(Font(17, true));
        Pop->SetColorAndOpacity(FSlateColor(ColLevelUp));
        UHorizontalBoxSlot* PS = Row->AddChildToHorizontalBox(Pop);
        PS->SetVerticalAlignment(VAlign_Center);

        UVerticalBoxSlot* RS = Box->AddChildToVerticalBox(Row);
        RS->SetPadding(FMargin(0.f, 5.f));

        Bars.Add(Bar);
        LevelLabels.Add(Lv);
        PopupLabels.Add(Pop);
    }

    // ── Static spoils ───────────────────────────────────────────────────────
    for (const FString& Line : SpoilLines)
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        T->SetText(FText::FromString(Line));
        T->SetFont(Font(Line.IsEmpty() ? 8 : 17, false));
        T->SetColorAndOpacity(FSlateColor(ColText));
        T->SetJustification(ETextJustify::Center);
        UVerticalBoxSlot* LS = Box->AddChildToVerticalBox(T);
        LS->SetHorizontalAlignment(HAlign_Center);
        LS->SetPadding(FMargin(0.f, 3.f));
    }

    UButton* Continue = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    Continue->SetBackgroundColor(ColContinue);
    Continue->OnClicked.AddDynamic(this, &UVictoryScreenWidget::HandleContinue);
    UTextBlock* CL = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    CL->SetText(FText::FromString(TEXT("Continue")));
    CL->SetFont(Font(20, true));
    CL->SetColorAndOpacity(FSlateColor(ColText));
    Continue->SetContent(CL);
    UVerticalBoxSlot* CS = Box->AddChildToVerticalBox(Continue);
    CS->SetHorizontalAlignment(HAlign_Center);
    CS->SetPadding(FMargin(0.f, 26.f, 0.f, 0.f));

    return Super::RebuildWidget();
}

void UVictoryScreenWidget::NativeConstruct()
{
    Super::NativeConstruct();

    Elapsed = 0.f;
    for (int32 i = 0; i < Members.Num(); ++i) { ApplyMemberState(i, 0.f); }

    // Drive the fill off a world timer (reliable regardless of UMG's tick
    // frequency). The world is running while the results panel is up.
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(AnimTimer, this, &UVictoryScreenWidget::AdvanceAnim, 0.016f, true);
    }
}

void UVictoryScreenWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(AnimTimer);
    }
    Super::NativeDestruct();
}

void UVictoryScreenWidget::AdvanceAnim()
{
    Elapsed = FMath::Min(Elapsed + 0.016f, Duration);
    const float Alpha = Duration > 0.f ? Elapsed / Duration : 1.f;
    const float Eased = 1.f - FMath::Square(1.f - Alpha);   // ease-out

    for (int32 i = 0; i < Members.Num(); ++i)
    {
        ApplyMemberState(i, Eased);
    }

    if (Elapsed >= Duration)
    {
        if (UWorld* W = GetWorld())
        {
            W->GetTimerManager().ClearTimer(AnimTimer);
        }
    }
}

void UVictoryScreenWidget::ApplyMemberState(int32 Index, float EasedAlpha)
{
    if (!Members.IsValidIndex(Index)) { return; }
    const FVictoryMemberXP& M = Members[Index];

    const int32 ShownGain = FMath::RoundToInt(M.XPGained * EasedAlpha);
    int32 X = M.StartXP + ShownGain;

    int32 Level = M.StartLevel;
    int32 K = 0;
    while (M.Thresholds.IsValidIndex(K) && M.Thresholds[K] > 0 && X >= M.Thresholds[K])
    {
        X -= M.Thresholds[K];
        ++Level;
        ++K;
    }

    const int32 Thr = M.Thresholds.IsValidIndex(K) ? M.Thresholds[K] : FMath::Max(1, X);
    const float Pct = Thr > 0 ? (float)X / Thr : 1.f;

    if (Bars.IsValidIndex(Index) && Bars[Index])
    {
        Bars[Index]->SetPercent(FMath::Clamp(Pct, 0.f, 1.f));
    }
    if (LevelLabels.IsValidIndex(Index) && LevelLabels[Index])
    {
        LevelLabels[Index]->SetText(FText::FromString(FString::Printf(TEXT("Lv %d"), Level)));
    }
    if (PopupLabels.IsValidIndex(Index) && PopupLabels[Index])
    {
        const int32 LevelUps = Level - M.StartLevel;
        PopupLabels[Index]->SetText(LevelUps > 0
            ? FText::FromString(FString::Printf(TEXT("+%d"), LevelUps))
            : FText::GetEmpty());
    }
}

void UVictoryScreenWidget::HandleContinue()
{
    if (OnContinueRequested) { OnContinueRequested(); }
}
