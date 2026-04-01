#include "Components/AbilityManagerComponent.h"
#include "Abilities/CombatAbility.h"
#include "Characters/Base/CombatantBase.h"
#include "Engine/Engine.h"

UAbilityManagerComponent::UAbilityManagerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------------
//  Setup
// -----------------------------------------------------------------------------

void UAbilityManagerComponent::InitializeAbilities(ACombatantBase* OwningCombatant)
{
    Abilities.Empty();

    for (const TSubclassOf<UCombatAbility>& AbilityClass : AbilityClasses)
    {
        if (!AbilityClass) { continue; }

        UCombatAbility* NewAbility = NewObject<UCombatAbility>(this, AbilityClass);
        NewAbility->Initialize(OwningCombatant);
        Abilities.Add(NewAbility);
    }
}

// -----------------------------------------------------------------------------
//  Activation
// -----------------------------------------------------------------------------

bool UAbilityManagerComponent::TryActivateAbility(int32 AbilityIndex, const TArray<ACombatantBase*>& Targets)
{
    UCombatAbility* Ability = GetAbility(AbilityIndex);
    if (!Ability) { return false; }

    ACombatantBase* Owner = Cast<ACombatantBase>(GetOwner());
    if (!Owner) { return false; }

    if (!Ability->CanActivate(Owner)) { return false; }

    // Pay all costs before executing.
    for (const FAbilityCost& Cost : Ability->Costs)
    {
        const float Before = Owner->GetCurrentResource(Cost.ResourceType);
        Owner->SpendResource(Cost.ResourceType, Cost.Amount);
        const float After = Owner->GetCurrentResource(Cost.ResourceType);

        UE_LOG(LogTemp, Log, TEXT("[AbilityManager] %s cost: %.1f %s | %.1f → %.1f"),
            *Ability->DisplayName.ToString(),
            Cost.Amount,
            *UEnum::GetValueAsString(Cost.ResourceType),
            Before, After);
    }

    if (Ability->Costs.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[AbilityManager] %s has NO costs defined — check that the C++ subclass constructor adds to Costs."),
            *Ability->DisplayName.ToString());
    }

    Ability->Execute(Owner, Targets);
    Ability->StartCooldown();

    return true;
}

bool UAbilityManagerComponent::CanActivateAbility(int32 AbilityIndex) const
{
    const UCombatAbility* Ability = GetAbility(AbilityIndex);
    if (!Ability) { return false; }

    const ACombatantBase* Owner = Cast<ACombatantBase>(GetOwner());
    return Owner && Ability->CanActivate(Owner);
}

// -----------------------------------------------------------------------------
//  Turn lifecycle
// -----------------------------------------------------------------------------

void UAbilityManagerComponent::TickAllCooldowns()
{
    for (UCombatAbility* Ability : Abilities)
    {
        if (Ability)
        {
            Ability->TickCooldown();
        }
    }
}

// -----------------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------------

UCombatAbility* UAbilityManagerComponent::GetAbility(int32 Index) const
{
    if (!Abilities.IsValidIndex(Index)) { return nullptr; }
    return Abilities[Index];
}
