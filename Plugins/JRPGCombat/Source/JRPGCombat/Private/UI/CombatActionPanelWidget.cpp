#include "UI/CombatActionPanelWidget.h"
#include "Core/BattleManager.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/AbilityManagerComponent.h"
#include "Components/ProtocolManagerComponent.h"
#include "Abilities/CombatAbility.h"
#include "UI/CombatHUDWidget.h"
#include "UI/Minigames/AbilityMinigameWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"
#include "InputCoreTypes.h"

// -----------------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------------

TSharedRef<SWidget> UCombatActionPanelWidget::RebuildWidget()
{
    // Build the panel into a fresh root when there's no WBP layout (the HUD
    // constructs this directly from the C++ class).
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildPanelLayout();
    }
    return Super::RebuildWidget();
}

void UCombatActionPanelWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // ── Bind main-menu buttons ────────────────────────────────────────────────
    if (MeleeButton)    { MeleeButton->OnClicked.AddDynamic(this,    &UCombatActionPanelWidget::OnMeleeClicked); }
    // GunButton is intentionally not bound — gun aim is now triggered by holding RMB,
    // not by clicking a button. The button stays as a visual hint ("Hold RMB to Aim").
    if (SkillButton)    { SkillButton->OnClicked.AddDynamic(this,    &UCombatActionPanelWidget::OnSkillMenuClicked); }
    if (ProtocolButton) { ProtocolButton->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnProtocolMenuClicked); }
    if (SkipTurnButton) { SkipTurnButton->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkipTurnClicked); }

    // ── Bind skill submenu back button ────────────────────────────────────────
    if (BackButtonSkill) { BackButtonSkill->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnBackFromSkillClicked); }

    // ── Bind protocol submenu buttons ─────────────────────────────────────────
    if (BackButtonProtocol) { BackButtonProtocol->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnBackFromProtocolClicked); }
    if (HealingButton)      { HealingButton->OnClicked.AddDynamic(this,      &UCombatActionPanelWidget::OnHealingProtocolClicked); }
    if (RevivalButton)      { RevivalButton->OnClicked.AddDynamic(this,      &UCombatActionPanelWidget::OnRevivalProtocolClicked); }
    if (APButton)           { APButton->OnClicked.AddDynamic(this,           &UCombatActionPanelWidget::OnAPProtocolClicked); }

    // ── Bind target selection cancel button ───────────────────────────────────
    if (BackButtonTarget) { BackButtonTarget->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnBackFromTargetClicked); }

    // Start inactive until a player turn begins.
    SetMenuState(ECombatMenuState::Inactive);
}

