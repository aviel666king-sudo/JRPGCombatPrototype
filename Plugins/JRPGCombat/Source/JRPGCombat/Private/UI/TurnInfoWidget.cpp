#include "UI/TurnInfoWidget.h"
#include "Characters/Base/CombatantBase.h"
#include "CombatTypes.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Styling/CoreStyle.h"

TSharedRef<SWidget> UTurnInfoWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        BuildLayout();
    }
    return Super::RebuildWidget();
}

void UTurnInfoWidget::BuildLayout()
{
    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    WidgetTree->RootWidget = Col;

    TurnNumberText = WidgetTree->ConstructWidget<UTextBlock>();
    TurnNumberText->SetFont(FCoreStyle::GetDefaultFontStyle("Bold", 26));
    TurnNumberText->SetColorAndOpacity(FLinearColor(0.97f, 0.97f, 1.f, 1.f));
    TurnNumberText->SetJustification(ETextJustify::Center);
    Col->AddChildToVerticalBox(TurnNumberText);

    NextActorText = WidgetTree->ConstructWidget<UTextBlock>();
    NextActorText->SetFont(FCoreStyle::GetDefaultFontStyle("Regular", 17));
    NextActorText->SetColorAndOpacity(FLinearColor(0.72f, 0.78f, 0.90f, 1.f));
    NextActorText->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(NextActorText))
    {
        S->SetPadding(FMargin(0.f, 2.f, 0.f, 0.f));
        S->SetHorizontalAlignment(HAlign_Center);
    }
}

void UTurnInfoWidget::Refresh(int32 InternalTurnNumber, ACombatantBase* NextCombatant)
{
    // ── Turn number (0-based internally → 1-based display) ───────────────────
    if (TurnNumberText)
    {
        TurnNumberText->SetText(
            FText::Format(
                FText::FromString(TEXT("Turn {0}")),
                FText::AsNumber(InternalTurnNumber + 1)));
    }

    // ── Next actor ────────────────────────────────────────────────────────────
    if (NextActorText)
    {
        if (NextCombatant)
        {
            const FString Team = (NextCombatant->GetTeam() == ECombatTeam::Player)
                ? TEXT("Player")
                : TEXT("Enemy");

            const FString Name = NextCombatant->DisplayName.IsEmpty()
                ? NextCombatant->GetName()
                : NextCombatant->DisplayName.ToString();

            NextActorText->SetText(
                FText::FromString(FString::Printf(TEXT("Next: %s %s"), *Team, *Name)));
        }
        else
        {
            NextActorText->SetText(FText::FromString(TEXT("Next: —")));
        }
    }
}
