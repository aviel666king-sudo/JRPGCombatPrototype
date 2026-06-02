#include "UI/RosterWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"

#include "Characters/Player/PlayerCombatant.h"
#include "Equipment/CharacterWeaponDataAsset.h"
#include "Equipment/CharacterArmorDataAsset.h"
#include "Equipment/CharacterChipDataAsset.h"
#include "Equipment/CraftingMaterialDataAsset.h"
#include "Roster/RosterSubsystem.h"
#include "Exploration/JrpgGameMode.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    const FLinearColor ColScreenBG (0.015f, 0.025f, 0.045f, 0.96f);
    const FLinearColor ColCardBG   (0.05f,  0.08f,  0.13f,  1.0f);
    const FLinearColor ColParty1   (0.20f,  0.55f,  0.95f,  1.0f);
    const FLinearColor ColParty2   (0.95f,  0.65f,  0.20f,  1.0f);
    const FLinearColor ColBench    (0.45f,  0.45f,  0.50f,  1.0f);
    const FLinearColor ColBtn      (0.12f,  0.18f,  0.28f,  1.0f);
    const FLinearColor ColText     (0.92f,  0.94f,  0.98f,  1.0f);
    const FLinearColor ColSubtle   (0.62f,  0.66f,  0.72f,  1.0f);
    const FLinearColor ColPassive  (0.70f,  0.85f,  0.55f,  1.0f);

    FLinearColor AssignmentColour(EPartyAssignment A)
    {
        switch (A)
        {
            case EPartyAssignment::Party1: return ColParty1;
            case EPartyAssignment::Party2: return ColParty2;
            default:                       return ColBench;
        }
    }
}

// -----------------------------------------------------------------------------
//  URosterActionButton
// -----------------------------------------------------------------------------

void URosterActionButton::HandleClicked()
{
    if (OnActionClicked) { OnActionClicked(this); }
}

// -----------------------------------------------------------------------------
//  Static helpers
// -----------------------------------------------------------------------------

FSlateFontInfo URosterWidget::MakeFont(int32 Size, bool bBold)
{
    return FCoreStyle::GetDefaultFontStyle(bBold ? TEXT("Bold") : TEXT("Regular"), Size);
}

FText URosterWidget::AssignmentLabel(EPartyAssignment A)
{
    switch (A)
    {
        case EPartyAssignment::Party1: return FText::FromString(TEXT("Party 1"));
        case EPartyAssignment::Party2: return FText::FromString(TEXT("Party 2"));
        default:                       return FText::FromString(TEXT("Bench"));
    }
}

const TCHAR* URosterWidget::WeaponTierName(uint8 Tier)
{
    static const TCHAR* Names[] = { TEXT("D"), TEXT("C"), TEXT("B"), TEXT("A"), TEXT("S"), TEXT("S+") };
    return (Tier < UE_ARRAY_COUNT(Names)) ? Names[Tier] : TEXT("?");
}

const TCHAR* URosterWidget::BuffedStatName(uint8 Stat)
{
    static const TCHAR* Names[] = {
        TEXT("None"), TEXT("Max HP"), TEXT("Max AP"), TEXT("Attack"),
        TEXT("Defense"), TEXT("Speed"), TEXT("Crit") };
    return (Stat < UE_ARRAY_COUNT(Names)) ? Names[Stat] : TEXT("?");
}

URosterSubsystem* URosterWidget::GetRoster() const
{
    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    return GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
}

URosterActionButton* URosterWidget::MakeButton(const TCHAR* Label, ERosterAction Action, const FLinearColor& BG)
{
    URosterActionButton* Btn = WidgetTree->ConstructWidget<URosterActionButton>(URosterActionButton::StaticClass());
    Btn->SetBackgroundColor(BG);
    Btn->Action = Action;
    Btn->OnActionClicked = [this](URosterActionButton* B) { HandleButton(B); };
    Btn->OnClicked.AddDynamic(Btn, &URosterActionButton::HandleClicked);

    UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Lbl->SetText(FText::FromString(Label));
    Lbl->SetFont(MakeFont(13, true));
    Lbl->SetColorAndOpacity(FSlateColor(ColText));
    Btn->SetContent(Lbl);
    return Btn;
}

