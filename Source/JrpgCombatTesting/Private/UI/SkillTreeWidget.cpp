#include "UI/SkillTreeWidget.h"

#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

#include "Characters/Player/PlayerCombatant.h"
#include "Progression/SkillTreeDataAsset.h"
#include "Exploration/JrpgGameMode.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    const FLinearColor Dimmer   (0.00f, 0.00f, 0.00f, 0.74f);
    const FLinearColor CardBG   (0.05f, 0.06f, 0.09f, 0.98f);
    const FLinearColor TitleCol (0.92f, 0.94f, 1.00f, 1.0f);
    const FLinearColor CoinCol  (0.55f, 0.85f, 1.00f, 1.0f);

    // Node states.
    const FLinearColor OwnedBG     (0.16f, 0.42f, 0.22f, 1.0f);   // green
    const FLinearColor BuyableBG   (0.20f, 0.24f, 0.34f, 1.0f);   // active blue-grey
    const FLinearColor LockedBG    (0.10f, 0.10f, 0.12f, 1.0f);   // dim
    const FLinearColor NodeName    (0.97f, 0.97f, 1.00f, 1.0f);
    const FLinearColor OwnedText   (0.55f, 0.90f, 0.60f, 1.0f);
    const FLinearColor BuyText     (1.00f, 0.82f, 0.32f, 1.0f);   // gold cost
    const FLinearColor LockedText  (0.55f, 0.55f, 0.60f, 1.0f);

    constexpr float ColW  = 250.f;
    constexpr float RowH  = 112.f;
    constexpr float NodeW = 210.f;
    constexpr float NodeH = 88.f;
}

// ---- USkillNodeButton -------------------------------------------------------

void USkillNodeButton::HandleClicked()
{
    if (OnNodeClicked) { OnNodeClicked(NodeId); }
}

// ---- helpers ----------------------------------------------------------------

FSlateFontInfo USkillTreeWidget::MakeFont(int32 Size, bool bBold)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
}

APlayerCombatant* USkillTreeWidget::ResolveFencer() const
{
    AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM) { return nullptr; }
    for (ACombatantBase* Member : GM->GetPlayerParty())
    {
        if (APlayerCombatant* P = Cast<APlayerCombatant>(Member))
        {
            if (P->GetSkillTree()) { return P; }
        }
    }
    return nullptr;
}

// ---- build ------------------------------------------------------------------

TSharedRef<SWidget> USkillTreeWidget::RebuildWidget()
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

