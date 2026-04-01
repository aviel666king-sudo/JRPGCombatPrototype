#include "Abilities/Testing/Ability_SystemDisrupt.h"
#include "Characters/Base/CombatantBase.h"
#include "StatusEffects/AttackDownEffect.h"

void UAbility_SystemDisrupt::Execute_Implementation(ACombatantBase* Instigator,
                                                      const TArray<ACombatantBase*>& Targets)
{
    constexpr int32 Duration = 3;
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }
        ApplyEffectToTargetWithDuration(Instigator, Target, UAttackDownEffect::StaticClass(), Duration);
    }
}