// -----------------------------------------------------------------------------
//  Build root
// -----------------------------------------------------------------------------

TSharedRef<SWidget> URosterWidget::RebuildWidget()
{
    UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Screen = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Screen"));
    Screen->SetBrushColor(ColScreenBG);
    Screen->SetPadding(FMargin(40.f, 28.f));

    UCanvasPanelSlot* ScreenSlot = Root->AddChildToCanvas(Screen);
    ScreenSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
    ScreenSlot->SetOffsets(FMargin(0.f));

    Switcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("Switcher"));
    Screen->SetContent(Switcher);

    // ---- Page 0: Roster ----
    {
        UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RosterPage"));

        UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Title->SetText(FText::FromString(TEXT("Party Management")));
        Title->SetFont(MakeFont(28, true));
        Title->SetColorAndOpacity(FSlateColor(ColText));
        UVerticalBoxSlot* TS = Page->AddChildToVerticalBox(Title);
        TS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

        RosterList = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("RosterList"));
        UVerticalBoxSlot* LS = Page->AddChildToVerticalBox(RosterList);
        LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        UHorizontalBox* Footer = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
        PartyCountText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        PartyCountText->SetFont(MakeFont(16, false));
        PartyCountText->SetColorAndOpacity(FSlateColor(ColSubtle));
        UHorizontalBoxSlot* PCS = Footer->AddChildToHorizontalBox(PartyCountText);
        PCS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        PCS->SetVerticalAlignment(VAlign_Center);

        URosterActionButton* Close = MakeButton(TEXT("Close  (Tab / Esc)"), ERosterAction::Close, ColBtn);
        Footer->AddChildToHorizontalBox(Close);

        UVerticalBoxSlot* FS = Page->AddChildToVerticalBox(Footer);
        FS->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));

        Switcher->AddChild(Page);
    }

    // ---- Page 1: Detail ----
    {
        UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailPage"));

        URosterActionButton* Back = MakeButton(TEXT("Back to Roster"), ERosterAction::BackToRoster, ColBtn);
        UVerticalBoxSlot* BS = Page->AddChildToVerticalBox(Back);
        BS->SetHorizontalAlignment(HAlign_Left);
        BS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

        DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailBox"));
        UVerticalBoxSlot* DS = Page->AddChildToVerticalBox(DetailBox);
        DS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        Switcher->AddChild(Page);
    }

    // ---- Page 2: Switch ----
    {
        UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SwitchPage"));

        URosterActionButton* Back = MakeButton(TEXT("Back"), ERosterAction::BackToDetail, ColBtn);
        UVerticalBoxSlot* BS = Page->AddChildToVerticalBox(Back);
        BS->SetHorizontalAlignment(HAlign_Left);
        BS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

        SwitchBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("SwitchBox"));
        UVerticalBoxSlot* SS = Page->AddChildToVerticalBox(SwitchBox);
        SS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        Switcher->AddChild(Page);
    }

    return Super::RebuildWidget();
}

void URosterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (URosterSubsystem* R = GetRoster())
    {
        if (AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this)))
        {
            R->SaveHPFromParty(GM->GetPlayerParty());
        }
    }

    if (Switcher) { Switcher->SetActiveWidgetIndex(0); }
    RefreshRoster();
}

FReply URosterWidget::NativeOnKeyDown(const FGeometry& Geo, const FKeyEvent& Key)
{
    const FKey K = Key.GetKey();
    if (K == EKeys::Escape || K == EKeys::Tab)
    {
        const int32 Page = Switcher ? Switcher->GetActiveWidgetIndex() : 0;
        if (Page == 2)      { ShowDetail(CurrentMemberIndex); return FReply::Handled(); }
        if (Page == 1)      { Switcher->SetActiveWidgetIndex(0); RefreshRoster(); return FReply::Handled(); }
        if (OnCloseRequested) { OnCloseRequested(); }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geo, Key);
}