// -----------------------------------------------------------------------------
//  Layout (built in C++ — no WBP layout needed)
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::BuildPanelLayout()
{
    if (!WidgetTree) { return; }

    auto MakeFont = [](int32 Size, bool bBold)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
    };

    const FLinearColor BtnBG (0.16f, 0.18f, 0.26f, 1.f);
    const FLinearColor NameCol(0.97f, 0.97f, 1.f, 1.f);
    const FLinearColor KeyCol (1.f, 0.82f, 0.30f, 1.f);   // yellow hotkeys

    // Builds a button whose content is [Name] over an optional [hotkey] line.
    auto MakeButton = [&](const FString& Name, const FString& Hotkey,
                          TObjectPtr<UTextBlock>* OutSubText = nullptr) -> UButton*
    {
        UButton* B = WidgetTree->ConstructWidget<UButton>();
        B->SetBackgroundColor(BtnBG);

        UVerticalBox* V = WidgetTree->ConstructWidget<UVerticalBox>();

        UTextBlock* NameTxt = WidgetTree->ConstructWidget<UTextBlock>();
        NameTxt->SetText(FText::FromString(Name));
        NameTxt->SetFont(MakeFont(15, true));
        NameTxt->SetColorAndOpacity(NameCol);
        NameTxt->SetJustification(ETextJustify::Center);
        V->AddChildToVerticalBox(NameTxt);

        if (!Hotkey.IsEmpty() || OutSubText)
        {
            UTextBlock* Sub = WidgetTree->ConstructWidget<UTextBlock>();
            Sub->SetText(FText::FromString(Hotkey));
            Sub->SetFont(MakeFont(11, true));
            Sub->SetColorAndOpacity(KeyCol);
            Sub->SetJustification(ETextJustify::Center);
            V->AddChildToVerticalBox(Sub);
            if (OutSubText) { *OutSubText = Sub; }
        }

        B->SetContent(V);
        return B;
    };

    MenuSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>();

    // ── Slot 0: Main menu — 5 buttons in a row ────────────────────────────────
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        auto AddMain = [&](UButton* B)
        {
            UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(B);
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetPadding(FMargin(4.f, 0.f));
        };
        MeleeButton    = MakeButton(TEXT("Melee"),    TEXT("[1]"));        AddMain(MeleeButton);
        GunButton      = MakeButton(TEXT("Gun"),      TEXT("[RMB to Aim]")); AddMain(GunButton);
        SkillButton    = MakeButton(TEXT("Skill"),    TEXT("[3]"));        AddMain(SkillButton);
        ProtocolButton = MakeButton(TEXT("Protocol"), TEXT("[4]"));        AddMain(ProtocolButton);
        SkipTurnButton = MakeButton(TEXT("Skip Turn"),TEXT("[T]"));        AddMain(SkipTurnButton);
        MenuSwitcher->AddChild(Row);
    }

    // ── Slot 1: Skill menu — Back + horizontal skill row ──────────────────────
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

        BackButtonSkill = MakeButton(TEXT("Back"), TEXT("[Backspace]"));
        UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(BackButtonSkill);
        BS->SetPadding(FMargin(4.f, 0.f, 12.f, 0.f));

        SkillListBox = WidgetTree->ConstructWidget<UHorizontalBox>();
        UHorizontalBoxSlot* LS = Row->AddChildToHorizontalBox(SkillListBox);
        LS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        MenuSwitcher->AddChild(Row);
    }

    // ── Slot 2: Protocol menu — Back + 3 protocols (name + charges) ───────────
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        auto AddProto = [&](UButton* B)
        {
            UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(B);
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetPadding(FMargin(4.f, 0.f));
        };

        BackButtonProtocol = MakeButton(TEXT("Back"), TEXT("[Backspace]"));
        UHorizontalBoxSlot* BS = Row->AddChildToHorizontalBox(BackButtonProtocol);
        BS->SetPadding(FMargin(4.f, 0.f, 12.f, 0.f));

        HealingButton = MakeButton(TEXT("Healing [1]"), FString(), &HealingChargesText); AddProto(HealingButton);
        RevivalButton = MakeButton(TEXT("Revival [2]"), FString(), &RevivalChargesText); AddProto(RevivalButton);
        APButton      = MakeButton(TEXT("AP [3]"),      FString(), &APChargesText);      AddProto(APButton);

        MenuSwitcher->AddChild(Row);
    }

    // ── Slot 3: Target selection — hint + Cancel ──────────────────────────────
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

        TargetHintText = WidgetTree->ConstructWidget<UTextBlock>();
        TargetHintText->SetText(FText::FromString(TEXT("Select Target  —  A / D  •  Space confirm  •  Backspace cancel")));
        TargetHintText->SetFont(MakeFont(14, true));
        TargetHintText->SetColorAndOpacity(NameCol);
        UHorizontalBoxSlot* HS = Row->AddChildToHorizontalBox(TargetHintText);
        HS->SetVerticalAlignment(VAlign_Center);
        HS->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        BackButtonTarget = MakeButton(TEXT("Cancel"), TEXT("[Backspace]"));
        Row->AddChildToHorizontalBox(BackButtonTarget);

        MenuSwitcher->AddChild(Row);
    }

    // Dark panel background wrapping the switcher.
    UBorder* Bg = WidgetTree->ConstructWidget<UBorder>();
    Bg->SetBrushColor(FLinearColor(0.04f, 0.05f, 0.08f, 0.92f));
    Bg->SetPadding(FMargin(14.f, 10.f));
    Bg->SetContent(MenuSwitcher);

    // Attach to the widget root (replace whatever the WBP had).
    UWidget* Root = GetRootWidget();
    if (UPanelWidget* P = Cast<UPanelWidget>(Root))
    {
        P->ClearChildren();
        P->AddChild(Bg);
    }
    else if (UContentWidget* CW = Cast<UContentWidget>(Root))
    {
        CW->SetContent(Bg);
    }
    else
    {
        WidgetTree->RootWidget = Bg;
    }
}

