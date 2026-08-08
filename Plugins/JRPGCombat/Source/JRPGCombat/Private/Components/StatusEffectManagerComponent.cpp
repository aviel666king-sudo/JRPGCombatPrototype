#include "Components/StatusEffectManagerComponent.h"
#include "StatusEffects/StatusEffect.h"
#include "Characters/Base/CombatantBase.h"

UStatusEffectManagerComponent::UStatusEffectManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------------
//  Private helpers
// -----------------------------------------------------------------------------

TArray<TObjectPtr<UStatusEffect>> UStatusEffectManagerComponent::GetSortedCopy() const
{
    TArray<TObjectPtr<UStatusEffect>> Copy = ActiveEffects;

    // ActiveEffects is a UPROPERTY, so a garbage-collected effect leaves a null entry behind.
    // Drop those before sorting: as of UE 5.8 the TDereferenceWrapper specialization for
    // TObjectPtr (ObjectPtr.h) makes TArray::Sort call Predicate(*A, *B), so the engine
    // dereferences each element before the predicate ever sees it — a null can no longer be
    // guarded against from inside the predicate the way it was pre-5.8.
    Copy.RemoveAll([](const TObjectPtr<UStatusEffect>& E) { return !E; });

    Copy.Sort([](const UStatusEffect& A, const UStatusEffect& B)
    {
        return A.Priority > B.Priority;
    });
    return Copy;
}

UStatusEffect* UStatusEffectManagerComponent::FindEffect(TSubclassOf<UStatusEffect> EffectClass) const
{
    for (UStatusEffect* Effect : ActiveEffects)
    {
        if (Effect && Effect->GetClass() == EffectClass)
        {
            return Effect;
        }
    }
    return nullptr;
}

void UStatusEffectManagerComponent::PurgeExpired()
{
    TArray<UStatusEffect*> Expired;
    for (UStatusEffect* Effect : ActiveEffects)
    {
        if (Effect && Effect->IsExpired())
        {
            Expired.Add(Effect);
        }
    }

    for (UStatusEffect* Effect : Expired)
    {
        Effect->OnExpire();
        ActiveEffects.Remove(Effect);
    }
}

// -----------------------------------------------------------------------------
//  Apply / Remove
// -----------------------------------------------------------------------------

void UStatusEffectManagerComponent::ApplyEffect(TSubclassOf<UStatusEffect> EffectClass,
                                                  ACombatantBase* Source)
{
    if (!EffectClass) { return; }

    UStatusEffect* Existing = FindEffect(EffectClass);

    if (Existing)
    {
        switch (Existing->StackBehavior)
        {
            case EEffectStackBehavior::IgnoreIfAlreadyActive:
                return;

            case EEffectStackBehavior::RefreshDuration:
                Existing->Duration = Existing->BaseDuration;
                Existing->OnReapply();
                return;

            case EEffectStackBehavior::StackAndRefresh:
                Existing->StackCount = FMath::Min(Existing->StackCount + 1, Existing->MaxStacks);
                Existing->Duration   = Existing->BaseDuration;
                return;

            case EEffectStackBehavior::Replace:
                Existing->OnExpire();
                ActiveEffects.Remove(Existing);
                break;
        }
    }

    ACombatantBase* Owner = Cast<ACombatantBase>(GetOwner());
    if (!Owner) { return; }

    UStatusEffect* Effect = NewObject<UStatusEffect>(this, EffectClass);
    Effect->Init(Owner, Source);
    ActiveEffects.Add(Effect);

    Effect->OnApply();
}

void UStatusEffectManagerComponent::RemoveEffect(TSubclassOf<UStatusEffect> EffectClass)
{
    ActiveEffects.RemoveAll([&](const TObjectPtr<UStatusEffect>& E)
    {
        return E && E->GetClass() == EffectClass;
    });
}

void UStatusEffectManagerComponent::ClearAllEffects()
{
    ActiveEffects.Empty();
}

void UStatusEffectManagerComponent::ApplyEffectWithDuration(TSubclassOf<UStatusEffect> EffectClass,
                                                              ACombatantBase* Source,
                                                              int32 OverrideDuration)
{
    // Apply normally, then override duration on the resulting instance.
    // TODO: Minigame performance result (Perfect=4, Normal=3, Fail=2) should be
    //       passed as OverrideDuration from the ability that calls this.
    ApplyEffect(EffectClass, Source);

    // Find the effect that was just applied (or already existed) and set duration.
    UStatusEffect* Effect = FindEffect(EffectClass);
    if (Effect && OverrideDuration > 0)
    {
        Effect->Duration = OverrideDuration;
    }
}

// -----------------------------------------------------------------------------
//  Turn hooks
// -----------------------------------------------------------------------------

