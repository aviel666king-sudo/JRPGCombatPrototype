#include "StatusEffects/FreezeEffect.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/StatusEffectManagerComponent.h"

void UFreezeEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Freeze] Applied to %s."), Owner ? *Owner->GetName() : TEXT("?"));
}

void UFreezeEffect::OnAfterTakeDamage_Implementation(const FDamagePayload& Payload)
{
    // Any hit breaks Freeze.
    Duration = 0;
    UE_LOG(LogTemp, Log, TEXT("[Freeze] Broken on %s by incoming damage."), Owner ? *Owner->GetName() : TEXT("?"));
}
