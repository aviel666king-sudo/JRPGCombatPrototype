#include "UI/UnitStatusWidget.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/StatusEffectManagerComponent.h"
#include "StatusEffects/StatusEffect.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/PanelWidget.h"
#include "Components/ContentWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UUnitStatusWidget::RebuildWidget()
{
    // Build the card into a fresh root when there's no WBP-provided layout
    // (the HUD constructs these directly from the C++ class).
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildCardLayout();
    }
    return Super::RebuildWidget();
}

void UUnitStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsActivePlayer(false);
    SetIsTargeted(false);
    SetIsEnemyActing(false);
    SetIsFaded(false);
}

void UUnitStatusWidget::BuildCardLayout()
{
    if (!WidgetTree) { return; }

    auto MakeFont = [](int32 Size, bool bBold)
    {
        return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
    };

    // OuterBorder = highlight ring (transparent until targeted/acting).
    OuterBorder = WidgetTree->ConstructWidget<UBorder>();
    OuterBorder->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
    OuterBorder->SetPadding(FMargin(2.f));

    // CardOverlay = dark card background; also the tint target for active player.
    CardOverlay = WidgetTree->ConstructWidget<UBorder>();
    CardOverlay->SetBrushColor(FLinearColor(0.06f, 0.07f, 0.10f, 0.94f));
    CardOverlay->SetPadding(FMargin(10.f, 7.f));
    OuterBorder->SetContent(CardOverlay);

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    CardOverlay->SetContent(Col);

    // Name line.
    NameText = WidgetTree->ConstructWidget<UTextBlock>();
    NameText->SetFont(MakeFont(14, true));
    NameText->SetColorAndOpacity(FLinearColor(0.97f, 0.97f, 1.f, 1.f));
    Col->AddChildToVerticalBox(NameText);

    // HP bar (green) with centered "HP x / y" overlay.
    {
        UOverlay* O = WidgetTree->ConstructWidget<UOverlay>();

        HPBar = WidgetTree->ConstructWidget<UProgressBar>();
        HPBar->SetFillColorAndOpacity(FLinearColor(0.30f, 0.85f, 0.40f, 1.f));
        if (UOverlaySlot* S = Cast<UOverlaySlot>(O->AddChildToOverlay(HPBar)))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetVerticalAlignment(VAlign_Fill);
        }

        HPText = WidgetTree->ConstructWidget<UTextBlock>();
        HPText->SetFont(MakeFont(10, true));
        HPText->SetColorAndOpacity(FLinearColor::White);
        if (UOverlaySlot* S = Cast<UOverlaySlot>(O->AddChildToOverlay(HPText)))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Center);
        }

        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
        Box->SetHeightOverride(18.f);
        Box->AddChild(O);
        UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Box);
        VS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    }

    // AP bar (blue) with centered "AP x / y" overlay.
    {
        UOverlay* O = WidgetTree->ConstructWidget<UOverlay>();

        APBar = WidgetTree->ConstructWidget<UProgressBar>();
        APBar->SetFillColorAndOpacity(FLinearColor(0.35f, 0.62f, 1.f, 1.f));
        if (UOverlaySlot* S = Cast<UOverlaySlot>(O->AddChildToOverlay(APBar)))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetVerticalAlignment(VAlign_Fill);
        }

        APText = WidgetTree->ConstructWidget<UTextBlock>();
        APText->SetFont(MakeFont(9, true));
        APText->SetColorAndOpacity(FLinearColor::White);
        if (UOverlaySlot* S = Cast<UOverlaySlot>(O->AddChildToOverlay(APText)))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Center);
        }

        USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>();
        Box->SetHeightOverride(13.f);
        Box->AddChild(O);
        UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(Box);
        VS->SetPadding(FMargin(0.f, 3.f, 0.f, 0.f));
    }

    // Passive line (e.g. Fencer stance).
    PassiveText = WidgetTree->ConstructWidget<UTextBlock>();
    PassiveText->SetFont(MakeFont(10, false));
    PassiveText->SetColorAndOpacity(FLinearColor(0.70f, 0.80f, 1.f, 1.f));
    {
        UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(PassiveText);
        VS->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
    }

    // Status effects line.
    StatusEffectsText = WidgetTree->ConstructWidget<UTextBlock>();
    StatusEffectsText->SetFont(MakeFont(10, false));
    StatusEffectsText->SetColorAndOpacity(FLinearColor(1.f, 0.80f, 0.35f, 1.f));
    StatusEffectsText->SetAutoWrapText(true);
    {
        UVerticalBoxSlot* VS = Col->AddChildToVerticalBox(StatusEffectsText);
        VS->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
    }

    // Fixed card width so all cards line up.
    USizeBox* CardBox = WidgetTree->ConstructWidget<USizeBox>();
    CardBox->SetWidthOverride(232.f);
    CardBox->AddChild(OuterBorder);

    // Attach to the widget's root (works whether the WBP root is a panel or a
    // content widget; the prior contents are replaced).
    UWidget* Root = GetRootWidget();
    if (UPanelWidget* P = Cast<UPanelWidget>(Root))
    {
        P->ClearChildren();
        P->AddChild(CardBox);
    }
    else if (UContentWidget* CW = Cast<UContentWidget>(Root))
    {
        CW->SetContent(CardBox);
    }
    else
    {
        WidgetTree->RootWidget = CardBox;
    }
}

void UUnitStatusWidget::SetUnit(ACombatantBase* InUnit)
{
    Unit = InUnit;
    Refresh();
}

