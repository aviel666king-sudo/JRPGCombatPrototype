#include "StatusEffects/StatusEffect.h"
#include "Characters/Base/CombatantBase.h"

void UStatusEffect::Init(ACombatantBase* InOwner, ACombatantBase* InSource)
{
    Owner      = InOwner;
    Source     = InSource;
    Duration   = BaseDuration;
    StackCount = 1;
}

void UStatusEffect::OnApply_Implementation()     {}
void UStatusEffect::OnTurnStart_Implementation() {}
void UStatusEffect::OnTurnEnd_Implementation()   {}
void UStatusEffect::OnExpire_Implementation()    {}
void UStatusEffect::OnReapply_Implementation()   {}

void UStatusEffect::OnBeforeDealDamage_Implementation(FDamagePayload& Payload)      {}
void UStatusEffect::OnBeforeTakeDamage_Implementation(FDamagePayload& Payload)      {}
void UStatusEffect::OnAfterTakeDamage_Implementation(const FDamagePayload& Payload) {}
void UStatusEffect::OnDealDamage_Implementation(const FDamagePayload& Payload)      {}

void UStatusEffect::OnBeforeDealHealing_Implementation(float& Amount)    {}
void UStatusEffect::OnAfterDealHealing_Implementation(float FinalAmount) {}
void UStatusEffect::OnBeforeReceiveHealing_Implementation(float& Amount)    {}
void UStatusEffect::OnAfterReceiveHealing_Implementation(float FinalAmount) {}
