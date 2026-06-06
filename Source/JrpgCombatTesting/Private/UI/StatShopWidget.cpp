#include "UI/StatShopWidget.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Spacer.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

#include "Exploration/JrpgGameMode.h"
#include "Kismet/GameplayStatics.h"

namespace StatShopUI {}
using namespace StatShopUI;
namespace StatShopUI
{
    const FLinearColor Dimmer    (0.00f, 0.00f, 0.00f, 0.72f);
    const FLinearColor CardBG     (0.05f, 0.06f, 0.09f, 0.98f);
    const FLinearColor ColBG      (0.09f, 0.10f, 0.14f, 1.0f);
    const FLinearColor Stroke     (0.55f, 0.62f, 0.78f, 0.45f);
    const FLinearColor TitleCol   (0.92f, 0.94f, 1.00f, 1.0f);
    const FLinearColor SubCol     (0.60f, 0.64f, 0.74f, 1.0f);
    const FLinearColor NameCol    (0.97f, 0.97f, 1.00f, 1.0f);
    const FLinearColor LevelCol   (1.00f, 0.82f, 0.32f, 1.0f);
    const FLinearColor CoinCol    (0.55f, 0.85f, 1.00f, 1.0f);
    const FLinearColor StatNameCol(0.70f, 0.74f, 0.82f, 1.0f);
    const FLinearColor StatValCol (0.97f, 0.97f, 1.00f, 1.0f);
    const FLinearColor BuyCol     (0.45f, 0.90f, 0.55f, 1.0f);
}

// ---- UStatShopButton --------------------------------------------------------

void UStatShopButton::HandleClicked()
{
    if (OnUpgradeClicked) { OnUpgradeClicked(MemberIndex, Stat); }
}

// ---- helpers ----------------------------------------------------------------

FSlateFontInfo UStatShopWidget::MakeFont(int32 Size, bool bBold)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

const TCHAR* UStatShopWidget::StatLabel(EUpgradeStat Stat)
{
    switch (Stat)
    {
        case EUpgradeStat::MaxHP:   return TEXT("Max HP");
        case EUpgradeStat::Attack:  return TEXT("Attack");
        case EUpgradeStat::Defense: return TEXT("Defense");
        case EUpgradeStat::Speed:   return TEXT("Speed");
        default:                    return TEXT("?");
    }
}

APlayerCombatant* UStatShopWidget::GetMember(int32 Index) const
{
    AJrpgGameMode* GM = CachedGameMode.Get();
    if (!GM) { return nullptr; }
    const TArray<ACombatantBase*>& Party = GM->GetPlayerParty();
    if (!Party.IsValidIndex(Index)) { return nullptr; }
    return Cast<APlayerCombatant>(Party[Index]);
}

// ---- build ------------------------------------------------------------------

TSharedRef<SWidget> UStatShopWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(
            UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        WidgetTree->RootWidget = Root;
        BuildLayout(Root);
    }
    return Super::RebuildWidget();
}

void UStatShopWidget::BuildLayout(UPanelWidget* Root)
{
    // Full-screen dimmer.
    {
        UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
        Dim->SetBrushColor(Dimmer);
        UPanelSlot* PS = Root->AddChild(Dim);
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(PS))
        {
            CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
            CS->SetOffsets(FMargin(0.f));
        }
    }

    // Centered card.
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(CardBG);
    Card->SetPadding(FMargin(36.f, 28.f));
    {
        UPanelSlot* PS = Root->AddChild(Card);
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(PS))
        {
            CS->SetAnchors(FAnchors(0.5f, 0.5f));
            CS->SetAlignment(FVector2D(0.5f, 0.5f));
            CS->SetAutoSize(true);
        }
    }

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Col);

    // Title.
    {
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(FText::FromString(TEXT("STAT SHOP")));
        Title->SetFont(MakeFont(28, true));
        Title->SetColorAndOpacity(TitleCol);
        Col->AddChildToVerticalBox(Title);

        UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>();
        Sub->SetText(FText::FromString(TEXT("Spend StatCoins on permanent upgrades")));
        Sub->SetFont(MakeFont(13, false));
        Sub->SetColorAndOpacity(SubCol);
        UVerticalBoxSlot* SS = Col->AddChildToVerticalBox(Sub);
        SS->SetPadding(FMargin(0.f, 2.f, 0.f, 22.f));
    }

    // Member columns row.
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        for (int32 i = 0; i < MaxMembers; ++i)
        {
            MemberColumns[i] = BuildMemberColumn(i);
            UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(MemberColumns[i]);
            HS->SetPadding(FMargin(i == 0 ? 0.f : 14.f, 0.f, 0.f, 0.f));
            HS->SetVerticalAlignment(VAlign_Top);
        }
        Col->AddChildToVerticalBox(Row);
    }

    // Close button.
    {
        UStatShopButton* Close = WidgetTree->ConstructWidget<UStatShopButton>();
        Close->OnClicked.AddDynamic(Close, &UStatShopButton::HandleClicked);
        Close->OnUpgradeClicked = [this](int32, EUpgradeStat) { if (OnCloseRequested) { OnCloseRequested(); } };

        UTextBlock* CloseTxt = WidgetTree->ConstructWidget<UTextBlock>();
        CloseTxt->SetText(FText::FromString(TEXT("Close")));
        CloseTxt->SetFont(MakeFont(15, true));
        CloseTxt->SetColorAndOpacity(TitleCol);
        Close->SetContent(CloseTxt);

        UVerticalBoxSlot* CS = Col->AddChildToVerticalBox(Close);
        CS->SetPadding(FMargin(0.f, 24.f, 0.f, 0.f));
        CS->SetHorizontalAlignment(HAlign_Center);
    }
}