void UUnitStatusWidget::Refresh()
{
    ACombatantBase* C = Unit.Get();

    if (!C)
    {
        if (NameText)          { NameText->SetText(FText::GetEmpty()); }
        if (HPBar)             { HPBar->SetPercent(0.f); }
        if (HPText)            { HPText->SetText(FText::GetEmpty()); }
        if (APBar)             { APBar->SetPercent(0.f); }
        if (APText)            { APText->SetText(FText::GetEmpty()); }
        if (PassiveText)       { PassiveText->SetText(FText::GetEmpty()); }
        if (StatusEffectsText) { StatusEffectsText->SetText(FText::GetEmpty()); }
        SetIsFaded(true);
        return;
    }

    if (NameText)
    {
        NameText->SetText(C->DisplayName.IsEmpty()
            ? FText::FromString(C->GetName()) : C->DisplayName);
    }

    if (HPBar || HPText)
    {
        const float MaxHP = C->GetMaxHP();
        const float Pct   = MaxHP > 0.f ? FMath::Clamp(C->GetCurrentHP() / MaxHP, 0.f, 1.f) : 0.f;
        if (HPBar)  { HPBar->SetPercent(Pct); }
        if (HPText) { HPText->SetText(BuildHPText()); }
    }

    if (APBar || APText)
    {
        const float MaxAP = C->GetMaxAP();
        const float Pct   = MaxAP > 0.f ? FMath::Clamp(C->GetCurrentAP() / MaxAP, 0.f, 1.f) : 0.f;
        if (APBar)  { APBar->SetPercent(Pct); }
        if (APText) { APText->SetText(BuildAPText()); }
    }

    if (PassiveText)       { PassiveText->SetText(GetPassiveText()); }
    if (StatusEffectsText) { StatusEffectsText->SetText(BuildStatusEffectsText()); }

    SetIsFaded(C->IsDead());
}

// -----------------------------------------------------------------------------
//  Visual state setters
// -----------------------------------------------------------------------------

void UUnitStatusWidget::SetIsActivePlayer(bool bActive)
{
    if (!CardOverlay) { return; }
    const bool bApply = bActive && !IsUnavailable();
    // Highlight via a subtly lighter card background instead of tinting the
    // content — keeps name/HP/AP at their regular colors.
    CardOverlay->SetBrushColor(bApply
        ? FLinearColor(0.13f, 0.17f, 0.26f, 0.96f)    // active: lighter slate
        : FLinearColor(0.06f, 0.07f, 0.10f, 0.94f));  // normal dark
}

void UUnitStatusWidget::SetIsTargeted(bool bTargeted)
{
    bIsTargeted = bTargeted;
    RefreshBorderColor();
}

void UUnitStatusWidget::SetIsEnemyActing(bool bActing)
{
    bIsEnemyActing = bActing;
    RefreshBorderColor();
}

void UUnitStatusWidget::SetIsFaded(bool bFaded)
{
    SetRenderOpacity(bFaded ? DeadOpacity : 1.0f);
}

bool UUnitStatusWidget::IsUnavailable() const
{
    const ACombatantBase* C = Unit.Get();
    return !C || C->IsDead();
}

// -----------------------------------------------------------------------------
//  Border priority: orange (acting) > green (targeted) > transparent
// -----------------------------------------------------------------------------

void UUnitStatusWidget::RefreshBorderColor()
{
    if (!OuterBorder) { return; }

    FLinearColor Color = FLinearColor(0.f, 0.f, 0.f, 0.f);  // transparent

    if (bIsEnemyActing)
    {
        Color = EnemyActingBorderColor;  // orange — highest priority
    }
    else if (bIsTargeted && !IsUnavailable())
    {
        Color = TargetBorderColor;       // green
    }

    OuterBorder->SetBrushColor(Color);
}

// -----------------------------------------------------------------------------
//  Label builders
// -----------------------------------------------------------------------------

FText UUnitStatusWidget::BuildHPText() const
{
    const ACombatantBase* C = Unit.Get();
    if (!C) { return FText::GetEmpty(); }
    return FText::FromString(FString::Printf(TEXT("HP %d / %d"),
        FMath::FloorToInt(C->GetCurrentHP()), FMath::FloorToInt(C->GetMaxHP())));
}

FText UUnitStatusWidget::BuildAPText() const
{
    const ACombatantBase* C = Unit.Get();
    if (!C) { return FText::GetEmpty(); }
    return FText::FromString(FString::Printf(TEXT("AP %d / %d"),
        FMath::FloorToInt(C->GetCurrentAP()), FMath::FloorToInt(C->GetMaxAP())));
}

FText UUnitStatusWidget::BuildStatusEffectsText() const
{
    const ACombatantBase* C = Unit.Get();
    if (!C || !C->StatusEffectManager) { return FText::GetEmpty(); }

    TArray<FString> Parts;
    for (const UStatusEffect* Effect : C->StatusEffectManager->GetActiveEffects())
    {
        if (!Effect) { continue; }
        FString Entry = Effect->DisplayName.ToString();
        if (Effect->MaxStacks > 1 && Effect->StackCount > 1)
        {
            Entry += FString::Printf(TEXT(" x%d"), Effect->StackCount);
        }
        Parts.Add(Entry);
    }

    return Parts.IsEmpty() ? FText::GetEmpty() : FText::FromString(FString::Join(Parts, TEXT("  ")));
}

FText UUnitStatusWidget::GetPassiveText_Implementation() const
{
    const ACombatantBase* C = Unit.Get();
    return C ? C->GetCombatSubtitle() : FText::GetEmpty();
}
