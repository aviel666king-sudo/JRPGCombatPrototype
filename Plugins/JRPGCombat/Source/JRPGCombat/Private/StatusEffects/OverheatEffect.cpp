#include "StatusEffects/OverheatEffect.h"
#include "Characters/Base/CombatantBase.h"

void UOverheatEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[Overheat] Applied to %s. Abilities blocked for %d turns."), Owner ? *Owner->GetName() : TEXT("?"), BaseDuration);
}
