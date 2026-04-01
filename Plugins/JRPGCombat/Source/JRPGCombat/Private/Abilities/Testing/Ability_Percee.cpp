#include "Abilities/Testing/Ability_Percee.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/FragileEffect.h"
#include "Components/StatusEffectManagerComponent.h"

void UAbility_Percee::Execute_Implementation(ACombatantBase* Instigator,
                                              const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        // Base damage: 1.0× Attack Power.
        float BaseDamage = Instigator->GetBaseAttack() * 1.0f;

        // Fragile interaction: if the target is Fragile, add a 20% Percée-specific
        // bonus to BaseDamage BEFORE the payload is built. Fragile's own global +15%
        // then applies on top via OnBeforeTakeDamage, so the combined effective
        // advantage against a Fragile target is approximately +35%.
        const bool bTargetFragile = Target->StatusEffectManager &&
                                    Target->StatusEffectManager->HasEffect(UFragileEffect::StaticClass());
        if (bTargetFragile)
        {
            BaseDamage *= 1.20f;
        }

        FDamagePayload Payload = MakeDamagePayload(Instigator, BaseDamage, EDamageType::Physical);
        Target->ApplyDamage(Payload);
    }

    // Switch to Defensive after the hit.
    ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator);
    if (Fencer)
    {
        Fencer->SetStance(EFencerStance::Defensive);
    }
}
