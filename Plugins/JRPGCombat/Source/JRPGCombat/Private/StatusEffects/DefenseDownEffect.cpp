#include "StatusEffects/DefenseDownEffect.h"

void UDefenseDownEffect::OnBeforeTakeDamage_Implementation(FDamagePayload& Payload)
{
    // +25% incoming BaseDamage. Stacks additively with Fragile (+15%) if both active.
    Payload.BaseDamage *= (1.f + Amplifier);
}
