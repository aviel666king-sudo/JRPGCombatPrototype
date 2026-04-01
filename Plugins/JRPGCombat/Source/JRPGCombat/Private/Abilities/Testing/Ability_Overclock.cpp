#include "Abilities/Testing/Ability_Overclock.h"
#include "Characters/Base/CombatantBase.h"
#include "StatusEffects/AttackUpEffect.h"

void UAbility_Overclock::Execute_Implementation(ACombatantBase* Instigator,
                                                  const TArray<ACombatantBase*>& Targets)
{
    // Normal duration = 3. TODO: Pass minigame result (Perfect=4, Fail=2) via
    // ApplyEffectToTargetWithDuration when the minigame system is implemented.
    constexpr int32 Duration = 3;

    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }
        ApplyEffectToTargetWithDuration(Instigator, Target, UAttackUpEffect::StaticClass(), Duration);
    }
}
