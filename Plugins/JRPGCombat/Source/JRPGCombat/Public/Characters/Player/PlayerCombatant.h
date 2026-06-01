#pragma once

#include "CoreMinimal.h"
#include "Characters/Base/CombatantBase.h"
#include "CombatTypes.h"   // EElement
#include "PlayerCombatant.generated.h"

class UAnimMontage;
class UTexture2D;
class UCharacterChipDataAsset;
class UCharacterWeaponDataAsset;
class UCharacterArmorDataAsset;
class USkillTreeDataAsset;
class UCombatAbility;

/** Stats the stat-shop can permanently upgrade with StatCoins. */
UENUM(BlueprintType)
enum class EUpgradeStat : uint8
{
    MaxHP    UMETA(DisplayName = "Max HP"),
    Attack   UMETA(DisplayName = "Attack"),
    Defense  UMETA(DisplayName = "Defense"),
    Speed    UMETA(DisplayName = "Speed"),
};

/**
 * APlayerCombatant
 *
 * Structural base for all player-controlled characters.
 * Sets Team = Player and provides a future extension point for
 * player-specific mechanics (parry windows, UI hooks, input handling).
 *
 * Concrete characters inherit from this, NOT from ACombatantBase, e.g.:
 *
 *   ACombatantBase
 *     → APlayerCombatant
 *         → ACombatantFencer    (test character, lives in Testing/)
 *         → APlayerCombatant_Toren   (future, per sketches)
 *         → APlayerCombatant_<X>     (future, per sketches)
 *
 * Each character subclass should set its identity values (DisplayName,
 * WeaponType, PrimaryElement, Portrait) in its constructor or in the BP
 * defaults, and ship its own ability set via the AbilityManagerComponent.
 *
 * Equipment slots (chips / weapons / armor) are exposed here so the same
 * slot system works for every character. Stat application from equipment
 * is a future pass — for now the slots are just typed references that the
 * UI / save system can read.
 */
UCLASS(BlueprintType, Blueprintable)
class JRPGCOMBAT_API APlayerCombatant : public ACombatantBase
{
    GENERATED_BODY()

public:

    APlayerCombatant();

    // -------------------------------------------------------------------------
    //  Identity — set in the C++ subclass constructor OR in BP defaults.
    //  These extend ACombatantBase::DisplayName (inherited) with player-only
    //  identity bits read by the combat HUD, dialogue, character-select.
    // -------------------------------------------------------------------------

    /** One-line role/identity blurb. Optional, for character-select screens. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Identity",
              meta = (MultiLine = "true"))
    FText Tagline;

    /** Portrait shown in combat HUD + party screen. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Identity")
    TObjectPtr<UTexture2D> Portrait;

    /** Default damage element when no weapon is equipped / for self-buffs. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Identity")
    EElement PrimaryElement = EElement::Wind;

    // -------------------------------------------------------------------------
    //  Progression — per-character XP / level / currency.
    //  - XP comes from combat victories (sum of every defeated enemy's
    //    XPReward, divided equally across the entire winning party).
    //  - Levels apply a flat stat-growth to BaseStats and grant SkillCoins /
    //    StatCoins. The currency is banked for the future skill-tree and
    //    stat-shop systems; no spend UI yet.
    //  - DangerManager.PlayerEffectiveLevel is the party-aggregate read by
    //    the danger / encounter-merging / assassination-overlevel systems;
    //    BattleManager recomputes it as max(party.Level) after victory.
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Progression",
              meta = (ClampMin = "1"))
    int32 Level = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Progression",
              meta = (ClampMin = "0"))
    int32 CurrentXP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Progression",
              meta = (ClampMin = "0"))
    int32 SkillCoins = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Progression",
              meta = (ClampMin = "0"))
    int32 StatCoins = 0;

    /** XP required to advance FROM the current level. Linear curve:
     *  100 + 50 * (Level - 1). So L1→2 needs 100, L9→10 needs 500, L19→20
     *  needs 1000. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|Progression")
    int32 GetXPForNextLevel() const;

    /** Add XP. May trigger one or more LevelUp() calls if the haul covers
     *  multiple levels. Safe to call mid-battle; stat-growth applies to
     *  BaseStats and propagates at the next InitializeForBattle. */
    UFUNCTION(BlueprintCallable, Category = "Character|Progression")
    void GrantXP(int32 Amount);

