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

    /** Damage element. Each weapon carries its own — abilities can override. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EElement Element = EElement::Wind;

    /** Physical / Magical / Special. Affects target reaction lookup. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EDamageType DamageType = EDamageType::Physical;

    /** Base damage before stat modifiers / ability multipliers. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat",
              meta = (ClampMin = "0.0"))
    float BaseDamage = 20.f;
};
