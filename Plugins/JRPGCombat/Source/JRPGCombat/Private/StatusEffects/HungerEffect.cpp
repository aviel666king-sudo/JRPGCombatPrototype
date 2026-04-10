#include "StatusEffects/HungerEffect.h"
#include "Characters/Base/CombatantBase.h"

void UHungerEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Hunger] Applied to %s. Healing is blocked."), Owner ? *Owner->GetName() : TEXT("?"));
}
