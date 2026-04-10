#include "StatusEffects/DespairEffect.h"
#include "Characters/Base/CombatantBase.h"

void UDespairEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Despair] Applied to %s. AP gain is blocked."), Owner ? *Owner->GetName() : TEXT("?"));
}