// -----------------------------------------------------------------------------
//  Roster page
// -----------------------------------------------------------------------------

void URosterWidget::RefreshRoster()
{
    if (!RosterList) { return; }
    RosterList->ClearChildren();

    URosterSubsystem* R = GetRoster();
    if (!R) { return; }

    const int32 Count = R->GetMemberCount();
    if (Count == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(FText::FromString(TEXT("No party members yet. Start in the test level once to populate the roster.")));
        Empty->SetFont(MakeFont(16, false));
        Empty->SetColorAndOpacity(FSlateColor(ColSubtle));
        RosterList->AddChildToVerticalBox(Empty);
    }
    else
    {
        for (int32 i = 0; i < Count; ++i)
        {
            UBorder* Card = BuildCharacterCard(i);
            UVerticalBoxSlot* CS = RosterList->AddChildToVerticalBox(Card);
            CS->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
        }
    }

    if (PartyCountText)
    {
        PartyCountText->SetText(FText::FromString(FString::Printf(
            TEXT("Party 1: %d/%d    Party 2: %d/%d        Potions  Heal %d/%d   Revive %d/%d   AP %d/%d"),
            R->CountInParty(EPartyAssignment::Party1), URosterSubsystem::MaxPartySize,
            R->CountInParty(EPartyAssignment::Party2), URosterSubsystem::MaxPartySize,
            R->GetHealCharges(),   R->GetMaxHealCharges(),
            R->GetReviveCharges(), R->GetMaxReviveCharges(),
            R->GetAPCharges(),     R->GetMaxAPCharges())));
    }
}

UBorder* URosterWidget::BuildCharacterCard(int32 MemberIndex)
{
    URosterSubsystem* R = GetRoster();
    const FPartyMemberRecord& Rec = R->GetMember(MemberIndex);

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Card->SetBrushColor(ColCardBG);
    Card->SetPadding(FMargin(16.f, 12.f));

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
    Card->SetContent(Row);

    UVerticalBox* Info = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());

    UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Name->SetText(Rec.DisplayName.IsEmpty() ? FText::FromString(TEXT("Unnamed")) : Rec.DisplayName);
    Name->SetFont(MakeFont(20, true));
    Name->SetColorAndOpacity(FSlateColor(Rec.IsDead() ? FLinearColor(0.8f, 0.35f, 0.35f) : ColText));
    Info->AddChildToVerticalBox(Name);

    UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Sub->SetText(FText::FromString(Rec.IsDead()
        ? FString::Printf(TEXT("Lv %d      DOWNED"), Rec.Level)
        : FString::Printf(TEXT("Lv %d      HP %.0f / %.0f"), Rec.Level, Rec.CurrentHP, Rec.MaxHP)));
    Sub->SetFont(MakeFont(14, false));
    Sub->SetColorAndOpacity(FSlateColor(ColSubtle));
    Info->AddChildToVerticalBox(Sub);

    UHorizontalBoxSlot* InfoSlot = Row->AddChildToHorizontalBox(Info);
    InfoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    InfoSlot->SetVerticalAlignment(VAlign_Center);

    UBorder* Tag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Tag->SetBrushColor(AssignmentColour(Rec.Assignment));
    Tag->SetPadding(FMargin(10.f, 4.f));
    UTextBlock* TagText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TagText->SetText(AssignmentLabel(Rec.Assignment));
    TagText->SetFont(MakeFont(13, true));
    TagText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
    Tag->SetContent(TagText);
    UHorizontalBoxSlot* TagSlot = Row->AddChildToHorizontalBox(Tag);
    TagSlot->SetVerticalAlignment(VAlign_Center);
    TagSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));

    auto AddCardBtn = [&](const TCHAR* Label, ERosterAction Action, const FLinearColor& BG)
    {
        URosterActionButton* Btn = MakeButton(Label, Action, BG);
        Btn->MemberIndex = MemberIndex;
        UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(Btn);
        BS->SetPadding(FMargin(3.f, 0.f));
        BS->SetVerticalAlignment(VAlign_Center);
    };
    AddCardBtn(TEXT("P1"),      ERosterAction::AssignParty1, ColParty1 * 0.5f);
    AddCardBtn(TEXT("P2"),      ERosterAction::AssignParty2, ColParty2 * 0.5f);
    AddCardBtn(TEXT("Bench"),   ERosterAction::AssignBench,  ColBtn);
    AddCardBtn(TEXT("Details"), ERosterAction::OpenDetails,  ColBtn);

    return Card;
}

