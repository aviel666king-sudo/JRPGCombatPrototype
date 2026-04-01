#include "Abilities/Testing/Ability_RainOfFire.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/BurnEffect.h"

void UAbility_RainOfFire::Execute_Implementation(ACombatantBase* Instigator,
                                                   const TArray<ACombatantBase*>& Targets)
{
    // Snapshot the stance bonus BEFORE the stance switches to Offensive.
    // Both hits benefit from the Defensive bonus if the caster entered this
    // ability while in Defensive.
    ACombatantFencer* Fencer      = Cast<ACombatantFencer>(Instigator);
    const bool bDefensiveBonus    = Fencer &&
                                    Fencer->GetCurrentStance() == EFencerStance::Defensive;
    const int32 BurnStacksPerHit  = 3 + (bDefensiveBonus ? 2 : 0);

    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        // Two separate hits — each resolves through the full damage pipeline.
        for (int32 Hit = 0; Hit < 2; ++Hit)
        {
            FDamagePayload Payload = MakeDamagePayload(
                Instigator,
                Instigator->GetBaseAttack() * 0.6f,
                EDamageType::Magical);

            Target->ApplyDamage(Payload);

            // Apply Burn stacks for this hit. Each ApplyEffectToTarget call
            // adds one stack (StackAndRefresh behavior in UBurnEffect).
            for (int32 Stack = 0; Stack < BurnStacksPerHit; ++Stack)
            {
                ApplyEffectToTarget(Instigator, Target, UBurnEffect::StaticClass());
            }
        }
    }

    // Switch to Offensive after all hits and stacks are applied.
    if (Fencer)
    {
        Fencer->SetStance(EFencerStance::Offensive);
    }
}
