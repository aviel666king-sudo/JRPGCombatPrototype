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
    if (OnActionClicked)
    {
        OnActionClicked(MemberIndex, Action);
    }
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

        URosterActionButton* Close = WidgetTree->ConstructWidget<URosterActionButton>(URosterActionButton::StaticClass());
        Close->SetBackgroundColor(ColBtn);
        Close->Action = ERosterAction::Close;
        Close->OnActionClicked = [this](int32 I, ERosterAction A) { HandleAction(I, A); };
        Close->OnClicked.AddDynamic(Close, &URosterActionButton::HandleClicked);
        UTextBlock* CloseLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        CloseLbl->SetText(FText::FromString(TEXT("Close  (Tab / Esc)")));
        CloseLbl->SetFont(MakeFont(15, true));
        CloseLbl->SetColorAndOpacity(FSlateColor(ColText));
        Close->SetContent(CloseLbl);
        Footer->AddChildToHorizontalBox(Close);

        UVerticalBoxSlot* FS = Page->AddChildToVerticalBox(Footer);
        FS->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));

        Switcher->AddChild(Page);
    }

    // ---- Page 1: Detail ----
    {
        UVerticalBox* Page = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailPage"));

        URosterActionButton* Back = WidgetTree->ConstructWidget<URosterActionButton>(URosterActionButton::StaticClass());
        Back->SetBackgroundColor(ColBtn);
        Back->Action = ERosterAction::BackToRoster;
        Back->OnActionClicked = [this](int32 I, ERosterAction A) { HandleAction(I, A); };
        Back->OnClicked.AddDynamic(Back, &URosterActionButton::HandleClicked);
        UTextBlock* BackLbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        BackLbl->SetText(FText::FromString(TEXT("Back to Roster")));
        BackLbl->SetFont(MakeFont(15, true));
        BackLbl->SetColorAndOpacity(FSlateColor(ColText));
        Back->SetContent(BackLbl);
        UVerticalBoxSlot* BS = Page->AddChildToVerticalBox(Back);
        BS->SetHorizontalAlignment(HAlign_Left);
        BS->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

        DetailBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DetailBox"));
        UVerticalBoxSlot* DS = Page->AddChildToVerticalBox(DetailBox);
        DS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        Switcher->AddChild(Page);
    }

    return Super::RebuildWidget();
}

void URosterWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // If this level has live party actors (a combat level), sync their current
    // HP into the records first so the roster shows post-combat HP, not the
    // seed-time values.
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
        if (Switcher && Switcher->GetActiveWidgetIndex() == 1)
        {
            Switcher->SetActiveWidgetIndex(0);
            return FReply::Handled();
        }
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

    // Identity
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

    // Party tag
    const EPartyAssignment Assignment = Rec.Assignment;
    UBorder* Tag = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    Tag->SetBrushColor(AssignmentColour(Assignment));
    Tag->SetPadding(FMargin(10.f, 4.f));
    UTextBlock* TagText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    TagText->SetText(AssignmentLabel(Assignment));
    TagText->SetFont(MakeFont(13, true));
    TagText->SetColorAndOpacity(FSlateColor(FLinearColor::Black));
    Tag->SetContent(TagText);
    UHorizontalBoxSlot* TagSlot = Row->AddChildToHorizontalBox(Tag);
    TagSlot->SetVerticalAlignment(VAlign_Center);
    TagSlot->SetPadding(FMargin(0.f, 0.f, 14.f, 0.f));

    // Action buttons
    auto MakeBtn = [this, MemberIndex](const TCHAR* Label, ERosterAction Action, const FLinearColor& BG)
    {
        URosterActionButton* Btn = WidgetTree->ConstructWidget<URosterActionButton>(URosterActionButton::StaticClass());
        Btn->SetBackgroundColor(BG);
        Btn->MemberIndex = MemberIndex;
        Btn->Action = Action;
        Btn->OnActionClicked = [this](int32 I, ERosterAction A) { HandleAction(I, A); };
        Btn->OnClicked.AddDynamic(Btn, &URosterActionButton::HandleClicked);
        UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Lbl->SetText(FText::FromString(Label));
        Lbl->SetFont(MakeFont(13, true));
        Lbl->SetColorAndOpacity(FSlateColor(ColText));
        Btn->SetContent(Lbl);
        return Btn;
    };

    URosterActionButton* B1 = MakeBtn(TEXT("P1"),      ERosterAction::AssignParty1, ColParty1 * 0.5f);
    URosterActionButton* B2 = MakeBtn(TEXT("P2"),      ERosterAction::AssignParty2, ColParty2 * 0.5f);
    URosterActionButton* BB = MakeBtn(TEXT("Bench"),   ERosterAction::AssignBench,  ColBtn);
    URosterActionButton* BD = MakeBtn(TEXT("Details"), ERosterAction::OpenDetails,  ColBtn);

    for (URosterActionButton* B : { B1, B2, BB, BD })
    {
        UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(B);
        BS->SetPadding(FMargin(3.f, 0.f));
        BS->SetVerticalAlignment(VAlign_Center);
    }

    return Card;
}

// -----------------------------------------------------------------------------
//  Detail page
// -----------------------------------------------------------------------------

