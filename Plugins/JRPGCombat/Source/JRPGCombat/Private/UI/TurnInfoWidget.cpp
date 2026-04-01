#include "UI/TurnInfoWidget.h"
#include "Characters/Base/CombatantBase.h"
#include "CombatTypes.h"
#include "Components/TextBlock.h"

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
