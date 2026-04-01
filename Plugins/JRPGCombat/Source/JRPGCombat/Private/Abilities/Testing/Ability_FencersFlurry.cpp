#include "Abilities/Testing/Ability_FencersFlurry.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/FragileEffect.h"

void UAbility_FencersFlurry::Execute_Implementation(ACombatantBase* Instigator,
                                                      const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack() * 1.0f,
            EDamageType::Physical);

        Target->ApplyDamage(Payload);

        // Apply Fragile: target takes +15% damage for 3 turns.
        ApplyEffectToTarget(Instigator, Target, UFragileEffect::StaticClass());
    }

    // Switch to Offensive after the hit.
    ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator);
    if (Fencer)
    {
        Fencer->SetStance(EFencerStance::Offensive);
    }
}
