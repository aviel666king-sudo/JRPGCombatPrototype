#include "StatusEffects/DefenseUpEffect.h"

void UDefenseUpEffect::OnBeforeTakeDamage_Implementation(FDamagePayload& Payload)
{
    // -20% incoming BaseDamage before defense subtraction.
    Payload.BaseDamage *= (1.f - Reduction);
}