void USkillTreeWidget::BuildLayout(UPanelWidget* Root)
{
    // Dimmer.
    {
        UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
        Dim->SetBrushColor(Dimmer);
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Root->AddChild(Dim)))
        {
            CS->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
            CS->SetOffsets(FMargin(0.f));
        }
    }

    // Centered card.
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrushColor(CardBG);
    Card->SetPadding(FMargin(32.f, 24.f));
    if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Root->AddChild(Card)))
    {
        CS->SetAnchors(FAnchors(0.5f, 0.5f));
        CS->SetAlignment(FVector2D(0.5f, 0.5f));
        CS->SetAutoSize(true);
    }

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Card->SetContent(Col);

    // Title.
    {
        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>();
        Title->SetText(FText::FromString(TEXT("SKILL TREE")));
        Title->SetFont(MakeFont(26, true));
        Title->SetColorAndOpacity(TitleCol);
        Col->AddChildToVerticalBox(Title);
    }

    // SkillCoin balance.
    {
        CoinText = WidgetTree->ConstructWidget<UTextBlock>();
        CoinText->SetFont(MakeFont(15, true));
        CoinText->SetColorAndOpacity(CoinCol);
        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(CoinText);
        S->SetPadding(FMargin(0.f, 2.f, 0.f, 2.f));
    }

    // Equipped loadout counter + hint.
    {
        EquippedText = WidgetTree->ConstructWidget<UTextBlock>();
        EquippedText->SetFont(MakeFont(13, false));
        EquippedText->SetColorAndOpacity(FLinearColor(0.72f, 0.76f, 0.84f, 1.f));
        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(EquippedText);
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    }

    // DEBUG: force a level-up (grants stat + skill coins) for testing.
    {
        USkillNodeButton* LvlBtn = WidgetTree->ConstructWidget<USkillNodeButton>();
        LvlBtn->OnClicked.AddDynamic(LvlBtn, &USkillNodeButton::HandleClicked);
        LvlBtn->OnNodeClicked = [this](FName)
        {
            if (APlayerCombatant* F = CachedFencer.Get()) { F->DebugLevelUp(); Refresh(); }
        };
        LvlBtn->SetBackgroundColor(FLinearColor(0.45f, 0.22f, 0.48f, 1.f));

        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
        T->SetText(FText::FromString(TEXT("DEBUG: Level Up (+2 Skill / +3 Stat coins)")));
        T->SetFont(MakeFont(12, true));
        T->SetColorAndOpacity(FLinearColor(0.95f, 0.90f, 1.00f, 1.f));
        LvlBtn->SetContent(T);

        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(LvlBtn);
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f));
        S->SetHorizontalAlignment(HAlign_Center);
    }

    // Fixed-size canvas the nodes are positioned onto.
    {
        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
        Box->SetWidthOverride(3.f * ColW);
        Box->SetHeightOverride(3.f * RowH);

        NodeCanvas = WidgetTree->ConstructWidget<UCanvasPanel>();
        Box->AddChild(NodeCanvas);
        Col->AddChildToVerticalBox(Box);
    }

    // Close button.
    {
        USkillNodeButton* Close = WidgetTree->ConstructWidget<USkillNodeButton>();
        Close->OnClicked.AddDynamic(Close, &USkillNodeButton::HandleClicked);
        Close->OnNodeClicked = [this](FName) { if (OnCloseRequested) { OnCloseRequested(); } };

        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
        T->SetText(FText::FromString(TEXT("Close")));
        T->SetFont(MakeFont(15, true));
        T->SetColorAndOpacity(TitleCol);
        Close->SetContent(T);

        UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Close);
        S->SetPadding(FMargin(0.f, 20.f, 0.f, 0.f));
        S->SetHorizontalAlignment(HAlign_Center);
    }
}

void USkillTreeWidget::NativeConstruct()
{
    Super::NativeConstruct();
    CachedFencer = ResolveFencer();
    BuildNodes();
    Refresh();
}

void USkillTreeWidget::BuildNodes()
{
    if (!NodeCanvas) { return; }
    NodeCanvas->ClearChildren();
    NodeIds.Reset();
    NodeButtons.Reset();
    NodeStateTexts.Reset();

    APlayerCombatant* Fencer = CachedFencer.Get();
    if (!Fencer || !Fencer->GetSkillTree()) { return; }

    for (const FSkillNode& Node : Fencer->GetSkillTree()->Nodes)
    {
        USkillNodeButton* Btn = WidgetTree->ConstructWidget<USkillNodeButton>();
        Btn->NodeId = Node.NodeId;
        Btn->OnClicked.AddDynamic(Btn, &USkillNodeButton::HandleClicked);
        Btn->OnNodeClicked = [this](FName Id) { HandleNodeClicked(Id); };

        UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();

        UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>();
        NameTxt->SetText(Node.DisplayName);
        NameTxt->SetFont(MakeFont(13, true));
        NameTxt->SetColorAndOpacity(NodeName);
        NameTxt->SetJustification(ETextJustify::Center);
        V->AddChildToVerticalBox(NameTxt);

        UTextBlock* StateTxt = WidgetTree->ConstructWidget<UTextBlock>();
        StateTxt->SetFont(MakeFont(11, false));
        StateTxt->SetJustification(ETextJustify::Center);
        UVerticalBoxSlot* StS = V->AddChildToVerticalBox(StateTxt);
        StS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

        Btn->SetContent(V);

        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(NodeCanvas->AddChild(Btn)))
        {
            CS->SetPosition(FVector2D(Node.GridPos.X * ColW, Node.GridPos.Y * RowH));
            CS->SetSize(FVector2D(NodeW, NodeH));
        }

        NodeIds.Add(Node.NodeId);
        NodeButtons.Add(Btn);
        NodeStateTexts.Add(StateTxt);
    }
}