void URosterWidget::ShowDetail(int32 MemberIndex)
{
    URosterSubsystem* R = GetRoster();
    if (!DetailBox || !R || !R->IsValidMember(MemberIndex)) { return; }

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

    if (!Rec.Tagline.IsEmpty())
    {
        UTextBlock* Tag = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        Tag->SetText(Rec.Tagline);
        Tag->SetFont(MakeFont(13, false));
        Tag->SetColorAndOpacity(FSlateColor(ColSubtle));
        UVerticalBoxSlot* S = DetailBox->AddChildToVerticalBox(Tag);
        S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
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

void URosterWidget::AddLoadoutBlock(const FPartyMemberRecord& Rec)
{
    UTextBlock* Hdr = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Hdr->SetText(FText::FromString(TEXT("Loadout")));
    Hdr->SetFont(MakeFont(18, true));
    Hdr->SetColorAndOpacity(FSlateColor(ColParty1));
    UVerticalBoxSlot* HS = DetailBox->AddChildToVerticalBox(Hdr);
    HS->SetPadding(FMargin(0.f, 18.f, 0.f, 6.f));

    auto AddLine = [this](const FString& Label, const FString& Value)
    {
        UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        T->SetText(FText::FromString(FString::Printf(TEXT("%-14s %s"), *Label, *Value)));
        T->SetFont(MakeFont(15, false));
        T->SetColorAndOpacity(FSlateColor(ColText));
        DetailBox->AddChildToVerticalBox(T);
    };

    if (UCharacterWeaponDataAsset* W = Rec.MainWeapon)
    {
        AddLine(TEXT("Main Weapon"), FString::Printf(TEXT("%s  [Tier %s]  +%.0f %s"),
            *W->DisplayName.ToString(), WeaponTierName(static_cast<uint8>(W->CurrentTier)),
            W->GetCurrentBuffValue(), BuffedStatName(static_cast<uint8>(W->BuffedStat))));
    }
    else { AddLine(TEXT("Main Weapon"), TEXT("(empty)")); }

    if (UCharacterWeaponDataAsset* G = Rec.Gun)
    {
        AddLine(TEXT("Gun"), FString::Printf(TEXT("%s  [Tier %s]  +%.0f %s"),
            *G->DisplayName.ToString(), WeaponTierName(static_cast<uint8>(G->CurrentTier)),
            G->GetCurrentBuffValue(), BuffedStatName(static_cast<uint8>(G->BuffedStat))));
    }
    else { AddLine(TEXT("Gun"), TEXT("(empty)")); }

    if (UCharacterArmorDataAsset* A = Rec.Armor)
    {
        AddLine(TEXT("Armor"), A->DisplayName.ToString());
        if (A->SocketedChip)
        {
            AddLine(TEXT("  Armor Chip"), FString::Printf(TEXT("%s  [L%d]"),
                *A->SocketedChip->DisplayName.ToString(), A->SocketedChip->CurrentLevel));
        }
    }
    else { AddLine(TEXT("Armor"), TEXT("(empty)")); }

    for (int32 i = 0; i < APlayerCombatant::MaxChipSlots; ++i)
    {
        const FString Label = FString::Printf(TEXT("Chip %d"), i + 1);
        UCharacterChipDataAsset* Chip = Rec.Chips.IsValidIndex(i) ? Rec.Chips[i] : nullptr;
        if (Chip)
        {
            AddLine(Label, FString::Printf(TEXT("%s  [L%d]"),
                *Chip->DisplayName.ToString(), Chip->CurrentLevel));
        }
        else { AddLine(Label, TEXT("(empty)")); }
    }

    AddLine(TEXT("Skills"), FString::Printf(TEXT("%d / %d equipped"),
        Rec.EquippedSkillCount, APlayerCombatant::MaxEquippedSkills));

    UTextBlock* Note = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    Note->SetText(FText::FromString(TEXT("Gear switching + upgrade requirements: coming next.")));
    Note->SetFont(MakeFont(12, false));
    Note->SetColorAndOpacity(FSlateColor(ColSubtle));
    UVerticalBoxSlot* NS = DetailBox->AddChildToVerticalBox(Note);
    NS->SetPadding(FMargin(0.f, 18.f, 0.f, 0.f));
}

// -----------------------------------------------------------------------------
//  Actions
// -----------------------------------------------------------------------------

void URosterWidget::HandleAction(int32 MemberIndex, ERosterAction Action)
{
    switch (Action)
    {
        case ERosterAction::Close:
            if (OnCloseRequested) { OnCloseRequested(); }
            break;

        case ERosterAction::BackToRoster:
            if (Switcher) { Switcher->SetActiveWidgetIndex(0); }
            RefreshRoster();
            break;

        case ERosterAction::OpenDetails:
            ShowDetail(MemberIndex);
            break;

        case ERosterAction::AssignParty1:
        case ERosterAction::AssignParty2:
        case ERosterAction::AssignBench:
        {
            URosterSubsystem* R = GetRoster();
            if (R)
            {
                const EPartyAssignment Target =
                    (Action == ERosterAction::AssignParty1) ? EPartyAssignment::Party1 :
                    (Action == ERosterAction::AssignParty2) ? EPartyAssignment::Party2 :
                                                              EPartyAssignment::Bench;
                R->SetAssignment(MemberIndex, Target);
            }
            RefreshRoster();
            break;
        }
    }
}
