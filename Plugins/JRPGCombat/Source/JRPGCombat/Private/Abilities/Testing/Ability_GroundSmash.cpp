#include "Abilities/Testing/Ability_GroundSmash.h"
#include "Characters/Base/CombatantBase.h"

void UAbility_GroundSmash::Execute_Implementation(ACombatantBase* Instigator,
                                                    const TArray<ACombatantBase*>& Targets)
{
    UE_LOG(LogTemp, Warning, TEXT("Enemy used Ground Smash!"));

    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        // 1.0x Attack as Physical damage through the standard pipeline.
        // ApplyDamage handles defense, status hooks, stance multipliers,
        // and fires BP_OnDamageTaken on the target — nothing is bypassed.
        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack(),
            EDamageType::Physical);

        Target->ApplyDamage(Payload);
    }
}
