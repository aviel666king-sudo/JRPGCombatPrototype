#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "CombatantBase.generated.h"

class UAbilityManagerComponent;
class UStatusEffectManagerComponent;
class UCapsuleComponent;
class USkeletalMeshComponent;
class UAnimMontage;
class UWidgetComponent;
class UUserWidget;

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
    //  Element resistances
    //  Set per-combatant in Blueprint defaults. Unlisted elements = Normal.
    //  Example: add Fire→Weak to make this unit take 1.5x fire damage.
    // -------------------------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant|Resistances")
    TMap<EElement, EResistanceType> ElementResistances;

    /** Returns this combatant's resistance type for the given element. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combatant|Resistances")
    EResistanceType GetResistanceType(EElement Element) const;

    /** Returns the damage multiplier for the given element (Weak=1.5, Normal=1.0, Resist=0.5, Block=0.0).
     *  Absorb is handled separately in ApplyDamage — this returns 0.f for it. */
    float GetElementMultiplier(EElement Element) const;

    // -------------------------------------------------------------------------
    //  Visual components
    // -------------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Visual")
    TObjectPtr<UCapsuleComponent> CapsuleComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Visual")
    TObjectPtr<USkeletalMeshComponent> Mesh;

    // -------------------------------------------------------------------------
    //  Combat components
    // -------------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Components")
    TObjectPtr<UAbilityManagerComponent> AbilityManager;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Components")
    TObjectPtr<UStatusEffectManagerComponent> StatusEffectManager;

    // -------------------------------------------------------------------------
    //  Damage Number Widget Stack (floating text above head on hit)
    //  Lives on every combatant automatically — no per-BP wiring needed.
    //  We keep a small pool of widget components stacked vertically. When
    //  damage hits, the lowest unused slot is chosen so multiple numbers can
    //  appear at once for sequence/multi-hit attacks. Override defaults in BP
    //  if a character needs a different height, duration, or stack count.
    // -------------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|UI")
    TArray<TObjectPtr<UWidgetComponent>> DamageWidgets;

    /** Widget class used by every slot in DamageWidgets. Defaults to
     *  WBP_DamageNumber via ConstructorHelpers; override per BP if needed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant|UI")
    TSubclassOf<UUserWidget> DamageWidgetClass;

    /** How long each damage number stays on screen after a hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant|UI")
    float DamageWidgetVisibleDuration = 1.0f;

    /** Local Z height (above the capsule pivot) of the lowest damage-number slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant|UI")
    float DamageWidgetBaseZ = 200.f;

    /** Vertical spacing between stacked damage numbers. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combatant|UI")
    float DamageWidgetStackSpacing = 60.f;

    // -------------------------------------------------------------------------
    //  Battle setup
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "Combatant")
    virtual void InitializeForBattle();

    /** The arena spawn-slot actor (grass-pad / platform) this combatant was
     *  teleported to at battle start. Set by ABattleManager::TeleportToSpawn.
     *  BP can use it as a stable world anchor for UI (floating damage numbers,
     *  status icons) instead of relying on the actor's own location. Null
     *  outside of combat. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combatant|Battle")
    TObjectPtr<AActor> CurrentSpawnPoint;

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

    /** Rich version of OnDamageTaken — fires with the full payload so Blueprint
     *  can spawn floating damage numbers, resistance tags ("Weak!", "Resist",
     *  "Block", "Absorb"), and elemental icons.
     *
     *  Fires after damage (or absorb-healing) has been applied and resources
     *  spent. Payload.ResolvedDamage is the final amount; Payload.HitResistance
     *  is the target's reaction; Payload.Element is the element used. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combatant|Damage",
              meta = (DisplayName = "On Damage Resolved"))
    void BP_OnDamageResolved(const FDamagePayload& Payload);

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

    /** Optional one-line subtitle shown on the combat status card (e.g. the
     *  Fencer's current stance). Base returns empty; subclasses override. */
    virtual FText GetCombatSubtitle() const { return FText::GetEmpty(); }

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

    // -------------------------------------------------------------------------
    //  Animation — montage slots (assign in BP_PlayerFencer / BP_EnemyUnit)
    // -------------------------------------------------------------------------

    // Shared by all combatants
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combatant|Animation")
    TObjectPtr<UAnimMontage> AttackMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combatant|Animation")
    TObjectPtr<UAnimMontage> CastMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combatant|Animation")
    TObjectPtr<UAnimMontage> HitReactMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combatant|Animation")
    TObjectPtr<UAnimMontage> DeathMontage;

    // -------------------------------------------------------------------------
    //  Animation — helpers
    // -------------------------------------------------------------------------

    /** Play the montage that matches an ability category (Melee/Gun/Skill). */
    UFUNCTION(BlueprintCallable, Category = "Combatant|Animation")
    void PlayAbilityAnimation(EAbilityCategory Category);

    /** Play the hit-react montage, or death montage if the combatant just died. */
    UFUNCTION(BlueprintCallable, Category = "Combatant|Animation")
    void PlayReactionAnimation();

    /** Play any montage directly on this combatant's mesh. */
    UFUNCTION(BlueprintCallable, Category = "Combatant|Animation")
    void PlayMontage(UAnimMontage* Montage);

protected:

    virtual void BeginPlay() override;

    /** Picks an unused (or oldest) widget slot, populates it with the hit's
     *  data, makes it visible, and schedules its hide. Called automatically
     *  by ApplyDamage — no per-BP wiring needed. */
    void ShowDamageNumber(const FDamagePayload& Payload);

    /** Hides a single damage-number slot. Called by the per-slot hide timer. */
    void HideDamageWidgetSlot(int32 SlotIndex);

private:

    UPROPERTY()
    TMap<EResourceType, FResourcePool> Resources;

    /** One handle per slot in DamageWidgets (parallel array). */
    TArray<FTimerHandle> DamageWidgetHideTimers;

    /** Round-robin cursor — when all slots are busy and a new hit lands,
     *  this slot gets overwritten (oldest visible). */
    int32 NextDamageWidgetIndex = 0;
};