// -----------------------------------------------------------------------------
//  Initialization
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::InitializePanel(ABattleManager* InBattleManager,
                                                UCombatHUDWidget* InOwnerHUD)
{
    BattleManager = InBattleManager;
    OwnerHUD      = InOwnerHUD;

    // Subscribe to the minigame signal — fires after target confirmation for
    // any Skill ability that has a MinigameClass configured.
    if (InBattleManager)
    {
        InBattleManager->OnSkillMinigameShouldStart.AddDynamic(
            this, &UCombatActionPanelWidget::OnBMSkillMinigameShouldStart);
    }

    SetMenuState(ECombatMenuState::Inactive);
}

// -----------------------------------------------------------------------------
//  State machine
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::SetMenuState(ECombatMenuState NewState)
{
    // Track where we came from when entering target selection — but only on the
    // FIRST transition into it. BeginTargetSelection re-broadcasts a phase change
    // that calls SetMenuState(SelectingTarget) again; without this guard that
    // second call would overwrite PreTargetState with SelectingTarget itself,
    // making the first Backspace a no-op (requiring two presses to back out).
    if (NewState == ECombatMenuState::SelectingTarget
        && CurrentState != ECombatMenuState::SelectingTarget)
    {
        PreTargetState = CurrentState;
    }

    // Gun mode is only valid while the player is actively in SelectingTarget.
    // Clear it whenever we transition to any other state so that backing out,
    // a turn ending, or an enemy turn all exit gun mode cleanly.
    if (NewState != ECombatMenuState::SelectingTarget)
    {
        bInGunMode            = false;
        StoredGunAbilityIndex = -1;
    }

    // If a minigame was running and we're transitioning away from it forcibly
    // (e.g. enemy turn fires and sets Inactive), clean up the widget.
    if (CurrentState == ECombatMenuState::MinigameActive
        && NewState != ECombatMenuState::MinigameActive)
    {
        if (ActiveMinigameWidget)
        {
            ActiveMinigameWidget->OnMinigameCompleted.RemoveAll(this);
            ActiveMinigameWidget->RemoveFromParent();
            ActiveMinigameWidget = nullptr;
        }
    }

    CurrentState = NewState;

    // Fade / unfade the entire panel.
    // MinigameActive dims the panel (minigame overlay is the focus).
    const bool bDimmed = (NewState == ECombatMenuState::Inactive
                       || NewState == ECombatMenuState::MinigameActive);
    SetRenderOpacity(bDimmed ? 0.35f : 1.0f);
    SetIsEnabled(NewState != ECombatMenuState::Inactive
              && NewState != ECombatMenuState::MinigameActive);

    // Refresh dynamic content before switching.
    switch (NewState)
    {
        case ECombatMenuState::MainMenu:
            RefreshMainMenuButtons();
            // Proactively recapture HUD keyboard focus whenever we enter MainMenu.
            // Buttons becoming visible/enabled can steal Slate focus away from the
            // HUD widget, which would stop NativeOnKeyDown from firing.
            if (OwnerHUD.IsValid())
            {
                if (APlayerController* PC = GetOwningPlayer()) { OwnerHUD->SetUserFocus(PC); }
            }
            break;
        case ECombatMenuState::SkillMenu:
            RefreshSkillMenu();
            break;
        case ECombatMenuState::ProtocolMenu:
            RefreshProtocolMenu();
            break;
        case ECombatMenuState::SelectingTarget:
            // Recapture HUD keyboard focus as soon as we enter target selection.
            // This covers the common path where focus was never lost, and also
            // pre-empts the alt-tab scenario so input works immediately.
            if (OwnerHUD.IsValid())
            {
                if (APlayerController* PC = GetOwningPlayer()) { OwnerHUD->SetUserFocus(PC); }
            }
            break;
        default:
            break;
    }

    ApplySwitcherIndex();
}

