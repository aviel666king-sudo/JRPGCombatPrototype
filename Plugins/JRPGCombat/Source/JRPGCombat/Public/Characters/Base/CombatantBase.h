#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "CombatantBase.generated.h"

class UAbilityManagerComponent;
class UStatusEffectManagerComponent;

UCLASS(Abstract, BlueprintType, Blueprintable)
class JRPGCOMBAT_API ACombatantBase : public AActor
{
    GENERATED_BODY()

public:

    ACombatantBase();

    // -------------------------------------------------------------------------
    //  Config
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combatant")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combatant")
    ECombatTeam Team = ECombatTeam::Player;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combatant|Stats")
    FCombatStats BaseStats;

    // -------------------------------------------------------------------------
    //  Components
    // -------------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Components")
    TObjectPtr<UAbilityManagerComponent> AbilityManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Components")
    TObjectPtr<UStatusEffectManagerComponent> StatusEffectManager;

    // -------------------------------------------------------------------------
    //  Battle setup
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Combatant")
    virtual void InitializeForBattle();

    // -------------------------------------------------------------------------
    //  Turn callbacks
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combatant")
    void OnTurnStart();
    virtual void OnTurnStart_Implementation();

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Combatant")
    void OnTurnEnd();
    virtual void OnTurnEnd_Implementation();

    // -------------------------------------------------------------------------
    //  Damage — C++ pipeline
    // -------------------------------------------------------------------------

    virtual void ApplyDamage(FDamagePayload& Payload);

    virtual float GetOutgoingDamageMultiplier() const { return 1.0f; }
    virtual float GetIncomingDamageMultiplier() const { return 1.0f; }

    // -------------------------------------------------------------------------
    //  Damage — Blueprint API
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Combatant|Damage",
              meta = (DisplayName = "Apply Damage"))
    void BP_ApplyDamage(ACombatantBase* Source, float BaseDamage, EDamageType DamageType);

    UFUNCTION(BlueprintImplementableEvent, Category = "Combatant|Damage",
              meta = (DisplayName = "On Damage Taken"))
    void BP_OnDamageTaken(ACombatantBase* Source, float FinalDamage, EDamageType DamageType);

    // -------------------------------------------------------------------------
    //  Healing — C++ pipeline
    // -------------------------------------------------------------------------

    virtual void ApplyHealing(float Amount, ACombatantBase* Source = nullptr);

    // -------------------------------------------------------------------------
    //  Healing — Blueprint API
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Combatant|Healing",
              meta = (DisplayName = "Apply Healing"))
    void BP_ApplyHealing(ACombatantBase* Source, float Amount);

    UFUNCTION(BlueprintImplementableEvent, Category = "Combatant|Healing",
              meta = (DisplayName = "On Healing Received"))
    void BP_OnHealingReceived(ACombatantBase* Source, float FinalAmount);

    // -------------------------------------------------------------------------
    //  NEW: Revival
    //
    //  Restores HP to HPPercent × MaxHP and clears the dead state.
    //  Only does anything if the combatant is currently dead.
    //  Called by Ability_RevivalProtocol.
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Combatant|Healing")
    void Revive(float HPPercent);

    // -------------------------------------------------------------------------
    //  Resources
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetCurrentResource(EResourceType Type) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetMaxResource(EResourceType Type) const;

    UFUNCTION(BlueprintCallable, Category = "Combatant|Resources")
    void SpendResource(EResourceType Type, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Combatant|Resources")
    void RestoreResource(EResourceType Type, float Amount);

    UFUNCTION(BlueprintCallable, Category = "Combatant|Resources")
    bool CanAffordCost(const FAbilityCost& Cost) const;

    // HP helpers
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetCurrentHP() const { return GetCurrentResource(EResourceType::HP); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetMaxHP() const { return GetMaxResource(EResourceType::HP); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetMissingHP() const { return GetMaxHP() - GetCurrentHP(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetHealthPercent() const;

    // AP helpers
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetCurrentAP() const { return GetCurrentResource(EResourceType::AP); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetMaxAP() const { return GetMaxResource(EResourceType::AP); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resources")
    float GetAPPercent() const;

    // Stat helpers
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Stats")
    float GetBaseAttack() const { return BaseStats.Attack; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Stats")
    float GetBaseDefense() const { return BaseStats.Defense; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Stats")
    float GetSpeed() const { return BaseStats.Speed; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Stats")
    float GetEffectiveSpeed() const;

    // -------------------------------------------------------------------------
    //  State queries
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant")
    bool IsDead() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant")
    bool IsAlive() const { return !IsDead(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant")
    ECombatTeam GetTeam() const { return Team; }

protected:

    virtual void BeginPlay() override;

private:

    UPROPERTY()
    TMap<EResourceType, FResourcePool> Resources;
};
