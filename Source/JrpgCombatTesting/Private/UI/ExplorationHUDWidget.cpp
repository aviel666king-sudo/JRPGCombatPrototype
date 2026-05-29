#include "UI/ExplorationHUDWidget.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "Fonts/SlateFontInfo.h"

#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "CombatTypes.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    // Palette --------------------------------------------------------------
    const FLinearColor PanelBG     (0.04f, 0.05f, 0.07f, 0.82f);
    const FLinearColor PanelStroke (0.55f, 0.62f, 0.78f, 0.55f);
    const FLinearColor HeaderCol   (0.62f, 0.70f, 0.88f, 1.0f);
    const FLinearColor NameCol     (0.97f, 0.97f, 1.00f, 1.0f);
    const FLinearColor DeadCol     (0.55f, 0.55f, 0.58f, 1.0f);
    const FLinearColor LevelCol    (1.00f, 0.82f, 0.32f, 1.0f);
    const FLinearColor HPTextCol   (1.00f, 1.00f, 1.00f, 1.0f);
    const FLinearColor BarTrack    (0.10f, 0.11f, 0.14f, 1.0f);
    const FLinearColor HintCol     (0.60f, 0.63f, 0.70f, 1.0f);

    const FLinearColor HPGreen (0.30f, 0.85f, 0.40f, 1.0f);
    const FLinearColor HPAmber (0.95f, 0.78f, 0.25f, 1.0f);
    const FLinearColor HPRed   (0.90f, 0.27f, 0.27f, 1.0f);

    FLinearColor HPColorFor(float Pct)
    {
        if (Pct <= 0.001f) { return HPRed; }
        if (Pct < 0.25f)   { return HPRed; }
        if (Pct < 0.50f)   { return HPAmber; }
        return HPGreen;
    }
}

FSlateFontInfo UExplorationHUDWidget::MakeFont(int32 Size, bool bBold)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

void UExplorationHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    CachedGameMode = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    BuildLayout();
}

void UExplorationHUDWidget::BuildLayout()
{
    if (PanelRoot) { return; }            // already built
    if (!WidgetTree) { return; }

    UPanelWidget* Root = Cast<UPanelWidget>(GetRootWidget());
    if (!Root) { return; }                // WBP root must be a panel (Canvas Panel)

    // Outer border (the dark rounded card) ---------------------------------
    PanelRoot = WidgetTree->ConstructWidget<UBorder>();
    PanelRoot->SetBrushColor(PanelBG);
    PanelRoot->SetPadding(FMargin(24.f, 18.f));

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    PanelRoot->SetContent(Col);

    // Header ---------------------------------------------------------------
    {
        UTextBlock* Header = WidgetTree->ConstructWidget<UTextBlock>();
        Header->SetText(FText::FromString(TEXT("PARTY")));
        Header->SetFont(MakeFont(15, true));
        Header->SetColorAndOpacity(HeaderCol);
        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Header);
        S->SetPadding(FMargin(2.f, 0.f, 0.f, 12.f));
    }

    // Party rows -----------------------------------------------------------
    for (int32 i = 0; i < NumRows; ++i)
    {
        UVerticalBox* RowCol = WidgetTree->ConstructWidget<UVerticalBox>();

        // Top line: Name .... Lv N
        {
            UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();

            NameTexts[i] = WidgetTree->ConstructWidget<UTextBlock>();
            NameTexts[i]->SetFont(MakeFont(20, true));
            NameTexts[i]->SetColorAndOpacity(NameCol);
            Line->AddChildToHorizontalBox(NameTexts[i]);

            USpacer* Sp = WidgetTree->ConstructWidget<USpacer>();
            UHorizontalBoxSlot* SpSlot = Line->AddChildToHorizontalBox(Sp);
            SpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

            LevelTexts[i] = WidgetTree->ConstructWidget<UTextBlock>();
            LevelTexts[i]->SetFont(MakeFont(16, true));
            LevelTexts[i]->SetColorAndOpacity(LevelCol);
            UHorizontalBoxSlot* LvSlot = Line->AddChildToHorizontalBox(LevelTexts[i]);
            LvSlot->SetVerticalAlignment(VAlign_Center);
            LvSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

            UVerticalBoxSlot* LineSlot = RowCol->AddChildToVerticalBox(Line);
            LineSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
        }

        // HP bar with the "cur / max" text overlaid + centered
        {
            UOverlay* BarOverlay = WidgetTree->ConstructWidget<UOverlay>();

            HPBars[i] = WidgetTree->ConstructWidget<UProgressBar>();
            HPBars[i]->SetFillColorAndOpacity(HPGreen);
            // Track colour via the bar style background tint.
            FProgressBarStyle BarStyle = HPBars[i]->GetWidgetStyle();
            BarStyle.BackgroundImage.TintColor = FSlateColor(BarTrack);
            HPBars[i]->SetWidgetStyle(BarStyle);
            UOverlaySlot* BarSlot = BarOverlay->AddChildToOverlay(HPBars[i]);
            BarSlot->SetHorizontalAlignment(HAlign_Fill);
            BarSlot->SetVerticalAlignment(VAlign_Fill);

            HPTexts[i] = WidgetTree->ConstructWidget<UTextBlock>();
            HPTexts[i]->SetFont(MakeFont(14, true));
            HPTexts[i]->SetColorAndOpacity(HPTextCol);
            UOverlaySlot* TxtSlot = BarOverlay->AddChildToOverlay(HPTexts[i]);
            TxtSlot->SetHorizontalAlignment(HAlign_Center);
            TxtSlot->SetVerticalAlignment(VAlign_Center);

            // Fixed bar size via a SizeBox wrapper (ProgressBar has no size API).
            USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>();
            BarBox->SetWidthOverride(320.f);
            BarBox->SetHeightOverride(26.f);
            BarBox->AddChild(BarOverlay);

            UVerticalBoxSlot* OverlaySlot = RowCol->AddChildToVerticalBox(BarBox);
            OverlaySlot->SetPadding(FMargin(0.f));
        }

        UVerticalBoxSlot* RowSlot = Col->AddChildToVerticalBox(RowCol);
        RowSlot->SetPadding(FMargin(0.f, (i == 0 ? 0.f : 12.f), 0.f, 0.f));
    }

    // Divider (1px line via a height-locked SizeBox holding a tinted border) --
    {
        UBorder* Divider = WidgetTree->ConstructWidget<UBorder>();
        Divider->SetBrushColor(PanelStroke);
        Divider->SetPadding(FMargin(0.f));

        USizeBox* DivBox = WidgetTree->ConstructWidget<USizeBox>();
        DivBox->SetHeightOverride(1.f);
        DivBox->AddChild(Divider);

        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(DivBox);
        S->SetPadding(FMargin(0.f, 12.f, 0.f, 10.f));
    }

    // Protocol footer: Heal | Revive | AP ----------------------------------
    {
        UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>();

        auto AddCounter = [&](TObjectPtr<UTextBlock>& Out, const FLinearColor& Col2)
        {
            Out = WidgetTree->ConstructWidget<UTextBlock>();
            Out->SetFont(MakeFont(16, true));
            Out->SetColorAndOpacity(Col2);
            UHorizontalBoxSlot* S = Footer->AddChildToHorizontalBox(Out);
            S->SetPadding(FMargin(0.f, 0.f, 22.f, 0.f));
        };

        AddCounter(HealCharges,   FLinearColor(0.45f, 0.90f, 0.55f, 1.f));
        AddCounter(ReviveCharges, FLinearColor(0.65f, 0.75f, 1.00f, 1.f));
        AddCounter(APCharges,     FLinearColor(0.95f, 0.82f, 0.40f, 1.f));

        Col->AddChildToVerticalBox(Footer);
    }

    // Hint -----------------------------------------------------------------
    {
        UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>();
        Hint->SetText(FText::FromString(TEXT("Press H to Heal")));
        Hint->SetFont(MakeFont(12, false));
        Hint->SetColorAndOpacity(HintCol);
        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Hint);
        S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    }

    // Attach to the root canvas, anchored bottom-left.
    UPanelSlot* RootSlot = Root->AddChild(PanelRoot);
    if (UCanvasPanelSlot* CSlot = Cast<UCanvasPanelSlot>(RootSlot))
    {
        CSlot->SetAnchors(FAnchors(0.f, 1.f));   // bottom-left
        CSlot->SetAlignment(FVector2D(0.f, 1.f));
        CSlot->SetAutoSize(true);
        CSlot->SetPosition(FVector2D(28.f, -28.f));
    }

    PanelRoot->SetVisibility(ESlateVisibility::Collapsed);
}

void UExplorationHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!PanelRoot) { BuildLayout(); if (!PanelRoot) { return; } }

    // Show only while holding Tab.
    if (!CachedPawn.IsValid())
    {
        CachedPawn = Cast<AExplorationPawn>(GetOwningPlayerPawn());
    }
    const bool bOpen = CachedPawn.IsValid() && CachedPawn->IsPartyPanelOpen();
    PanelRoot->SetVisibility(bOpen ? ESlateVisibility::HitTestInvisible
                                   : ESlateVisibility::Collapsed);
    if (!bOpen) { return; }               // skip data refresh while hidden

    if (!CachedGameMode.IsValid())
    {
        CachedGameMode = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
        if (!CachedGameMode.IsValid()) { return; }
    }
    AJrpgGameMode* GM = CachedGameMode.Get();

    for (int32 i = 0; i < NumRows; ++i) { RefreshRow(i); }

    if (HealCharges)   { HealCharges->SetText(GM->GetProtocolChargesText(EProtocolType::Healing)); }
    if (ReviveCharges) { ReviveCharges->SetText(GM->GetProtocolChargesText(EProtocolType::Revival)); }
    if (APCharges)     { APCharges->SetText(GM->GetProtocolChargesText(EProtocolType::AP)); }
}

void UExplorationHUDWidget::RefreshRow(int32 Index)
{
    AJrpgGameMode* GM = CachedGameMode.Get();
    if (!GM) { return; }

    const bool bValid = GM->IsPartyMemberValid(Index);
    const bool bDead  = bValid && GM->IsPartyMemberDead(Index);

    if (UTextBlock* NameText = NameTexts[Index])
    {
        NameText->SetText(GM->GetPartyMemberName(Index));
        NameText->SetColorAndOpacity(bDead ? DeadCol : NameCol);
    }

    if (UTextBlock* LevelText = LevelTexts[Index])
    {
        LevelText->SetText(bValid
            ? FText::FromString(FString::Printf(TEXT("Lv %d"), GM->GetPartyMemberLevel(Index)))
            : FText::GetEmpty());
    }

    const float Pct = bValid ? GM->GetPartyMemberHPPercent(Index) : 0.f;
    if (UProgressBar* HPBar = HPBars[Index])
    {
        HPBar->SetPercent(Pct);
        HPBar->SetFillColorAndOpacity(bDead ? DeadCol : HPColorFor(Pct));
    }

    if (UTextBlock* HPText = HPTexts[Index])
    {
        HPText->SetText(bDead ? FText::FromString(TEXT("DOWN")) : GM->GetPartyMemberHPText(Index));
    }
}