// -----------------------------------------------------------------------------
//  Detail page
// -----------------------------------------------------------------------------

void URosterWidget::ShowDetail(int32 MemberIndex)
{
    URosterSubsystem* R = GetRoster();
    if (!DetailBox || !R || !R->IsValidMember(MemberIndex)) { return; }

    CurrentMemberIndex = MemberIndex;
    const FPartyMemberRecord& Rec = R->GetMember(MemberIndex);
    DetailBox->ClearChildren();
    AddDetailHeader(Rec);
    AddStatsBlock(Rec);
    AddLoadoutBlock(Rec);

    if (Switcher) { Switcher->SetActiveWidgetIndex(1); }
}

void URosterWidget::AddDetailHeader(const FPartyMemberRecord& Rec)
{
    UTextBlock* Name = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Name->SetText(Rec.DisplayName.IsEmpty() ? FText::FromString(TEXT("Unnamed")) : Rec.DisplayName);
    Name->SetFont(MakeFont(26, true));
    Name->SetColorAndOpacity(FSlateColor(ColText));
    DetailBox->AddChildToVerticalBox(Name);

    UTextBlock* Lv = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Lv->SetText(FText::FromString(FString::Printf(TEXT("Level %d      HP %.0f / %.0f"),
        Rec.Level, Rec.CurrentHP, Rec.MaxHP)));
    Lv->SetFont(MakeFont(15, false));
    Lv->SetColorAndOpacity(FSlateColor(ColSubtle));
    DetailBox->AddChildToVerticalBox(Lv);

    if (bUpgradeMode)
    {
        URosterSubsystem* R = GetRoster();
        const FString MatName = (R && R->GetPrimaryMaterial())
            ? R->GetPrimaryMaterial()->DisplayName.ToString() : FString(TEXT("Material"));
        UTextBlock* Econ = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Econ->SetText(FText::FromString(FString::Printf(TEXT("Gold %d      %s %d        (Upgrade Gear)"),
            R ? R->GetGold() : 0, *MatName, R ? R->GetMaterialCount(R->GetPrimaryMaterial()) : 0)));
        Econ->SetFont(MakeFont(15, true));
        Econ->SetColorAndOpacity(FSlateColor(ColParty2));
        UVerticalBoxSlot* ES = DetailBox->AddChildToVerticalBox(Econ);
        ES->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    }
}

void URosterWidget::AddStatsBlock(const FPartyMemberRecord& Rec)
{
    UTextBlock* Hdr = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Hdr->SetText(FText::FromString(TEXT("Stats")));
    Hdr->SetFont(MakeFont(18, true));
    Hdr->SetColorAndOpacity(FSlateColor(ColParty1));
    UVerticalBoxSlot* HS = DetailBox->AddChildToVerticalBox(Hdr);
    HS->SetPadding(FMargin(0.f, 18.f, 0.f, 6.f));

    auto AddLine = [this](const FString& Text)
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        T->SetText(FText::FromString(Text));
        T->SetFont(MakeFont(15, false));
        T->SetColorAndOpacity(FSlateColor(ColText));
        DetailBox->AddChildToVerticalBox(T);
    };
    AddLine(FString::Printf(TEXT("Max HP     %.0f"), Rec.MaxHP));
    AddLine(FString::Printf(TEXT("Attack     %.0f"), Rec.Attack));
    AddLine(FString::Printf(TEXT("Defense    %.0f"), Rec.Defense));
    AddLine(FString::Printf(TEXT("Speed      %.0f"), Rec.Speed));
}