void USkillTreeWidget::Refresh()
{
    APlayerCombatant* Fencer = CachedFencer.Get();
    if (!Fencer) { return; }

    if (CoinText)
    {
        CoinText->SetText(FText::FromString(
            FString::Printf(TEXT("SkillCoins: %d"), Fencer->SkillCoins)));
    }
    if (EquippedText)
    {
        EquippedText->SetText(FText::FromString(FString::Printf(
            TEXT("Equipped: %d / %d   (click an owned skill to equip/unequip)"),
            Fencer->GetEquippedSkillCount(), APlayerCombatant::MaxEquippedSkills)));
    }

    const USkillTreeDataAsset* Tree = Fencer->GetSkillTree();
    if (!Tree) { return; }

    for (int32 i = 0; i < NodeIds.Num(); ++i)
    {
        const FName Id = NodeIds[i];
        USkillNodeButton* Btn = NodeButtons[i];
        UTextBlock* StateTxt = NodeStateTexts[i];
        if (!Btn || !StateTxt) { continue; }

        const FSkillNode* Node = Tree->FindNode(Id);
        const int32 Cost = Node ? Node->SkillCoinCost : 0;

        if (Fencer->IsNodeUnlocked(Id))
        {
            // Owned — clicking toggles equip on/off.
            if (Fencer->IsNodeEquipped(Id))
            {
                Btn->SetBackgroundColor(OwnedBG);
                Btn->SetIsEnabled(true);
                StateTxt->SetText(FText::FromString(TEXT("Equipped ✓")));
                StateTxt->SetColorAndOpacity(OwnedText);
            }
            else if (Fencer->CanEquipMore())
            {
                Btn->SetBackgroundColor(BuyableBG);
                Btn->SetIsEnabled(true);
                StateTxt->SetText(FText::FromString(TEXT("Equip")));
                StateTxt->SetColorAndOpacity(BuyText);
            }
            else
            {
                // Owned but loadout full — must unequip another first.
                Btn->SetBackgroundColor(LockedBG);
                Btn->SetIsEnabled(false);
                StateTxt->SetText(FText::FromString(TEXT("Owned (loadout full)")));
                StateTxt->SetColorAndOpacity(LockedText);
            }
        }
        else if (Fencer->CanUnlockNode(Id))
        {
            Btn->SetBackgroundColor(BuyableBG);
            Btn->SetIsEnabled(true);
            StateTxt->SetText(FText::FromString(FString::Printf(TEXT("Buy: %d"), Cost)));
            StateTxt->SetColorAndOpacity(BuyText);
        }
        else
        {
            Btn->SetBackgroundColor(LockedBG);
            Btn->SetIsEnabled(false);
            const bool bPrereq = Fencer->ArePrerequisitesMet(Id);
            StateTxt->SetText(FText::FromString(
                bPrereq ? FString::Printf(TEXT("Need %d coins"), Cost) : TEXT("Locked")));
            StateTxt->SetColorAndOpacity(LockedText);
        }
    }
}

void USkillTreeWidget::HandleNodeClicked(FName NodeId)
{
    APlayerCombatant* Fencer = CachedFencer.Get();
    if (!Fencer) { return; }

    if (Fencer->IsNodeUnlocked(NodeId))
    {
        // Already owned — toggle it in/out of the active loadout.
        Fencer->ToggleEquipNode(NodeId);
    }
    else
    {
        // Not owned — attempt to buy it (auto-equips if there's room).
        Fencer->TryUnlockNode(NodeId);
    }
    Refresh();
}
