#pragma once

#include "CoreMinimal.h"
#include "EquipmentTypes.generated.h"

/**
 * Weapon family — a descriptive tag grouping variants of the same kind of
 * weapon. Used for inventory sorting + UI display ("Shock Baton Mk II",
 * "Heavy Revolver", etc.). NOT used for equip validation — that's done by
 * matching the weapon's OwnerCharacterClass to the wielder's class.
 *
 * Each character usually has TWO unique families (one main + one gun), e.g.
 * Shroud & Boss → ShockBaton + Revolver. Extend this enum as new characters
 * are designed.
 */
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None         UMETA(DisplayName = "None"),
    // Shroud & Boss
    ShockBaton   UMETA(DisplayName = "Shock Baton"),
    Revolver     UMETA(DisplayName = "Revolver"),
    // Placeholders for future characters — extend as needed
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

/**
 * Weapon tier — determines the magnitude of a weapon's single buffed stat and
 * which passives it has unlocked. Main weapons range D..S+; guns range C..S
 * (the unused tier slots are simply 0 in BuffValuePerTier).
 */
UENUM(BlueprintType)
enum class EWeaponTier : uint8
{
    D       UMETA(DisplayName = "D"),
    C       UMETA(DisplayName = "C"),
    B       UMETA(DisplayName = "B"),
    A       UMETA(DisplayName = "A"),
    S       UMETA(DisplayName = "S"),
    SPlus   UMETA(DisplayName = "S+"),
};

/**
 * Which BaseStat field a weapon's tier scaling buffs. Authored per-weapon so
 * one Katana variant can buff Attack while another Katana buffs Speed.
 */
UENUM(BlueprintType)
enum class EBuffedStat : uint8
{
    None        UMETA(DisplayName = "None"),
    MaxHP       UMETA(DisplayName = "Max HP"),
    MaxAP       UMETA(DisplayName = "Max AP"),
    Attack      UMETA(DisplayName = "Attack"),
    Defense     UMETA(DisplayName = "Defense"),
    Speed       UMETA(DisplayName = "Speed"),
    CritChance  UMETA(DisplayName = "Crit Chance"),
};
