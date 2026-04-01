#include "Abilities/Testing/Ability_GunShot.h"
#include "Characters/Base/CombatantBase.h"

void UAbility_GunShot::Execute_Implementation(ACombatantBase* Instigator,
                                              const TArray<ACombatantBase*>& Targets)
{
    if (!Instigator)
    {
        return;
    }

    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead())
        {
            continue;
        }

        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack(),
            EDamageType::Physical);

        Target->ApplyDamage(Payload);
    }
}
