#include "Abilities/Testing/Ability_ArmorBreach.h"
#include "Characters/Base/CombatantBase.h"
#include "StatusEffects/DefenseDownEffect.h"

void UAbility_ArmorBreach::Execute_Implementation(ACombatantBase* Instigator,
                                                    const TArray<ACombatantBase*>& Targets)
{
    constexpr int32 Duration = 3;
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }
        ApplyEffectToTargetWithDuration(Instigator, Target, UDefenseDownEffect::StaticClass(), Duration);
    }
}
