#include "Abilities/Testing/Ability_GuardDown.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/FragileEffect.h"

void UAbility_GuardDown::Execute_Implementation(ACombatantBase* Instigator,
                                                 const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }
        ApplyEffectToTarget(Instigator, Target, UFragileEffect::StaticClass());
    }

    if (ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator))
    {
        Fencer->SetStance(EFencerStance::Offensive);
    }
}
