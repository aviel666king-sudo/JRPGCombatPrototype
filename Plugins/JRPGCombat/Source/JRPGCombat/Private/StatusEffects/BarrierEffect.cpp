#include "StatusEffects/BarrierEffect.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/StatusEffectManagerComponent.h"

void UBarrierEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Barrier] Applied to %s. Stacks: %d"), Owner ? *Owner->GetName() : TEXT("?"), StackCount);
}

void UBarrierEffect::OnBeforeTakeDamage_Implementation(FDamagePayload& Payload)
{
    if (Payload.BaseDamage <= 0.f) return;

    // Absorb the hit entirely. Consume one stack.
    Payload.BaseDamage = 0.f;
    --StackCount;

    UE_LOG(LogTemp, Log, TEXT("[Barrier] Hit absorbed on %s. Stacks remaining: %d"), Owner ? *Owner->GetName() : TEXT("?"), StackCount);

    if (StackCount <= 0)
    {
        Duration = 0; // Expire.
    }
}
