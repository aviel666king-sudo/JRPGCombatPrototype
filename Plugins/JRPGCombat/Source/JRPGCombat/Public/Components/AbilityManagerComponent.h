#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "AbilityManagerComponent.generated.h"

class UCombatAbility;
class ACombatantBase;

/**
 * UAbilityManagerComponent
 *
 * Attached to ACombatantBase. Owns the combatant's list of ability instances
 * for the current battle. Validates, pays costs, fires abilities, and ticks
 * cooldowns. All ability activation goes through this component.
 */
UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class JRPGCOMBAT_API UAbilityManagerComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UAbilityManagerComponent();

    // -------------------------------------------------------------------------
    //  Setup
    // -------------------------------------------------------------------------

    /**
     * Instantiate one UCombatAbility object for each class in AbilityClasses.
     * Called by ACombatantBase::InitializeForBattle.
     */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    void InitializeAbilities(ACombatantBase* OwningCombatant);

    /**
     * Ability classes to instantiate. Populated via the editor on the owning
     * Actor, or at runtime from a data asset.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Abilities")
    TArray<TSubclassOf<UCombatAbility>> AbilityClasses;

    // -------------------------------------------------------------------------
    //  Activation
    // -------------------------------------------------------------------------

    /**
     * Try to activate the ability at the given index.
     *   1. Calls CanActivate — returns false and does nothing if it fails.
     *   2. Pays all resource costs via the owning ACombatantBase.
     *   3. Calls Execute on the ability.
     *   4. Starts the cooldown.
     *
     * Returns true if the ability fired successfully.
     */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    bool TryActivateAbility(int32 AbilityIndex, const TArray<ACombatantBase*>& Targets);

    /** Returns true if the ability at AbilityIndex exists and passes CanActivate. */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    bool CanActivateAbility(int32 AbilityIndex) const;

    // -------------------------------------------------------------------------
    //  Turn lifecycle
    // -------------------------------------------------------------------------

    /** Tick cooldowns on all abilities. Called by ACombatantBase::OnTurnEnd. */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    void TickAllCooldowns();

    // -------------------------------------------------------------------------
    //  Queries
    // -------------------------------------------------------------------------

    /** Returns the ability instance at the given index, or nullptr. */
    UFUNCTION(BlueprintCallable, Category = "Abilities")
    UCombatAbility* GetAbility(int32 Index) const;

    UFUNCTION(BlueprintCallable, Category = "Abilities")
    int32 GetAbilityCount() const { return Abilities.Num(); }

private:

    /** Live ability instances for this battle. Built by InitializeAbilities. */
    UPROPERTY()
    TArray<TObjectPtr<UCombatAbility>> Abilities;
};
