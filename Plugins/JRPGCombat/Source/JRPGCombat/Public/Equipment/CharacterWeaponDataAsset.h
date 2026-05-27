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
 *   - Main weapon  → goes in the MainWeapon slot, drives melee/ability damage
 *   - Gun          → goes in the Gun slot, drives gun-shot ability damage
 *
 * Each playable character is locked to ONE WeaponType for the main slot.
 * The equipment system rejects a weapon whose WeaponType doesn't match the
 * character's. Guns are universal (every character has a gun slot).
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

    /** Archetype — must match the character's WeaponType for the main slot.
     *  For the gun slot, pick any of the gun-type entries (Pistol/Rifle/...). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Combat")
    EWeaponType WeaponType = EWeaponType::Katana;

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
