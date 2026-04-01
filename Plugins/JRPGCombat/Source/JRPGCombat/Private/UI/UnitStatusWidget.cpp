#include "UI/UnitStatusWidget.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/StatusEffectManagerComponent.h"
#include "StatusEffects/StatusEffect.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"

void UUnitStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsActivePlayer(false);
    SetIsTargeted(false);
    SetIsEnemyActing(false);
    SetIsFaded(false);
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
    CardOverlay->SetContentColorAndOpacity(bApply ? ActivePlayerTint : FLinearColor::White);
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
    return FText::GetEmpty();
}
