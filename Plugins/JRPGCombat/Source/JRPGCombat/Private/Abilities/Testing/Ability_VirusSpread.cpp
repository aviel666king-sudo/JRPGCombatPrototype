#include "Abilities/Testing/Ability_VirusSpread.h"
#include "Characters/Base/CombatantBase.h"
#include "StatusEffects/VirusSpreadEffect.h"
#include "Components/StatusEffectManagerComponent.h"

void UAbility_VirusSpread::Execute_Implementation(ACombatantBase* Instigator,
                                                    const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        // Apply StacksToApply stacks. Each ApplyEffect call adds 1 stack via
        // StackAndRefresh behavior in VirusSpreadEffect.
        for (int32 i = 0; i < StacksToApply; ++i)
        {
            Target->StatusEffectManager->ApplyEffect(
                UVirusSpreadEffect::StaticClass(), Instigator);
        }
    }
}
