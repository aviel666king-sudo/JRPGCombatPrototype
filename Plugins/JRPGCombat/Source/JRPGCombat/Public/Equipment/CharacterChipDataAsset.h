#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CombatTypes.h"            // EElement, FCombatStats
#include "Equipment/EquipmentTypes.h"  // EChipSlot, EChipRarity
#include "CharacterChipDataAsset.generated.h"

class UTexture2D;

/**
 * UCharacterChipDataAsset
 *
 * One slottable chip. Authored as a UDataAsset in the Content Browser so
 * designers can iterate on chip stats without touching code.
 *
 * Where it goes:
 *   - APlayerCombatant has 3 chip slots
 *   - The chip's StatDelta is applied to the wielder's BaseStats at
 *     equipment time (apply hook lives on APlayerCombatant — see future
 *     "ApplyEquipment" pass; for now this is just data).
 *
 * Slot / rarity are flavor only — any chip can go in any of the 3 chip
 * slots, and CharacterSpecific chips simply skip the rarity rolls and are
 * usually tied to a specific character via OwnerCharacterClass.
 */
UCLASS(BlueprintType)
class JRPGCOMBAT_API UCharacterChipDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity",
              meta = (MultiLine = "true"))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    TObjectPtr<UTexture2D> Icon;

    /** Suggested thematic slot. Flavor only — any chip can go in any slot. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    EChipSlot ThematicSlot = EChipSlot::Brain;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    EChipRarity Rarity = EChipRarity::Common;

    /** Optional thematic element. Defaults to a neutral physical. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    EElement ThematicElement = EElement::Wind;

    /** When the rarity is CharacterSpecific, this points at the character
     *  class that's allowed to use it. Other characters can't equip it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    TSubclassOf<class APlayerCombatant> OwnerCharacterClass;

    /** Stat delta applied additively to the wielder's BaseStats when equipped.
     *  e.g. MaxHP = 50 → +50 max HP. Negative values are fine for trade-offs. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Stats")
    FCombatStats StatDelta;

    /** Free-text gameplay effect description (the "unique effect" per pitch).
     *  Concrete effect hooks will land in a future pass — this is the
     *  designer-facing label for now. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Effect",
              meta = (MultiLine = "true"))
    FText EffectDescription;
};