void UCombatActionPanelWidget::ApplySwitcherIndex()
{
    if (!MenuSwitcher) { return; }

    int32 Index = 0;
    switch (CurrentState)
    {
        case ECombatMenuState::Inactive:
        case ECombatMenuState::MainMenu:        Index = 0; break;
        case ECombatMenuState::SkillMenu:
        case ECombatMenuState::MinigameActive:  Index = 1; break; // panel dimmed; skill list stays visible behind overlay
        case ECombatMenuState::ProtocolMenu:    Index = 2; break;
        case ECombatMenuState::SelectingTarget: Index = 3; break;
    }

    MenuSwitcher->SetActiveWidgetIndex(Index);
}

// -----------------------------------------------------------------------------
//  Keyboard input (forwarded from CombatHUDWidget)
// -----------------------------------------------------------------------------

bool UCombatActionPanelWidget::HandleKeyDown(const FKey& Key)
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return false; }

    switch (CurrentState)
    {
        // ── Main menu ─────────────────────────────────────────────────────────
        case ECombatMenuState::MainMenu:
        {
            if (Key == EKeys::One)   { OnMeleeClicked();        return true; }
            // [2] removed — gun aim is now triggered by holding RMB
            if (Key == EKeys::Three) { OnSkillMenuClicked();     return true; }
            if (Key == EKeys::Four)  { OnProtocolMenuClicked();  return true; }
            if (Key == EKeys::T)     { OnSkipTurnClicked();      return true; }
            return false;
        }

        // ── Skill submenu ─────────────────────────────────────────────────────
        case ECombatMenuState::SkillMenu:
        {
            // Z / X / C / V / B / N select skills 1–6.
            const TArray<FKey> SkillKeys = {
                EKeys::Z, EKeys::X, EKeys::C, EKeys::V, EKeys::B, EKeys::N
            };
            for (int32 i = 0; i < SkillKeys.Num(); ++i)
            {
                if (Key == SkillKeys[i] && SkillButtonAbilityIndices.IsValidIndex(i))
                {
                    const int32 AbilIdx = SkillButtonAbilityIndices[i];
                    if (BM->GetCurrentPhase() == EBattlePhase::AwaitingInput)
                    {
                        ACombatantBase* Actor = BM->GetActiveCombatant();
                        if (Actor && Actor->AbilityManager->CanActivateAbility(AbilIdx))
                        {
                            // Enter target selection — minigame fires AFTER target is confirmed.
                            SetMenuState(ECombatMenuState::SelectingTarget);
                            BM->BeginTargetSelection(AbilIdx);
                        }
                    }
                    return true;
                }
            }
            if (Key == EKeys::BackSpace) { OnBackFromSkillClicked(); return true; }
            return false;
        }

        // ── Protocol submenu ──────────────────────────────────────────────────
        case ECombatMenuState::ProtocolMenu:
        {
            if (Key == EKeys::Z)         { OnHealingProtocolClicked();  return true; }
            if (Key == EKeys::X)         { OnRevivalProtocolClicked();  return true; }
            if (Key == EKeys::C)         { OnAPProtocolClicked();       return true; }
            if (Key == EKeys::BackSpace) { OnBackFromProtocolClicked(); return true; }
            return false;
        }

        // ── Minigame active — forward SPACE as fallback; Backspace cancels ──────
        case ECombatMenuState::MinigameActive:
        {
            if (Key == EKeys::SpaceBar && ActiveMinigameWidget)
            {
                ActiveMinigameWidget->OnConfirmPressed();
                return true;
            }
            if (Key == EKeys::BackSpace && ActiveMinigameWidget)
            {
                // Cancel the minigame — widget calls OnMinigameCompleted(0.0)
                // which aborts skill execution and returns to the skill menu.
                ActiveMinigameWidget->CancelMinigame();
                return true;
            }
            return true; // swallow everything else; minigame owns input
        }

        // ── Target selection — navigation + confirm + cancel ───────────────────
        case ECombatMenuState::SelectingTarget:
        {
            if (Key == EKeys::A)
            {
                BM->NavigateTargets(-1);
                return true;
            }
            if (Key == EKeys::D)
            {
                BM->NavigateTargets(+1);
                return true;
            }
            if (Key == EKeys::SpaceBar || Key == EKeys::Enter)
            {
                BM->ConfirmTargetSelection();
                return true;
            }
            if (Key == EKeys::BackSpace)
            {
                BM->CancelTargetSelection();
                // Return to whichever menu was open before targeting started.
                SetMenuState(PreTargetState);
                // Buttons re-enabling may steal Slate focus — recapture the HUD.
                if (OwnerHUD.IsValid())
                {
                    if (APlayerController* PC = GetOwningPlayer()) { OwnerHUD->SetUserFocus(PC); }
                }
                return true;
            }
            return false;
        }

        default:
            return false;
    }
}

