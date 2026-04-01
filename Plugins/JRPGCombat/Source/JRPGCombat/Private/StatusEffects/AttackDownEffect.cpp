#include "StatusEffects/AttackDownEffect.h"

void UAttackDownEffect::OnBeforeDealDamage_Implementation(FDamagePayload& Payload)
{
    // -30% outgoing BaseDamage. Clamped to 0.1x minimum to prevent zeroing.
    Payload.BaseDamage *= FMath::Max(0.1f, 1.f - Reduction);
}
