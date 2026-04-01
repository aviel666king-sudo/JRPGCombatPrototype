#include "Abilities/Testing/Ability_LastChance.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"

void UAbility_LastChance::Execute_Implementation(ACombatantBase* Instigator,
                                                  const TArray<ACombatantBase*>& Targets)
{
    if (!Instigator) { return; }

    // Set HP to 1: spend (current HP - 1). SpendResource clamps to 0 minimum,
    // so if HP is already <= 1 this is a safe no-op.
    const float CurrentHP = Instigator->GetCurrentHP();
    if (CurrentHP > 1.f)
    {
        Instigator->SpendResource(EResourceType::HP, CurrentHP - 1.f);
    }

    // Fully restore AP to MaxAP. RestoreResource clamps to Max so this is safe.
    Instigator->RestoreResource(EResourceType::AP, Instigator->GetMaxAP());

    // Switch to Virtuose. SetStance grants +1 AP on top — RestoreResource
    // still clamps to MaxAP so the total will never exceed the cap.
    ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator);
    if (Fencer)
    {
        Fencer->SetStance(EFencerStance::Virtuose);
    }
}
