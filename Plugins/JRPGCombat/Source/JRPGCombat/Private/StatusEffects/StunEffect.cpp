#include "StatusEffects/StunEffect.h"
#include "Characters/Base/CombatantBase.h"

void UStunEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Stun] Applied to %s. Stacks: %d"), Owner ? *Owner->GetName() : TEXT("?"), StackCount);
}

void UStunEffect::OnTurnStart_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Stun] %s turn skipped (stacks: %d)."), Owner ? *Owner->GetName() : TEXT("?"), StackCount);
}