    /** DEBUG: force one immediate level-up (stat growth + currency). Wired to
     *  a debug button in the skill-tree screen. */
    void DebugLevelUp();

    // -------------------------------------------------------------------------
    //  Equipment — chips + armor contribute additive StatDelta to BaseStats.
    //  Weapons are handled separately (their BaseDamage/Element are read by
    //  ability code paths, not by the generic stat system).
    // -------------------------------------------------------------------------

    /** Sum every equipped chip + armor's StatDelta, undo any previously-baked
     *  delta, and apply the new total to BaseStats. Re-equip-safe (calling it
     *  again after a swap correctly diffs the change). Called automatically at
     *  the start of every battle via InitializeForBattle. */
    void ApplyEquipmentBonuses();

    /** Override so equipment bonuses bake into BaseStats before the resource
     *  pool (HP/AP max) is initialised from BaseStats. */
    virtual void InitializeForBattle() override;

    // -------------------------------------------------------------------------
    //  Stat shop — spend StatCoins on permanent BaseStats upgrades. Read by
    //  UStatShopWidget (opened at checkpoints).
    // -------------------------------------------------------------------------

    /** StatCoin cost of one upgrade of this stat. Flat 1 for now. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|StatShop")
    int32 GetUpgradeCost(EUpgradeStat Stat) const;

    /** How much one purchase adds to the stat (+10 HP, +2 Atk/Def, +1 Spd). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|StatShop")
    float GetUpgradeAmount(EUpgradeStat Stat) const;

    /** Current BaseStats value for the given stat (for shop display). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|StatShop")
    float GetStatValue(EUpgradeStat Stat) const;

    /** Spend StatCoins to permanently raise the stat. Returns false if the
     *  player can't afford it. MaxHP upgrades refresh the HP cap immediately. */
    UFUNCTION(BlueprintCallable, Category = "Character|StatShop")
    bool TryUpgradeStat(EUpgradeStat Stat);

    // -------------------------------------------------------------------------
    //  Skill tree — spend SkillCoins to unlock combat abilities. The branching
    //  structure (nodes, costs, prerequisites) lives in SkillTree; the unlocked
    //  set lives here. Unlocked abilities are merged into the combat menu by
    //  UAbilityManagerComponent::InitializeAbilities.
    // -------------------------------------------------------------------------

    /** This character's skill-tree definition. Assign in the BP. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|SkillTree")
    TObjectPtr<USkillTreeDataAsset> SkillTree;

    /** True if the given node has been bought. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|SkillTree")
    bool IsNodeUnlocked(FName NodeId) const { return UnlockedNodes.Contains(NodeId); }

    /** True if every prerequisite of the node is already unlocked. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|SkillTree")
    bool ArePrerequisitesMet(FName NodeId) const;

    /** True if the node exists, isn't already unlocked, prereqs are met, and
     *  the player can afford the SkillCoin cost. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|SkillTree")
    bool CanUnlockNode(FName NodeId) const;

    /** Spend SkillCoins to unlock the node. Returns false if CanUnlockNode is
     *  false. On success the granted ability appears in the next battle. */
    UFUNCTION(BlueprintCallable, Category = "Character|SkillTree")
    bool TryUnlockNode(FName NodeId);

    /** Collect the ability classes for every unlocked node. */
    void GetUnlockedAbilityClasses(TArray<TSubclassOf<UCombatAbility>>& Out) const;

    /** Read-only access to this character's tree (for the skill-tree UI). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Character|SkillTree")
    USkillTreeDataAsset* GetSkillTree() const { return SkillTree; }

    // -------------------------------------------------------------------------
    //  Skill loadout — a character may UNLOCK any number of skills but only
    //  EQUIP up to MaxEquippedSkills into battle at once. The combat menu is
    //  built from the equipped set; the player swaps them in the skill tree.
    // -------------------------------------------------------------------------

    static constexpr int32 MaxEquippedSkills = 6;

    bool  IsNodeEquipped(FName NodeId) const { return EquippedNodes.Contains(NodeId); }
    int32 GetEquippedSkillCount() const      { return EquippedNodes.Num(); }
    bool  CanEquipMore() const               { return EquippedNodes.Num() < MaxEquippedSkills; }

    /** Equip an unlocked skill if there's a free slot. Returns false otherwise. */
    bool TryEquipNode(FName NodeId);

