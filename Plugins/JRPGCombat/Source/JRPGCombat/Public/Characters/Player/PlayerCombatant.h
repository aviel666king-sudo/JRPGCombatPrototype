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