void URosterWidget::AddLoadoutRow(const FString& Label, const FString& Value,
                                  ESwitchSlot TargetSlot, int32 ChipSlot, bool bChangeCampOnly)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    T->SetText(FText::FromString(FString::Printf(TEXT("%-14s %s"), *Label, *Value)));
    T->SetFont(MakeFont(15, false));
    T->SetColorAndOpacity(FSlateColor(ColText));
    UHorizontalBoxSlot* TS = Row->AddChildToHorizontalBox(T);
    TS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    TS->SetVerticalAlignment(VAlign_Center);

    // Change button. For camp-only slots (armor chip re-bind) it appears only
    // in the camp upgrade screen.
    if (!bChangeCampOnly || bUpgradeMode)
    {
        URosterActionButton* Change = MakeButton(TEXT("Change"), ERosterAction::OpenSwitch, ColBtn);
        Change->MemberIndex = CurrentMemberIndex;
        Change->SwitchSlot = TargetSlot;
        Change->SlotIndex = ChipSlot;
        UHorizontalBoxSlot* CS = Row->AddChildToHorizontalBox(Change);
        CS->SetVerticalAlignment(VAlign_Center);
    }

    // Upgrade button (camp only). Resolve the equipped item + its cost.
    if (bUpgradeMode)
    {
        URosterSubsystem* R = GetRoster();
        FString UpLabel = TEXT("—");
        bool bClickable = false;
        bool bHasItem = false;   // does this slot hold something upgradable?
        if (R && R->IsValidMember(CurrentMemberIndex))
        {
            const FPartyMemberRecord& Rec = R->GetMember(CurrentMemberIndex);
            int32 NeedGold = 0, NeedMat = 0; bool bMaxed = false, bHave = false;
            if (TargetSlot == ESwitchSlot::MainWeapon && Rec.MainWeapon)
            { bHave = R->GetWeaponUpgradeInfo(Rec.MainWeapon, NeedGold, NeedMat, bMaxed); }
            else if (TargetSlot == ESwitchSlot::Gun && Rec.Gun)
            { bHave = R->GetWeaponUpgradeInfo(Rec.Gun, NeedGold, NeedMat, bMaxed); }
            else if (TargetSlot == ESwitchSlot::Armor && Rec.Armor)
            { bHave = R->GetArmorUpgradeInfo(Rec.Armor, NeedGold, NeedMat, bMaxed); }
            else if (TargetSlot == ESwitchSlot::Chip && Rec.Chips.IsValidIndex(ChipSlot) && Rec.Chips[ChipSlot])
            { bHave = R->GetChipUpgradeInfo(Rec.Chips[ChipSlot], NeedGold, NeedMat, bMaxed); }
            else if (TargetSlot == ESwitchSlot::ArmorChip && Rec.Armor && Rec.Armor->SocketedChip)
            { bHave = R->GetChipUpgradeInfo(Rec.Armor->SocketedChip, NeedGold, NeedMat, bMaxed); }

            bHasItem = bHave;
            if (bHave)
            {
                if (bMaxed) { UpLabel = TEXT("MAX"); }
                else        { UpLabel = FString::Printf(TEXT("Up  %dg/%ds"), NeedGold, NeedMat); bClickable = true; }
            }
        }

        // Only show an Upgrade button when something is actually equipped in
        // this slot. Empty slots get no upgrade option.
        if (bHasItem)
        {
            URosterActionButton* Up = MakeButton(*UpLabel, ERosterAction::UpgradeSlot,
                bClickable ? ColParty2 * 0.6f : ColBtn);
            Up->MemberIndex = CurrentMemberIndex;
            Up->SwitchSlot = TargetSlot;
            Up->SlotIndex = ChipSlot;
            UHorizontalBoxSlot* US = Row->AddChildToHorizontalBox(Up);
            US->SetVerticalAlignment(VAlign_Center);
            US->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
        }
    }

    UVerticalBoxSlot* RS = DetailBox->AddChildToVerticalBox(Row);
    RS->SetPadding(FMargin(0.f, 2.f));
}

