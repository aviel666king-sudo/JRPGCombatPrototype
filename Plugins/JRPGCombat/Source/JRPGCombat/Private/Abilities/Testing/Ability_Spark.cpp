#include "Abilities/Testing/Ability_Spark.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Testing/CombatantFencer.h"
#include "StatusEffects/BurnEffect.h"

void UAbility_Spark::Execute_Implementation(ACombatantBase* Instigator,
                                            const TArray<ACombatantBase*>& Targets)
{
    // Extra Burn if launched from Offensive stance (snapshot before the switch).
    ACombatantFencer* Fencer    = Cast<ACombatantFencer>(Instigator);
    const bool bOffensiveBonus  = Fencer &&
                                  Fencer->GetCurrentStance() == EFencerStance::Offensive;
    const int32 BurnStacks       = 3 + (bOffensiveBonus ? 2 : 0);

    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        FDamagePayload Payload = MakeDamagePayload(
            Instigator,
            Instigator->GetBaseAttack() * 0.7f,
            EDamageType::Magical);

        Target->ApplyDamage(Payload);

        for (int32 Stack = 0; Stack < BurnStacks; ++Stack)
        {
            ApplyEffectToTarget(Instigator, Target, UBurnEffect::StaticClass());
        }
    }

    if (Fencer)
    {
        Fencer->SetStance(EFencerStance::Defensive);
    }
}
