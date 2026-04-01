#include "Abilities/Testing/Ability_AccelerationBoost.h"
#include "Characters/Base/CombatantBase.h"
#include "StatusEffects/AgilityUpEffect.h"

void UAbility_AccelerationBoost::Execute_Implementation(ACombatantBase* Instigator,
                                                          const TArray<ACombatantBase*>& Targets)
{
    constexpr int32 Duration = 3;
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }
        ApplyEffectToTargetWithDuration(Instigator, Target, UAgilityUpEffect::StaticClass(), Duration);
    }
}
