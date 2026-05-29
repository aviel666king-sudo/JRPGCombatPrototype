#include "Abilities/Testing/Ability_Combustion.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "Components/StatusEffectManagerComponent.h"
#include "StatusEffects/BurnEffect.h"

void UAbility_Combustion::Execute_Implementation(ACombatantBase* Instigator,
                                                  const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        // Count the target's Burn stacks, then consume them.
        int32 BurnStacks = 0;
        if (Target->StatusEffectManager)
        {
            for (UStatusEffect* Effect : Target->StatusEffectManager->GetActiveEffects())
            {
                if (const UBurnEffect* Burn = Cast<UBurnEffect>(Effect))
                {
                    BurnStacks += Burn->GetTicksRemaining();
                }
            }
        }

        // Base 0.8x Attack + 0.15x Attack per consumed Burn stack.
        const float BaseDamage = Instigator->GetBaseAttack() * (0.8f + 0.15f * BurnStacks);

        FDamagePayload Payload = MakeDamagePayload(Instigator, BaseDamage, EDamageType::Magical);
        Target->ApplyDamage(Payload);

        // Burn is spent by the blast.
        if (BurnStacks > 0 && Target->StatusEffectManager)
        {
            Target->StatusEffectManager->RemoveEffect(UBurnEffect::StaticClass());
        }
    }

    if (ACombatantFencer* Fencer = Cast<ACombatantFencer>(Instigator))
    {
        Fencer->SetStance(EFencerStance::Offensive);
    }
}
