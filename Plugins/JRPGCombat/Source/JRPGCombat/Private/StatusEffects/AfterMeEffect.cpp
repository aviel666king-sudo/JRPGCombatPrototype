#include "StatusEffects/AfterMeEffect.h"
#include "Characters/Base/CombatantBase.h"

void UAfterMeEffect::OnApply_Implementation()
{
    UE_LOG(LogTemp, Log, TEXT("[AfterMe] Applied to %s. Target weight x%.1f."), Owner ? *Owner->GetName() : TEXT("?"), TargetingMultiplier);
}
