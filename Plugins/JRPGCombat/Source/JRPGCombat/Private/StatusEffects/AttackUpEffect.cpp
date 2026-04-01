#include "StatusEffects/AttackUpEffect.h"

void UAttackUpEffect::OnBeforeDealDamage_Implementation(FDamagePayload& Payload)
{
    // +30% outgoing BaseDamage before defense math runs.
    // Both buffs and debuffs on Attack are allowed simultaneously — the pipeline
    // applies them independently and the results compound.
    Payload.BaseDamage *= (1.f + Multiplier);
}
