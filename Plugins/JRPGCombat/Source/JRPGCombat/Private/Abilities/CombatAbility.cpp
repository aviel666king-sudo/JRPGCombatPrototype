#include "Abilities/CombatAbility.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/StatusEffectManagerComponent.h"
#include "StatusEffects/StatusEffect.h"

// -----------------------------------------------------------------------------
//  Execution
// -----------------------------------------------------------------------------

void UCombatAbility::Execute(ACombatantBase* Instigator, const TArray<ACombatantBase*>& Targets)
{
    // 1. Run C++ gameplay logic. C++ subclasses override Execute_Implementation.
    Execute_Implementation(Instigator, Targets);

    // 2. Notify Blueprint children. BP_OnExecute fires after C++ logic
    //    completes, so any state changes (damage, effects) are already committed
    //    when Blueprint receives the event. Safe to leave unimplemented in BP.
    BP_OnExecute(Instigator, Targets);
}

void UCombatAbility::Execute_Implementation(ACombatantBase* Instigator, const TArray<ACombatantBase*>& Targets)
{
    // Base does nothing. C++ subclasses (e.g. UAbility_BasicStrike) override this.
}

// -----------------------------------------------------------------------------
//  CanActivate
// -----------------------------------------------------------------------------

bool UCombatAbility::CanActivate_Implementation(const ACombatantBase* Instigator) const
{
    if (!Instigator || !IsReady())
    {
        return false;
    }

    for (const FAbilityCost& Cost : Costs)
    {
        if (!Instigator->CanAffordCost(Cost))
        {
            return false;
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
//  Cooldown
// -----------------------------------------------------------------------------

void UCombatAbility::TickCooldown()
{
    if (CurrentCooldown > 0)
    {
        --CurrentCooldown;
    }
}

void UCombatAbility::StartCooldown()
{
    CurrentCooldown = MaxCooldown;
}

// -----------------------------------------------------------------------------
//  Initialisation
// -----------------------------------------------------------------------------

void UCombatAbility::Initialize_Implementation(ACombatantBase* OwningCombatant)
{
    CurrentCooldown = 0;
}

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

FDamagePayload UCombatAbility::MakeDamagePayload(ACombatantBase* Instigator,
                                                   float BaseDamage,
                                                   EDamageType DamageType) const
{
    FDamagePayload Payload;
    Payload.Source     = Instigator;
    Payload.BaseDamage = BaseDamage * ActiveMultiplier;  // 1.0 when no minigame ran
    Payload.DamageType = DamageType;
    return Payload;
}

void UCombatAbility::ApplyEffectToTarget(ACombatantBase* Instigator,
                                          ACombatantBase* Target,
                                          TSubclassOf<UStatusEffect> EffectClass) const
{
    if (!Target || !EffectClass) { return; }
    Target->StatusEffectManager->ApplyEffect(EffectClass, Instigator);
}

void UCombatAbility::ApplyEffectToTargetWithDuration(ACombatantBase* Instigator,
                                                       ACombatantBase* Target,
                                                       TSubclassOf<UStatusEffect> EffectClass,
                                                       int32 Duration) const
{
    if (!Target || !EffectClass) { return; }
    Target->StatusEffectManager->ApplyEffectWithDuration(EffectClass, Instigator, Duration);
}