void UStatusEffectManagerComponent::NotifyTurnStart()
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnTurnStart(); }
    }
    // Duration is NOT decremented here. Only decremented once, on turn end.
}

void UStatusEffectManagerComponent::NotifyTurnEnd()
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnTurnEnd(); }
    }

    // Decrement duration once after all callbacks have fired.
    // Stack-driven effects (e.g. Burn, VirusSpread) manage their own expiry via StackCount;
    // they set Duration = 0 themselves when stacks run out — skip auto-decrement for them.
    for (UStatusEffect* Effect : ActiveEffects)
    {
        if (Effect && !Effect->bStackDrivenDuration) { --Effect->Duration; }
    }

    PurgeExpired();
}

// -----------------------------------------------------------------------------
//  Damage hooks
// -----------------------------------------------------------------------------

void UStatusEffectManagerComponent::NotifyBeforeDealDamage(FDamagePayload& Payload)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnBeforeDealDamage(Payload); }
    }
}

void UStatusEffectManagerComponent::NotifyBeforeTakeDamage(FDamagePayload& Payload)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnBeforeTakeDamage(Payload); }
    }
}

void UStatusEffectManagerComponent::NotifyAfterTakeDamage(const FDamagePayload& Payload)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnAfterTakeDamage(Payload); }
    }
}

void UStatusEffectManagerComponent::NotifyDealDamage(const FDamagePayload& Payload)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnDealDamage(Payload); }
    }
}

// -----------------------------------------------------------------------------
//  Healing hooks
// -----------------------------------------------------------------------------

void UStatusEffectManagerComponent::NotifyBeforeDealHealing(float& Amount)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnBeforeDealHealing(Amount); }
    }
}

void UStatusEffectManagerComponent::NotifyAfterDealHealing(float FinalAmount)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnAfterDealHealing(FinalAmount); }
    }
}

void UStatusEffectManagerComponent::NotifyBeforeReceiveHealing(float& Amount)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnBeforeReceiveHealing(Amount); }
    }
}

void UStatusEffectManagerComponent::NotifyAfterReceiveHealing(float FinalAmount)
{
    for (UStatusEffect* Effect : GetSortedCopy())
    {
        if (Effect) { Effect->OnAfterReceiveHealing(FinalAmount); }
    }
}

// -----------------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------------

bool UStatusEffectManagerComponent::HasEffect(TSubclassOf<UStatusEffect> EffectClass) const
{
    return FindEffect(EffectClass) != nullptr;
}

float UStatusEffectManagerComponent::GetSpeedMultiplier() const
{
    // Aggregate all speed modifiers additively into a single multiplier.
    // Each effect returns a delta (e.g. +0.35 for AgilityUp, -0.35 for AgilityDown).
    // Combined result is clamped to a minimum of 0.1 to prevent zero/negative speed.
    float TotalModifier = 0.f;
    for (const UStatusEffect* Effect : ActiveEffects)
    {
        if (Effect) { TotalModifier += Effect->GetSpeedModifier(); }
    }
    return FMath::Max(0.1f, 1.f + TotalModifier);
}

bool UStatusEffectManagerComponent::ShouldSkipTurn() const
{
    for (const UStatusEffect* E : ActiveEffects)
        if (E && E->BlocksTurnAction()) return true;
    return false;
}

bool UStatusEffectManagerComponent::ShouldConfuseAttack() const
{
    for (const UStatusEffect* E : ActiveEffects)
        if (E && E->ConfusesTarget()) return true;
    return false;
}

bool UStatusEffectManagerComponent::CanReceiveHealing() const
{
    for (const UStatusEffect* E : ActiveEffects)
        if (E && E->BlocksHealing()) return false;
    return true;
}

bool UStatusEffectManagerComponent::CanGainAP() const
{
    for (const UStatusEffect* E : ActiveEffects)
        if (E && E->BlocksAPGain()) return false;
    return true;
}

bool UStatusEffectManagerComponent::CanUseAbilities() const
{
    for (const UStatusEffect* E : ActiveEffects)
        if (E && E->BlocksAbilityUse()) return false;
    return true;
}

bool UStatusEffectManagerComponent::ConsumeExtraTurn()
{
    for (UStatusEffect* E : ActiveEffects)
    {
        if (E && E->GrantsExtraTurn())
        {
            E->ConsumeExtraTurnGrant();
            return true;
        }
    }
    return false;
}

float UStatusEffectManagerComponent::GetEnemyTargetWeight() const
{
    float Weight = 1.f;
    for (const UStatusEffect* E : ActiveEffects)
        if (E) Weight *= E->GetTargetWeight();
    return Weight;
}
