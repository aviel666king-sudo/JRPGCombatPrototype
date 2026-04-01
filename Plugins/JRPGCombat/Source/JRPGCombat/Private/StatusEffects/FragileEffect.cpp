#include "StatusEffects/FragileEffect.h"

void UFragileEffect::OnBeforeTakeDamage_Implementation(FDamagePayload& Payload)
{
    // Amplify incoming BaseDamage before defense math runs.
    // Global +15% incoming damage. Percée applies an additional bonus on top
    // when it detects Fragile, bringing its total effective advantage to ~+35%.
    Payload.BaseDamage *= 1.15f;
}
