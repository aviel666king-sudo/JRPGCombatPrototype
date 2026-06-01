#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"            // EElement, EDamageType
#include "Equipment/EquipmentTypes.h"  // EWeaponType
#include "CharacterWeaponDataAsset.generated.h"

class UTexture2D;
class USkeletalMesh;

/**
 * UCharacterWeaponDataAsset
 *
 * A main weapon OR a gun. Distinguished by bIsGun:
 *   - Main weapon → goes in the MainWeapon slot
 *   - Gun         → goes in the Gun slot
 *
 * EQUIP RULE: a weapon is locked to ONE specific character class via
 * OwnerCharacterClass. Only that exact playable-character subclass can
 * equip this weapon. There's no cross-character weapon sharing — e.g. a
 * Revolver authored for Shroud & Boss cannot be equipped by anyone else,
 * even if they happen to use a "gun-family" weapon.
 *
 * Each character will accumulate a collection of weapons over time — all
 * variants of the same two families (one main + one gun per character).
 * Variants differ in stats / passives but never in ownership.
 *
 * Per the pitch, upgrades unlock passives + stat increases — that's handled
 * by a future upgrade tier system, not on the base data asset.
 */
UCLASS(BlueprintType)
class JRPGCOMBAT_API UCharacterWeaponDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity",
              meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity")
    TObjectPtr<UTexture2D> Icon;

    /** Visual mesh hung off the character's hand socket. Optional. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity")
    TObjectPtr<USkeletalMesh> VisualMesh;

    /** WHICH PLAYABLE CHARACTER CAN EQUIP THIS WEAPON. Required.
     *  Pick the APlayerCombatant subclass (e.g. APlayerCombatant_ShroudBoss)
     *  this weapon is locked to. The equipment system rejects any equip
     *  attempt where the wielder's class != this class. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Identity")
    TSubclassOf<class APlayerCombatant> OwnerCharacterClass;

    /** Descriptive family tag (Shock Baton / Revolver / Katana / ...).
     *  UI / inventory sorting only — NOT used for equip validation. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EWeaponType WeaponType = EWeaponType::None;

    /** True = goes in the gun slot. False = main weapon. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    bool bIsGun = false;

    /** Physical element. EVERY weapon carries one — pick from the physical
     *  half of EElement: Pierce / Slash / Smash. Set to None ONLY for purely
     *  magical weapons (staves, etc.) that deal no physical damage at all.
     *  (The dropdown shows all EElement values; designers must stay within
     *  the physical subset for this field.) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EElement PhysicalElement = EElement::Slash;

    /** Magical element. EVERY weapon carries one — pick from the magical half
     *  of EElement: Wind / Fire / Ice / Electric / Nature / Light / Dark, or
     *  None for plain non-imbued weapons. A fire-imbued katana = PhysicalElement
     *  Slash + MagicalElement Fire; a pure flame staff = PhysicalElement None
     *  + MagicalElement Fire. Abilities can override either field at cast-time. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EElement MagicalElement = EElement::None;

    /** Physical / Magical / Special. Affects target reaction lookup. Independent
     *  of the element fields — a katana with MagicalElement=Fire can still be
     *  DamageType=Physical (fire-imbued blade) or DamageType=Magical (pure
     *  flame slash). Designer chooses. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EDamageType DamageType = EDamageType::Physical;

    /** Base damage before stat modifiers / ability multipliers. Read by future
     *  ability paths that scale damage off the weapon (not yet wired). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat",
              meta = (ClampMin = "0.0"))
    float BaseDamage = 20.f;

    // -------------------------------------------------------------------------
    //  Tier + stat buff
    //
    //  Each weapon variant buffs ONE BaseStats field, and the magnitude of that
    //  buff scales with CurrentTier. Designer authors which stat is buffed and
    //  the value at each tier — guns leave the D slot at 0 since they start at
    //  C; S+ is Main-only and is 0 for guns.
    //
    //  Future tiers also unlock passive effects — that infrastructure isn't
    //  built yet (planned passive system), so for now this is stats-only.
    // -------------------------------------------------------------------------

    /** Which BaseStats field this weapon buffs while equipped. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Tier")
    EBuffedStat BuffedStat = EBuffedStat::Attack;

    /** Current tier this weapon is at. Determines which entry of
     *  BuffValuePerTier is applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weapon|Tier")
    EWeaponTier CurrentTier = EWeaponTier::D;

    /** Buff value for BuffedStat at each tier, indexed by EWeaponTier:
     *  [0]=D, [1]=C, [2]=B, [3]=A, [4]=S, [5]=S+. Guns leave [0] at 0. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Tier")
    TArray<float> BuffValuePerTier;

    /** Passive unlocked at each tier (descriptive text for now — the system
     *  that makes passives fire in combat is a later phase). Index by
     *  EWeaponTier: [0]=D … [5]=S+. Leave an entry empty for "no new passive at
     *  this tier". The roster screen lists every passive up to CurrentTier. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Tier",
              meta = (MultiLine = "true"))
    TArray<FText> PassiveDescriptionPerTier;

    /** Helper: passive text for a given tier index, or empty. */
    FText GetPassiveForTier(int32 TierIndex) const
    {
        return PassiveDescriptionPerTier.IsValidIndex(TierIndex)
            ? PassiveDescriptionPerTier[TierIndex] : FText::GetEmpty();
    }

    /** Helper: returns the BuffValuePerTier entry for CurrentTier (0 if out of
     *  range / unset). */
    float GetCurrentBuffValue() const
    {
        const int32 Index = static_cast<int32>(CurrentTier);
        return BuffValuePerTier.IsValidIndex(Index) ? BuffValuePerTier[Index] : 0.f;
    }
};
