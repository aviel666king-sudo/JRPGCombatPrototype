#include "Abilities/Testing/Ability_SwiftStride.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/BurnEffect.h"
#include "Components/StatusEffectManagerComponent.h"

void UAbility_SwiftStride::Execute_Implementation(ACombatantBase* Instigator,
                                                    const TArray<ACombatantBase*>& Targets)
{
    ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator);

    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack() * 0.75f,
            EDamageType::Physical);

        Target->ApplyDamage(Payload);

        // Check for Burn on the target AFTER the hit resolves.
        const bool bTargetBurning = Target->StatusEffectManager &&
                                    Target->StatusEffectManager->HasEffect(UBurnEffect::StaticClass());

        if (bTargetBurning && Fencer)
        {
            // SetStance to Virtuose grants +1 AP via the stance system.
            Fencer->SetStance(EFencerStance::Virtuose);

            // +2 flat AP on top. Both are capped to MaxAP by RestoreResource.
            Instigator->RestoreResource(EResourceType::AP, 2.f);
        }
    }
}