void URosterWidget::AddLoadoutBlock(const FPartyMemberRecord& Rec)
{
    UTextBlock* Hdr = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Hdr->SetText(FText::FromString(TEXT("Loadout  (switch any slot)")));
    Hdr->SetFont(MakeFont(18, true));
    Hdr->SetColorAndOpacity(FSlateColor(ColParty1));
    UVerticalBoxSlot* HS = DetailBox->AddChildToVerticalBox(Hdr);
    HS->SetPadding(FMargin(0.f, 18.f, 0.f, 6.f));

    auto WeaponValue = [this](UCharacterWeaponDataAsset* W) -> FString
    {
        return W ? FString::Printf(TEXT("%s  [Tier %s]  +%.0f %s"),
            *W->DisplayName.ToString(), WeaponTierName(static_cast<uint8>(W->CurrentTier)),
            W->GetCurrentBuffValue(), BuffedStatName(static_cast<uint8>(W->BuffedStat)))
            : FString(TEXT("(empty)"));
    };

    auto AddPassives = [this](UCharacterWeaponDataAsset* W)
    {
        if (!W) { return; }
        const int32 TierIdx = static_cast<int32>(W->CurrentTier);
        for (int32 t = 0; t <= TierIdx; ++t)
        {
            const FText P = W->GetPassiveForTier(t);
            if (P.IsEmpty()) { continue; }
            UTextBlock* PT = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            PT->SetText(FText::FromString(FString::Printf(TEXT("    %s: %s"),
                WeaponTierName(static_cast<uint8>(t)), *P.ToString())));
            PT->SetFont(MakeFont(12, false));
            PT->SetColorAndOpacity(FSlateColor(ColPassive));
            DetailBox->AddChildToVerticalBox(PT);
        }
    };

    AddLoadoutRow(TEXT("Main Weapon"), WeaponValue(Rec.MainWeapon), ESwitchSlot::MainWeapon, -1);
    AddPassives(Rec.MainWeapon);

    AddLoadoutRow(TEXT("Gun"), WeaponValue(Rec.Gun), ESwitchSlot::Gun, -1);
    AddPassives(Rec.Gun);

    AddLoadoutRow(TEXT("Armor"),
        Rec.Armor ? Rec.Armor->DisplayName.ToString() : FString(TEXT("(empty)")),
        ESwitchSlot::Armor, -1);

    // Armor's socketed chip — re-bind only at camp (bChangeCampOnly).
    {
        UCharacterChipDataAsset* AChip = Rec.Armor ? Rec.Armor->SocketedChip.Get() : nullptr;
        const FString AVal = !Rec.Armor
            ? FString(TEXT("(no armor)"))
            : (AChip ? FString::Printf(TEXT("%s  [L%d]"), *AChip->DisplayName.ToString(), AChip->CurrentLevel)
                     : FString(TEXT("(empty)")));
        AddLoadoutRow(TEXT("  Armor Chip"), AVal, ESwitchSlot::ArmorChip, -1, /*bChangeCampOnly=*/true);
    }

    for (int32 i = 0; i < APlayerCombatant::MaxChipSlots; ++i)
    {
        UCharacterChipDataAsset* Chip = Rec.Chips.IsValidIndex(i) ? Rec.Chips[i] : nullptr;
        const FString Val = Chip
            ? FString::Printf(TEXT("%s  [L%d]"), *Chip->DisplayName.ToString(), Chip->CurrentLevel)
            : FString(TEXT("(empty)"));
        AddLoadoutRow(FString::Printf(TEXT("Chip %d"), i + 1), Val, ESwitchSlot::Chip, i);
    }

    UTextBlock* Skills = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Skills->SetText(FText::FromString(FString::Printf(TEXT("Skills         %d / %d equipped"),
        Rec.EquippedSkillCount, APlayerCombatant::MaxEquippedSkills)));
    Skills->SetFont(MakeFont(15, false));
    Skills->SetColorAndOpacity(FSlateColor(ColText));
    UVerticalBoxSlot* SS = DetailBox->AddChildToVerticalBox(Skills);
    SS->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
}

// -----------------------------------------------------------------------------
//  Switch page
// -----------------------------------------------------------------------------

