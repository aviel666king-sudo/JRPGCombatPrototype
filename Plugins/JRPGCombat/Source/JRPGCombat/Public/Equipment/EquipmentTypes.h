#pragma once

#include "CoreMinimal.h"
#include "EquipmentTypes.generated.h"

/**
 * Weapon archetype. Each playable character is locked to ONE archetype
 * (katana wielders can't equip a rapier, etc.) — character-defining choice
 * per the pitch. Add to this enum as new character weapons get designed.
 */
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None         UMETA(DisplayName = "None"),
    Katana       UMETA(DisplayName = "Katana"),
    Rapier       UMETA(DisplayName = "Rapier"),
    Spear        UMETA(DisplayName = "Spear"),
    Greatsword   UMETA(DisplayName = "Greatsword"),
    Daggers      UMETA(DisplayName = "Dual Daggers"),
    Bow          UMETA(DisplayName = "Bow"),
    Staff        UMETA(DisplayName = "Staff"),
    Gauntlet     UMETA(DisplayName = "Gauntlet"),
    Pistol       UMETA(DisplayName = "Pistol"),
    Rifle        UMETA(DisplayName = "Rifle"),
    Shotgun      UMETA(DisplayName = "Shotgun"),
};

/**
 * The three thematic chip slots per character (Brain / Heart / Spine).
 * Flavor only — no mechanical restriction per the pitch. Any chip can go in
 * any slot, but designers may want to author chips with implied themes.
 */
UENUM(BlueprintType)
enum class EChipSlot : uint8
{
    Brain   UMETA(DisplayName = "Brain"),
    Heart   UMETA(DisplayName = "Heart"),
    Spine   UMETA(DisplayName = "Spine"),
};

/**
 * Chip rarity. Per the pitch:
 *   - Common / Rare / Epic = world drops, shared across characters
 *   - CharacterSpecific = no rarity tier, locked to one character, unique
 */
UENUM(BlueprintType)
enum class EChipRarity : uint8
{
    Common              UMETA(DisplayName = "Common"),
    Rare                UMETA(DisplayName = "Rare"),
    Epic                UMETA(DisplayName = "Epic"),
    CharacterSpecific   UMETA(DisplayName = "Character-Specific"),
};
