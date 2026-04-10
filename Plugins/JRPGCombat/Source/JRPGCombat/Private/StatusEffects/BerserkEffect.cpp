#include "StatusEffects/BerserkEffect.h"
#include "Characters/Base/CombatantBase.h"

void UBerserkEffect::OnApply_Implementation()
{
    bExtraTurnReady = true;
    UE_LOG(LogTemp, Log, TEXT("[Berserk] Applied to %s."), Owner ? *Owner->GetName() : TEXT("?"));
}

void UBerserkEffect::OnTurnStart_Implementation()
{
    // Arm the extra-turn flag at the beginning of each turn.
    bExtraTurnReady = true;
}

void UBerserkEffect::OnTurnEnd_Implementation()
{
    // Reset after the turn ends (BattleManager calls ConsumeExtraTurn before this,
    // so if it was consumed it's already false; if not consumed just reset it).
    bExtraTurnReady = false;
}

void UBerserkEffect::OnBeforeDealDamage_Implementation(FDamagePayload& Payload)
{
    Payload.BaseDamage *= (1.f + DamageBonus);
}
