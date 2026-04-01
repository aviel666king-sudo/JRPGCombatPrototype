#include "StatusEffects/BurnEffect.h"
#include "Characters/Base/CombatantBase.h"

void UBurnEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Burn] Applied to %s. Stacks: %d"),
        Owner ? *Owner->GetName() : TEXT("?"), StackCount);
}

void UBurnEffect::OnTurnStart_Implementation()
{
    if (!Owner || Owner->IsDead()) { return; }

    // 3% MaxHP as Magical damage, routed through the full pipeline.
    // TODO: Future - allow Chips/Stats to modify DamagePercent via a damage context.
    const float DamageAmount = Owner->GetMaxHP() * DamagePercent;

    FDamagePayload Payload;
    Payload.Source     = Source.Get();
    Payload.BaseDamage = DamageAmount;
    Payload.DamageType = EDamageType::Magical;

    Owner->ApplyDamage(Payload);

    // Consume 1 stack per tick.
    --StackCount;

    UE_LOG(LogTemp, Log, TEXT("[Burn] Ticked on %s. Dealt %.1f damage. Stacks remaining: %d"),
        *Owner->GetName(), Payload.ResolvedDamage, StackCount);

    if (StackCount <= 0)
    {
        Duration = 0;  // Signal PurgeExpired at end of NotifyTurnEnd.
    }
}

void UBurnEffect::OnExpire_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Burn] Expired on %s."),
        Owner ? *Owner->GetName() : TEXT("?"));
}