// -----------------------------------------------------------------------------
//  Dynamic content refresh
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::RefreshMainMenuButtons()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return; }

    ACombatantBase* Actor = BM->GetActiveCombatant();

    // Melee — enabled if acting character has a tagged Melee ability.
    if (MeleeButton)
    {
        const int32 Idx = FindMeleeAbilityIndex();
        const bool bCanMelee = Idx >= 0 && Actor &&
                               Actor->AbilityManager->CanActivateAbility(Idx);
        MeleeButton->SetIsEnabled(bCanMelee);
    }

    // Gun — displayed as a passive "Hold RMB to Aim" hint; always non-interactive.
    if (GunButton) { GunButton->SetIsEnabled(false); }

    // Skill — enabled if at least one Skill-category ability is available.
    if (SkillButton)
    {
        SkillButton->SetIsEnabled(HasSkillAbilities());
    }

    // Protocol — enabled if any protocol has charges remaining.
    if (ProtocolButton && BM->ProtocolManager)
    {
        const bool bHasCharges =
            BM->ProtocolManager->CanSpend(EProtocolType::Healing) ||
            BM->ProtocolManager->CanSpend(EProtocolType::Revival) ||
            BM->ProtocolManager->CanSpend(EProtocolType::AP);
        ProtocolButton->SetIsEnabled(bHasCharges);
    }

    // Skip Turn is always enabled.
    if (SkipTurnButton) { SkipTurnButton->SetIsEnabled(true); }
}

void UCombatActionPanelWidget::RefreshSkillMenu()
{
    if (!SkillListBox) { return; }

    SkillListBox->ClearChildren();
    SkillButtonAbilityIndices.Empty();

    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return; }

    ACombatantBase* Actor = BM->GetActiveCombatant();
    if (!Actor || !Actor->AbilityManager) { return; }

    const int32 AbilityCount = Actor->AbilityManager->GetAbilityCount();

    for (int32 i = 0; i < AbilityCount; ++i)
    {
        const UCombatAbility* Ability = Actor->AbilityManager->GetAbility(i);
        if (!Ability || Ability->AbilityCategory != EAbilityCategory::Skill) { continue; }

        const int32 SlotIndex = SkillButtonAbilityIndices.Num();  // 0-based position in skill list
        SkillButtonAbilityIndices.Add(i);

        // Build button label: "1. Skill Name   AP: 2"
        const bool bCanUse = Actor->AbilityManager->CanActivateAbility(i);

        FString APCostStr;
        for (const FAbilityCost& Cost : Ability->Costs)
        {
            if (Cost.ResourceType == EResourceType::AP)
            {
                APCostStr = FString::Printf(TEXT("  AP: %d"), FMath::FloorToInt(Cost.Amount));
                break;
            }
        }

        const FString LabelStr = FString::Printf(TEXT("%d. %s%s"),
            SlotIndex + 1,
            *Ability->DisplayName.ToString(),
            *APCostStr);

        // Create a Button with a centered, wrapping TextBlock child.
        UButton* Btn = NewObject<UButton>(this);
        Btn->SetBackgroundColor(FLinearColor(0.16f, 0.18f, 0.26f, 1.f));
        UTextBlock* Label = NewObject<UTextBlock>(this);
        Label->SetText(FText::FromString(LabelStr));
        Label->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 12));
        Label->SetJustification(ETextJustify::Center);
        Label->SetAutoWrapText(true);
        Label->SetColorAndOpacity(bCanUse
            ? FSlateColor(FLinearColor::White)
            : FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f, 1.f)));
        Btn->AddChild(Label);
        Btn->SetIsEnabled(bCanUse);

        // Bind click — FOnButtonClickedEvent is a dynamic multicast delegate
        // and does not support AddLambda. Use pre-declared UFUNCTION dispatchers.
        switch (SlotIndex)
        {
            case 0: Btn->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkillSlot0Clicked); break;
            case 1: Btn->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkillSlot1Clicked); break;
            case 2: Btn->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkillSlot2Clicked); break;
            case 3: Btn->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkillSlot3Clicked); break;
            case 4: Btn->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkillSlot4Clicked); break;
            case 5: Btn->OnClicked.AddDynamic(this, &UCombatActionPanelWidget::OnSkillSlot5Clicked); break;
            default: break;
        }

        UHorizontalBoxSlot* HSlot = SkillListBox->AddChildToHorizontalBox(Btn);
        if (HSlot)
        {
            HSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            HSlot->SetPadding(FMargin(3.f, 0.f));
            HSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }
}

