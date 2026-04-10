#include "StatusEffects/RegenEffect.h"
#include "Characters/Base/CombatantBase.h"

void URegenEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Regen] Applied to %s."), Owner ? *Owner->GetName() : TEXT("?"));
}

void URegenEffect::OnTurnStart_Implementation()
{
    if (!Owner || Owner->IsDead()) return;
    const float Amount = Owner->GetMaxHP() * HealPercent;
    Owner->ApplyHealing(Amount, Source.Get());
    UE_LOG(LogTemp, Log, TEXT("[Regen] %s healed for %.1f."), *Owner->GetName(), Amount);
}