void URosterWidget::ShowSwitch(ESwitchSlot TargetSlot, int32 ChipSlot)
{
    URosterSubsystem* R = GetRoster();
    if (!SwitchBox || !R || !R->IsValidMember(CurrentMemberIndex)) { return; }

    CurrentSwitchSlot = TargetSlot;
    CurrentChipSlot = ChipSlot;
    SwitchBox->ClearChildren();

    UClass* CharClass = R->GetMember(CurrentMemberIndex).CharacterClass;

    // Title
    const TCHAR* SlotName =
        (TargetSlot == ESwitchSlot::MainWeapon) ? TEXT("Main Weapon") :
        (TargetSlot == ESwitchSlot::Gun)        ? TEXT("Gun") :
        (TargetSlot == ESwitchSlot::Armor)      ? TEXT("Armor") :
        (TargetSlot == ESwitchSlot::ArmorChip)  ? TEXT("Armor Chip") : TEXT("Chip");
    UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Title->SetText(FText::FromString(FString::Printf(TEXT("Choose %s"), SlotName)));
    Title->SetFont(MakeFont(20, true));
    Title->SetColorAndOpacity(FSlateColor(ColText));
    UVerticalBoxSlot* TS = SwitchBox->AddChildToVerticalBox(Title);
    TS->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

    // "Remove" option (equip nothing).
    {
        URosterActionButton* Btn = MakeButton(TEXT("(Remove / leave empty)"), ERosterAction::EquipItem, ColBtn);
        Btn->Payload = nullptr;
        UVerticalBoxSlot* S = SwitchBox->AddChildToVerticalBox(Btn);
        S->SetPadding(FMargin(0.f, 3.f));
    }

    auto AddItemBtn = [&](const FString& Label, UObject* Item)
    {
        URosterActionButton* Btn = MakeButton(*Label, ERosterAction::EquipItem, ColCardBG);
        Btn->Payload = Item;
        UVerticalBoxSlot* S = SwitchBox->AddChildToVerticalBox(Btn);
        S->SetPadding(FMargin(0.f, 3.f));
    };

    int32 Shown = 0;
    if (TargetSlot == ESwitchSlot::MainWeapon || TargetSlot == ESwitchSlot::Gun)
    {
        TArray<UCharacterWeaponDataAsset*> Weapons;
        if (TargetSlot == ESwitchSlot::MainWeapon) { R->GetAvailableMainWeapons(CharClass, Weapons); }
        else                                 { R->GetAvailableGuns(CharClass, Weapons); }
        for (UCharacterWeaponDataAsset* W : Weapons)
        {
            if (!W) { continue; }
            AddItemBtn(FString::Printf(TEXT("%s   [Tier %s]  +%.0f %s"),
                *W->DisplayName.ToString(), WeaponTierName(static_cast<uint8>(W->CurrentTier)),
                W->GetCurrentBuffValue(), BuffedStatName(static_cast<uint8>(W->BuffedStat))), W);
            ++Shown;
        }
    }
    else if (TargetSlot == ESwitchSlot::Armor)
    {
        TArray<UCharacterArmorDataAsset*> Armors;
        R->GetAvailableArmors(Armors);
        for (UCharacterArmorDataAsset* A : Armors)
        {
            if (!A) { continue; }
            AddItemBtn(A->DisplayName.ToString(), A);
            ++Shown;
        }
    }
    else // Chip or ArmorChip — separate owned-chip pools (no overlap)
    {
        TArray<UCharacterChipDataAsset*> Chips;
        R->GetAvailableChips(Chips, /*bArmorChips=*/ TargetSlot == ESwitchSlot::ArmorChip);
        for (UCharacterChipDataAsset* C : Chips)
        {
            if (!C) { continue; }
            AddItemBtn(FString::Printf(TEXT("%s   [L%d]"), *C->DisplayName.ToString(), C->CurrentLevel), C);
            ++Shown;
        }
    }

    if (Shown == 0)
    {
        UTextBlock* Empty = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Empty->SetText(FText::FromString(TEXT("No items of this type authored yet.")));
        Empty->SetFont(MakeFont(14, false));
        Empty->SetColorAndOpacity(FSlateColor(ColSubtle));
        SwitchBox->AddChildToVerticalBox(Empty);
    }

    if (Switcher) { Switcher->SetActiveWidgetIndex(2); }
}