void UCombatActionPanelWidget::RefreshProtocolMenu()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || !BM->ProtocolManager) { return; }

    UProtocolManagerComponent* PM = BM->ProtocolManager;

    auto UpdateButton = [&](UButton* Btn, UTextBlock* ChargeText, EProtocolType Type)
    {
        if (!Btn) { return; }
        const bool bCanUse = PM->CanSpend(Type);
        Btn->SetIsEnabled(bCanUse);
        if (ChargeText)
        {
            ChargeText->SetText(FText::FromString(
                FString::Printf(TEXT("Charges: %d / %d"),
                    PM->GetCurrentCharges(Type),
                    PM->GetMaxCharges(Type))));
        }
    };

    UpdateButton(HealingButton, HealingChargesText, EProtocolType::Healing);
    UpdateButton(RevivalButton, RevivalChargesText, EProtocolType::Revival);
    UpdateButton(APButton,      APChargesText,      EProtocolType::AP);
}

// -----------------------------------------------------------------------------
//  Button callbacks — Main Menu
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::OnMeleeClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }

    const int32 Idx = FindMeleeAbilityIndex();
    if (Idx < 0) { return; }

    SetMenuState(ECombatMenuState::SelectingTarget);
    BM->BeginTargetSelection(Idx);
}

void UCombatActionPanelWidget::OnGunClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }

    const int32 Idx = FindGunAbilityIndex();
    if (Idx < 0) { return; }

    // Enter persistent Gun mode — after each shot we stay in targeting
    // until the player presses Backspace to explicitly exit.
    bInGunMode = true;
    StoredGunAbilityIndex = Idx;

    SetMenuState(ECombatMenuState::SelectingTarget);
    BM->BeginTargetSelection(Idx);
}

void UCombatActionPanelWidget::OnSkillMenuClicked()
{
    if (CurrentState != ECombatMenuState::MainMenu) { return; }
    SetMenuState(ECombatMenuState::SkillMenu);
}

void UCombatActionPanelWidget::OnProtocolMenuClicked()
{
    if (CurrentState != ECombatMenuState::MainMenu) { return; }
    SetMenuState(ECombatMenuState::ProtocolMenu);
}

void UCombatActionPanelWidget::OnSkipTurnClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }
    BM->RequestSkipTurn();
}

// -----------------------------------------------------------------------------
//  Button callbacks — Skill Menu
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::OnBackFromSkillClicked()
{
    SetMenuState(ECombatMenuState::MainMenu);
    // Buttons becoming enabled steal Slate focus from the HUD.
    // Recapture immediately so keyboard shortcuts keep working.
    if (OwnerHUD.IsValid())
    {
        if (APlayerController* PC = GetOwningPlayer()) { OwnerHUD->SetUserFocus(PC); }
    }
}