UPanelWidget* UStatShopWidget::BuildMemberColumn(int32 Index)
{
    UBorder* ColCard = WidgetTree->ConstructWidget<UBorder>();
    ColCard->SetBrushColor(ColBG);
    ColCard->SetPadding(FMargin(18.f, 16.f));

    UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();
    ColCard->SetContent(V);

    NameTexts[Index] = WidgetTree->ConstructWidget<UTextBlock>();
    NameTexts[Index]->SetFont(MakeFont(19, true));
    NameTexts[Index]->SetColorAndOpacity(NameCol);
    V->AddChildToVerticalBox(NameTexts[Index]);

    LevelTexts[Index] = WidgetTree->ConstructWidget<UTextBlock>();
    LevelTexts[Index]->SetFont(MakeFont(13, true));
    LevelTexts[Index]->SetColorAndOpacity(LevelCol);
    V->AddChildToVerticalBox(LevelTexts[Index]);

    CoinTexts[Index] = WidgetTree->ConstructWidget<UTextBlock>();
    CoinTexts[Index]->SetFont(MakeFont(14, true));
    CoinTexts[Index]->SetColorAndOpacity(CoinCol);
    UVerticalBoxSlot* CoinSlot = V->AddChildToVerticalBox(CoinTexts[Index]);
    CoinSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 12.f));

    for (int32 s = 0; s < NumStats; ++s)
    {
        const EUpgradeStat Stat = static_cast<EUpgradeStat>(s);

        UHorizontalBox* StatRow = WidgetTree->ConstructWidget<UHorizontalBox>();

        UTextBlock* NameLbl = WidgetTree->ConstructWidget<UTextBlock>();
        NameLbl->SetText(FText::FromString(StatLabel(Stat)));
        NameLbl->SetFont(MakeFont(13, false));
        NameLbl->SetColorAndOpacity(StatNameCol);
        UHorizontalBoxSlot* NameSlot = StatRow->AddChildToHorizontalBox(NameLbl);
        NameSlot->SetVerticalAlignment(VAlign_Center);

        USpacer* Sp = WidgetTree->ConstructWidget<USpacer>();
        UHorizontalBoxSlot* SpSlot = StatRow->AddChildToHorizontalBox(Sp);
        SpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        StatValueTexts[Index][s] = WidgetTree->ConstructWidget<UTextBlock>();
        StatValueTexts[Index][s]->SetFont(MakeFont(14, true));
        StatValueTexts[Index][s]->SetColorAndOpacity(StatValCol);
        UHorizontalBoxSlot* ValSlot = StatRow->AddChildToHorizontalBox(StatValueTexts[Index][s]);
        ValSlot->SetVerticalAlignment(VAlign_Center);
        ValSlot->SetPadding(FMargin(8.f, 0.f, 10.f, 0.f));

        UStatShopButton* Buy = WidgetTree->ConstructWidget<UStatShopButton>();
        Buy->MemberIndex = Index;
        Buy->Stat        = Stat;
        Buy->OnClicked.AddDynamic(Buy, &UStatShopButton::HandleClicked);
        Buy->OnUpgradeClicked = [this](int32 m, EUpgradeStat st) { HandleUpgrade(m, st); };

        UTextBlock* BuyTxt = WidgetTree->ConstructWidget<UTextBlock>();
        BuyTxt->SetText(FText::FromString(TEXT("+")));
        BuyTxt->SetFont(MakeFont(15, true));
        BuyTxt->SetColorAndOpacity(BuyCol);
        Buy->SetContent(BuyTxt);

        BuyButtons[Index][s] = Buy;
        UHorizontalBoxSlot* BuySlot = StatRow->AddChildToHorizontalBox(Buy);
        BuySlot->SetVerticalAlignment(VAlign_Center);

        UVerticalBoxSlot* RowSlot = V->AddChildToVerticalBox(StatRow);
        RowSlot->SetPadding(FMargin(0.f, s == 0 ? 0.f : 6.f, 0.f, 0.f));
    }

    return ColCard;
}

// ---- runtime ----------------------------------------------------------------

void UStatShopWidget::NativeConstruct()
{
    Super::NativeConstruct();
    CachedGameMode = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    Refresh();
}

void UStatShopWidget::Refresh()
{
    for (int32 i = 0; i < MaxMembers; ++i)
    {
        APlayerCombatant* M = GetMember(i);
        if (MemberColumns[i])
        {
            MemberColumns[i]->SetVisibility(M ? ESlateVisibility::Visible
                                              : ESlateVisibility::Collapsed);
        }
        if (!M) { continue; }

        if (NameTexts[i])  { NameTexts[i]->SetText(M->DisplayName); }
        if (LevelTexts[i]) { LevelTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("Lv %d"), M->Level))); }
        if (CoinTexts[i])  { CoinTexts[i]->SetText(FText::FromString(FString::Printf(TEXT("StatCoins: %d"), M->StatCoins))); }

        for (int32 s = 0; s < NumStats; ++s)
        {
            const EUpgradeStat Stat = static_cast<EUpgradeStat>(s);
            if (StatValueTexts[i][s])
            {
                StatValueTexts[i][s]->SetText(
                    FText::FromString(FString::Printf(TEXT("%.0f"), M->GetStatValue(Stat))));
            }
            if (BuyButtons[i][s])
            {
                BuyButtons[i][s]->SetIsEnabled(M->StatCoins >= M->GetUpgradeCost(Stat));
            }
        }
    }
}

void UStatShopWidget::HandleUpgrade(int32 MemberIndex, EUpgradeStat Stat)
{
    if (APlayerCombatant* M = GetMember(MemberIndex))
    {
        M->TryUpgradeStat(Stat);
        Refresh();
    }
}
