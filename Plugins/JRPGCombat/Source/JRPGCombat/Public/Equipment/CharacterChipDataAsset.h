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

    /** True = this chip is an ARMOR chip (socketed into a piece of armor) and
     *  may NOT be placed in a character chip slot. False = a regular character
     *  chip and may NOT be socketed into armor. Keeps the two pools separate so
     *  the same chip can't sit in both places at once. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    bool bIsArmorChip = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    EChipRarity Rarity = EChipRarity::Common;

    /** When the rarity is CharacterSpecific, this points at the character
     *  class that's allowed to use it. Other characters can't equip it. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Identity")
    TSubclassOf<class APlayerCombatant> OwnerCharacterClass;

    // -------------------------------------------------------------------------
    //  Level + stat scaling
    //
    //  Chips have 3 levels. Each level has its own StatDelta authored by the
    //  designer, so e.g. L1 = +20 HP, L2 = +35 HP, L3 = +55 HP. Levels are
    //  upgraded via main-game currency + materials (upgrade pipeline TBD).
    // -------------------------------------------------------------------------

    /** Current level (1..3). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Chip|Stats",
              meta = (ClampMin = "1", ClampMax = "3"))
    int32 CurrentLevel = 1;

    /** Per-level stat delta. Index 0 = L1, 1 = L2, 2 = L3. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Stats")
    TArray<FCombatStats> StatDeltaPerLevel;

    /** Helper: returns the StatDeltaPerLevel entry for CurrentLevel (zeroed
     *  FCombatStats if out of range / unset). */
    FCombatStats GetCurrentStatDelta() const
    {
        const int32 Index = FMath::Clamp(CurrentLevel - 1, 0, 2);
        return StatDeltaPerLevel.IsValidIndex(Index)
            ? StatDeltaPerLevel[Index]
            : FCombatStats{};
    }

    /** Free-text passive effect description (the "unique effect" — modifies
     *  abilities, damage, etc.). Concrete passive hooks live in a future pass
     *  — this is the designer-facing label for now. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Chip|Effect",
              meta = (MultiLine = "true"))
    FText EffectDescription;
};