void UCombatActionPanelWidget::OnSkillButtonClicked(int32 SlotIndex)
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }
    if (!SkillButtonAbilityIndices.IsValidIndex(SlotIndex)) { return; }

    const int32 AbilIdx = SkillButtonAbilityIndices[SlotIndex];
    ACombatantBase* Actor = BM->GetActiveCombatant();
    if (!Actor || !Actor->AbilityManager->CanActivateAbility(AbilIdx)) { return; }

    // Enter target selection — the minigame fires AFTER the player confirms
    // a target, triggered by BM->OnSkillMinigameShouldStart.
    SetMenuState(ECombatMenuState::SelectingTarget);
    BM->BeginTargetSelection(AbilIdx);
}

// -----------------------------------------------------------------------------
//  Button callbacks — Protocol Menu
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::OnBackFromProtocolClicked()
{
    SetMenuState(ECombatMenuState::MainMenu);
    // Same focus-theft issue as OnBackFromSkillClicked — recapture HUD focus.
    if (OwnerHUD.IsValid())
    {
        if (APlayerController* PC = GetOwningPlayer()) { OwnerHUD->SetUserFocus(PC); }
    }
}

void UCombatActionPanelWidget::OnBackFromTargetClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (BM) { BM->CancelTargetSelection(); }
    SetMenuState(PreTargetState);
    // Recapture HUD focus — clicking the button will have stolen Slate focus
    // from the HUD, so we restore it immediately so keyboard shortcuts resume.
    if (OwnerHUD.IsValid())
    {
        if (APlayerController* PC = GetOwningPlayer()) { OwnerHUD->SetUserFocus(PC); }
    }
}

void UCombatActionPanelWidget::OnHealingProtocolClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }

    SetMenuState(ECombatMenuState::SelectingTarget);
    BM->BeginProtocolTargetSelection(EProtocolType::Healing);
}

void UCombatActionPanelWidget::OnRevivalProtocolClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }

    SetMenuState(ECombatMenuState::SelectingTarget);
    BM->BeginProtocolTargetSelection(EProtocolType::Revival);
}

void UCombatActionPanelWidget::OnAPProtocolClicked()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM || BM->GetCurrentPhase() != EBattlePhase::AwaitingInput) { return; }

    SetMenuState(ECombatMenuState::SelectingTarget);
    BM->BeginProtocolTargetSelection(EProtocolType::AP);
}

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

int32 UCombatActionPanelWidget::FindMeleeAbilityIndex() const
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return -1; }
    ACombatantBase* Actor = BM->GetActiveCombatant();
    if (!Actor || !Actor->AbilityManager) { return -1; }

    for (int32 i = 0; i < Actor->AbilityManager->GetAbilityCount(); ++i)
    {
        const UCombatAbility* A = Actor->AbilityManager->GetAbility(i);
        if (A && A->AbilityCategory == EAbilityCategory::Melee) { return i; }
    }
    return -1;
}

int32 UCombatActionPanelWidget::FindGunAbilityIndex() const
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return -1; }
    ACombatantBase* Actor = BM->GetActiveCombatant();
    if (!Actor || !Actor->AbilityManager) { return -1; }

    for (int32 i = 0; i < Actor->AbilityManager->GetAbilityCount(); ++i)
    {
        const UCombatAbility* A = Actor->AbilityManager->GetAbility(i);
        if (A && A->AbilityCategory == EAbilityCategory::Gun) { return i; }
    }
    return -1;
}

