#include "StatusEffects/ConfuseEffect.h"
#include "Characters/Base/CombatantBase.h"

void UConfuseEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Confuse] Applied to %s."), Owner ? *Owner->GetName() : TEXT("?"));
}
