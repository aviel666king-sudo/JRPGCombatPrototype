#include "Abilities/Testing/Ability_FortifyProtocol.h"
#include "Characters/Base/CombatantBase.h"
#include "StatusEffects/DefenseUpEffect.h"

void UAbility_FortifyProtocol::Execute_Implementation(ACombatantBase* Instigator,
                                                        const TArray<ACombatantBase*>& Targets)
{
    constexpr int32 Duration = 3;
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }
        ApplyEffectToTargetWithDuration(Instigator, Target, UDefenseUpEffect::StaticClass(), Duration);
    }
}
