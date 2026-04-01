#include "Abilities/Protocols/Ability_HealingProtocol.h"
#include "Abilities/Protocols/Ability_RevivalProtocol.h"
#include "Abilities/Protocols/Ability_APProtocol.h"
#include "Characters/Base/CombatantBase.h"

// ─── Healing Protocol ────────────────────────────────────────────────────────

void UAbility_HealingProtocol::Execute_Implementation(ACombatantBase* Instigator,
                                                       const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        const float HealAmount = Target->GetMaxHP() * 0.40f;
        Target->ApplyHealing(HealAmount, Instigator);

        UE_LOG(LogTemp, Log, TEXT("[HealingProtocol] Healed %s for %.1f HP (40%% of max)."),
            *Target->GetName(), HealAmount);
    }
}

// ─── Revival Protocol ────────────────────────────────────────────────────────

void UAbility_RevivalProtocol::Execute_Implementation(ACombatantBase* Instigator,
                                                       const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target) { continue; }
        // Revive() is a no-op if the target is already alive — safe to call.
        Target->Revive(0.35f);

        UE_LOG(LogTemp, Log, TEXT("[RevivalProtocol] Revived %s at 35%% HP."),
            *Target->GetName());
    }
}

// ─── AP Protocol ─────────────────────────────────────────────────────────────

void UAbility_APProtocol::Execute_Implementation(ACombatantBase* Instigator,
                                                   const TArray<ACombatantBase*>& Targets)
{
    for (ACombatantBase* Target : Targets)
    {
        if (!Target || Target->IsDead()) { continue; }

        Target->RestoreResource(EResourceType::AP, 5.f);

        UE_LOG(LogTemp, Log, TEXT("[APProtocol] Granted +5 AP to %s."),
            *Target->GetName());
    }
}