// -----------------------------------------------------------------------------
//  Routing
// -----------------------------------------------------------------------------

void URosterWidget::HandleButton(URosterActionButton* Btn)
{
    if (!Btn) { return; }
    URosterSubsystem* R = GetRoster();

    switch (Btn->Action)
    {
        case ERosterAction::Close:
            if (OnCloseRequested) { OnCloseRequested(); }
            break;

        case ERosterAction::BackToRoster:
            if (Switcher) { Switcher->SetActiveWidgetIndex(0); }
            RefreshRoster();
            break;

        case ERosterAction::BackToDetail:
            ShowDetail(CurrentMemberIndex);
            break;

        case ERosterAction::OpenDetails:
            ShowDetail(Btn->MemberIndex);
            break;

        case ERosterAction::OpenSwitch:
            ShowSwitch(Btn->SwitchSlot, Btn->SlotIndex);
            break;

        case ERosterAction::UpgradeSlot:
        {
            if (R && R->IsValidMember(Btn->MemberIndex))
            {
                const FPartyMemberRecord& Rec = R->GetMember(Btn->MemberIndex);
                switch (Btn->SwitchSlot)
                {
                    case ESwitchSlot::MainWeapon: R->TryUpgradeWeapon(Rec.MainWeapon); break;
                    case ESwitchSlot::Gun:        R->TryUpgradeWeapon(Rec.Gun);        break;
                    case ESwitchSlot::Armor:      R->TryUpgradeArmor(Rec.Armor);       break;
                    case ESwitchSlot::Chip:
                        if (Rec.Chips.IsValidIndex(Btn->SlotIndex))
                        { R->TryUpgradeChip(Rec.Chips[Btn->SlotIndex]); }
                        break;
                    case ESwitchSlot::ArmorChip:
                        if (Rec.Armor) { R->TryUpgradeChip(Rec.Armor->SocketedChip); }
                        break;
                }
            }
            ShowDetail(CurrentMemberIndex);   // refresh costs + stats
            break;
        }

        case ERosterAction::EquipItem:
        {
            if (R && R->IsValidMember(CurrentMemberIndex))
            {
                UObject* Item = Btn->Payload.Get();
                switch (CurrentSwitchSlot)
                {
                    case ESwitchSlot::MainWeapon:
                        R->SetMemberMainWeapon(CurrentMemberIndex, Cast<UCharacterWeaponDataAsset>(Item));
                        break;
                    case ESwitchSlot::Gun:
                        R->SetMemberGun(CurrentMemberIndex, Cast<UCharacterWeaponDataAsset>(Item));
                        break;
                    case ESwitchSlot::Armor:
                        R->SetMemberArmor(CurrentMemberIndex, Cast<UCharacterArmorDataAsset>(Item));
                        break;
                    case ESwitchSlot::Chip:
                        R->SetMemberChip(CurrentMemberIndex, CurrentChipSlot, Cast<UCharacterChipDataAsset>(Item));
                        break;
                    case ESwitchSlot::ArmorChip:
                        R->SetMemberArmorChip(CurrentMemberIndex, Cast<UCharacterChipDataAsset>(Item));
                        break;
                }
            }
            ShowDetail(CurrentMemberIndex);   // refresh detail with the new gear
            break;
        }

        case ERosterAction::AssignParty1:
        case ERosterAction::AssignParty2:
        case ERosterAction::AssignBench:
        {
            if (R)
            {
                const EPartyAssignment Target =
                    (Btn->Action == ERosterAction::AssignParty1) ? EPartyAssignment::Party1 :
                    (Btn->Action == ERosterAction::AssignParty2) ? EPartyAssignment::Party2 :
                                                                  EPartyAssignment::Bench;
                R->SetAssignment(Btn->MemberIndex, Target);
            }
            RefreshRoster();
            break;
        }
    }
}
