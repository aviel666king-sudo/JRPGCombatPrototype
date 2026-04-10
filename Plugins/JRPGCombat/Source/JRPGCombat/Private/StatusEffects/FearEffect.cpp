#include "StatusEffects/FearEffect.h"
#include "Characters/Base/CombatantBase.h"

void UFearEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Fear] Applied to %s."), Owner ? *Owner->GetName() : TEXT("?"));
}

bool UFearEffect::BlocksTurnAction() const
{
    // 40% chance to cower and skip the turn.
    const bool bIgnore = FMath::FRand() < CommandIgnoreChance;
    if (bIgnore) { UE_LOG(LogTemp, Log, TEXT("[Fear] %s ignores commands this turn."), Owner ? *Owner->GetName() : TEXT("?")); }
    return bIgnore;
}
