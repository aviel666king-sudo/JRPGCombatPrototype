#include "Abilities/Testing/Ability_FencerBasicStrike.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"

void UAbility_FencerBasicStrike::Execute_Implementation(ACombatantBase* Instigator,
                                                         const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        // Damage applies with the CURRENT stance multiplier (e.g. Virtuose ×3.0).
        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack() * 1.0f,
            EDamageType::Physical);

        Target->ApplyDamage(Payload);
    }

    // Stance transition fires AFTER damage — uses pre-transition multiplier.
    ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator);
    if (!Fencer) { return; }

    switch (Fencer->GetCurrentStance())
    {
        case EFencerStance::Defensive:
            Fencer->SetStance(EFencerStance::Offensive);
            break;
        case EFencerStance::Offensive:
            Fencer->SetStance(EFencerStance::Defensive);
            break;
        case EFencerStance::Virtuose:
            Fencer->SetStance(EFencerStance::Stanceless);
            break;
        case EFencerStance::Stanceless:
            // No change — Stanceless remains Stanceless.
            break;
    }
}
