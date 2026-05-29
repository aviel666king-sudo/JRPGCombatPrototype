#include "Abilities/Testing/Ability_OffensiveSwitch.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/FragileEffect.h"

void UAbility_OffensiveSwitch::Execute_Implementation(ACombatantBase* Instigator,
                                                       const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack() * 0.8f,
            EDamageType::Physical);

        Target->ApplyDamage(Payload);

        // Fragile: target takes +15% damage for 3 turns (our "Defenceless").
        ApplyEffectToTarget(Instigator, Target, UFragileEffect::StaticClass());
    }

    if (ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator))
    {
        Fencer->SetStance(EFencerStance::Offensive);
    }
}
