#include "StatusEffects/SleepEffect.h"
#include "Characters/Base/CombatantBase.h"

void USleepEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Sleep] Applied to %s for %d turns."), Owner ? *Owner->GetName() : TEXT("?"), BaseDuration);
}

void USleepEffect::OnTurnStart_Implementation()
{
    if (!Owner || Owner->IsDead()) return;
    // Recover HP and AP while sleeping.
    Owner->ApplyHealing(Owner->GetMaxHP() * HealPercent, Source.Get());
    Owner->RestoreResource(EResourceType::AP, APRecover);
    UE_LOG(LogTemp, Log, TEXT("[Sleep] %s regenerates HP/AP while sleeping."), *Owner->GetName());
}

void USleepEffect::OnAfterTakeDamage_Implementation(const FDamagePayload& Payload)
{
    // Any hit wakes the unit.
    Duration = 0;
    UE_LOG(LogTemp, Log, TEXT("[Sleep] %s woken by damage."), Owner ? *Owner->GetName() : TEXT("?"));
}