bool UCombatActionPanelWidget::HasSkillAbilities() const
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return false; }
    ACombatantBase* Actor = BM->GetActiveCombatant();
    if (!Actor || !Actor->AbilityManager) { return false; }

    for (int32 i = 0; i < Actor->AbilityManager->GetAbilityCount(); ++i)
    {
        const UCombatAbility* A = Actor->AbilityManager->GetAbility(i);
        if (A && A->AbilityCategory == EAbilityCategory::Skill &&
            Actor->AbilityManager->CanActivateAbility(i))
        {
            return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
//  Minigame
// -----------------------------------------------------------------------------

void UCombatActionPanelWidget::OnBMSkillMinigameShouldStart(int32 AbilityIndex)
{
    // This fires AFTER target selection is confirmed, from BattleManager.
    // The player has already chosen their target; we now show the minigame overlay.
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return; }

    ACombatantBase* Actor = BM->GetActiveCombatant();
    if (!Actor) { return; }

    const UCombatAbility* Ability = Actor->AbilityManager->GetAbility(AbilityIndex);
    if (!Ability || !Ability->MinigameClass)
    {
        // Shouldn't reach here, but if MinigameClass is somehow null, execute immediately.
        BM->ExecutePendingSkillAfterMinigame(1.0f);
        return;
    }

    UAbilityMinigameWidget* Minigame = CreateWidget<UAbilityMinigameWidget>(
        GetOwningPlayer(), Ability->MinigameClass);

    if (!Minigame)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ActionPanel] Failed to create minigame widget for '%s'. Executing at 1.0x."),
            *Ability->DisplayName.ToString());
        BM->ExecutePendingSkillAfterMinigame(1.0f);
        return;
    }

    // Propagate support flag so the minigame suppresses the blue strip if needed.
    Minigame->bIsSupportAbility = Ability->bIsSupportAbility;

    // Subscribe before showing — widget may fire synchronously in edge cases.
    Minigame->OnMinigameCompleted.AddDynamic(this,
        &UCombatActionPanelWidget::OnMinigameCompleted);

    ActiveMinigameWidget = Minigame;

    // Fixed 320×320 widget centred on screen via AddToPlayerScreen.
    // This keeps the combat HUD fully visible behind the diamond.
    // ZOrder 10 keeps it above the HUD (ZOrder 0) but is not fullscreen.
    {
        const FVector2D WidgetSize(320.f, 320.f);
        // GetViewportSize not available at this point without a PC; use
        // SetDesiredSizeInViewport + AddToPlayerScreen for proper centering.
        Minigame->SetDesiredSizeInViewport(WidgetSize);
        Minigame->AddToPlayerScreen(10);
        // Position to centre: the widget anchors to top-left by default.
        // FAnchors centre + zero offsets centres it when desired size matches.
        // We achieve centring by setting the viewport position after adding.
        if (APlayerController* PC = GetOwningPlayer())
        {
            int32 VX = 0, VY = 0;
            PC->GetViewportSize(VX, VY);
            const FVector2D Centre(
                (VX * 0.5f) - (WidgetSize.X * 0.5f),
                (VY * 0.5f) - (WidgetSize.Y * 0.5f));
            Minigame->SetPositionInViewport(Centre, false);
        }
    }

    // Give the minigame keyboard focus for NativeOnKeyDown to fire.
    if (APlayerController* PC = GetOwningPlayer())
    {
        Minigame->SetUserFocus(PC);
    }

    // Dim and block the action panel while the overlay runs.
    SetMenuState(ECombatMenuState::MinigameActive);

    // Begin the animation.
    Minigame->StartMinigame();
}

void UCombatActionPanelWidget::OnMinigameCompleted(float OutcomeMultiplier)
{
    ActiveMinigameWidget = nullptr;

    ABattleManager* BM = BattleManager.Get();

    // Restore keyboard focus to the HUD so subsequent input works.
    if (OwnerHUD.IsValid())
    {
        if (APlayerController* PC = GetOwningPlayer())
        {
            OwnerHUD->SetUserFocus(PC);
        }
    }

    // OutcomeMultiplier == 0.0 is the cancel sentinel from CancelMinigame().
    // Abort skill execution and return the player to the skill menu.
    if (FMath::IsNearlyZero(OutcomeMultiplier))
    {
        if (BM) { BM->CancelPendingMinigameSkill(); }
        SetMenuState(ECombatMenuState::SkillMenu);
        return;
    }

    if (!BM)
    {
        SetMenuState(ECombatMenuState::MainMenu);
        return;
    }

    SetMenuState(ECombatMenuState::Inactive);
    BM->ExecutePendingSkillAfterMinigame(OutcomeMultiplier);
}