    /** Remove a skill from the active loadout. */
    void UnequipNode(FName NodeId);

    /** Equip if benched (and room), unequip if currently equipped. */
    bool ToggleEquipNode(FName NodeId);

    /** Ability classes for the currently equipped skills (combat menu source). */
    void GetEquippedAbilityClasses(TArray<TSubclassOf<UCombatAbility>>& Out) const;

protected:

    virtual void BeginPlay() override;

    /** Subclasses fill OutTree with their character-specific skill nodes when
     *  no SkillTree asset was assigned in the editor. Base does nothing. */
    virtual void PopulateDefaultSkillTree(USkillTreeDataAsset* OutTree) const {}

    /** Subclasses list node ids the character begins a run with (unlocked +
     *  auto-equipped). Base does nothing. */
    virtual void GetStartingSkillNodes(TArray<FName>& Out) const {}

    /** Set of unlocked skill-tree node ids. Persists on the runtime actor. */
    UPROPERTY(BlueprintReadOnly, Category = "Character|SkillTree")
    TSet<FName> UnlockedNodes;

    /** Tracks the equipment StatDelta currently baked into BaseStats so we can
     *  diff it out on re-equip. Transient — not saved (BaseStats already holds
     *  the applied total; saving both would double-count on load). */
    FCombatStats AppliedEquipmentDelta;

    /** Ordered list of equipped node ids (max MaxEquippedSkills). Subset of
     *  UnlockedNodes. Drives which skills appear in the combat menu. */
    TArray<FName> EquippedNodes;

    /** Single level-up step. Increments Level, applies flat stat growth to
     *  BaseStats (+5 HP, +1 Atk, +1 Def, +1 Spd), grants currency (+2 Skill,
     *  +3 Stat). Called from GrantXP when CurrentXP crosses the threshold. */
    void LevelUp();

public:

    // -------------------------------------------------------------------------
    //  Equipment slots — 3 chips + 1 armor + 2 weapons (main + gun).
    //
    //  These are EditAnywhere so a placed BP_Player<X> in the level can have
    //  its loadout configured per-instance for testing. The real game flow
    //  will populate them from save data when the player picks a loadout.
    // -------------------------------------------------------------------------

    /** Up to 3 chip slots. Empty entries = nothing equipped in that slot. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Equipment")
    TArray<TObjectPtr<UCharacterChipDataAsset>> Chips;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Equipment")
    TObjectPtr<UCharacterArmorDataAsset> Armor;

    /** Main weapon — its WeaponType must match this character's WeaponType. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Equipment")
    TObjectPtr<UCharacterWeaponDataAsset> MainWeapon;

    /** Gun slot — universal across characters. Any UCharacterWeaponDataAsset
     *  with bIsGun = true can go here. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Equipment")
    TObjectPtr<UCharacterWeaponDataAsset> Gun;

    /** Starting main weapon — auto-equipped into MainWeapon in BeginPlay if
     *  that slot is empty. Lets each character ship with a guaranteed default
     *  weapon (the one they have at game start, per the design pitch). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Equipment")
    TObjectPtr<UCharacterWeaponDataAsset> StartingMainWeapon;

    /** Starting gun — auto-equipped into Gun in BeginPlay if that slot is
     *  empty. Same purpose as StartingMainWeapon. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character|Equipment")
    TObjectPtr<UCharacterWeaponDataAsset> StartingGun;

    /** Max chip slots per character. Caps the Chips array at this size at
     *  edit time / equip time. */
    static constexpr int32 MaxChipSlots = 3;

    // -------------------------------------------------------------------------
    //  Animation
    // -------------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combatant|Animation")
    TObjectPtr<UAnimMontage> GunMontage;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combatant|Animation")
    TObjectPtr<UAnimMontage> ParryMontage;
};
