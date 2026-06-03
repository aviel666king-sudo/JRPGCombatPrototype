#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "JrpgSaveGame.generated.h"

class APlayerCombatant;
class UCharacterWeaponDataAsset;
class UCharacterArmorDataAsset;
class UCharacterChipDataAsset;
class UCraftingMaterialDataAsset;

/**
 * Object references below are TObjectPtr to content data-assets. The SaveGame
 * proxy archive serialises them as asset paths and re-loads them on read, so
 * they round-trip across a restart. The MUTABLE upgrade state (weapon tier,
 * chip / armor level) lives on those shared assets, so we save it alongside the
 * reference and re-apply it on load.
 */

USTRUCT()
struct FOwnedWeaponSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TObjectPtr<UCharacterWeaponDataAsset> Asset = nullptr;
    UPROPERTY(SaveGame) uint8 Tier = 0;
};

USTRUCT()
struct FOwnedChipSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TObjectPtr<UCharacterChipDataAsset> Asset = nullptr;
    UPROPERTY(SaveGame) int32 Level = 1;
};

USTRUCT()
struct FOwnedArmorSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TObjectPtr<UCharacterArmorDataAsset> Asset = nullptr;
    UPROPERTY(SaveGame) int32 Level = 1;
    UPROPERTY(SaveGame) TObjectPtr<UCharacterChipDataAsset> SocketedChip = nullptr;
    UPROPERTY(SaveGame) int32 SocketedChipLevel = 1;
};

USTRUCT()
struct FMaterialSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TObjectPtr<UCraftingMaterialDataAsset> Asset = nullptr;
    UPROPERTY(SaveGame) int32 Count = 0;
};

USTRUCT()
struct FPartyMemberSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) TSubclassOf<APlayerCombatant> CharacterClass;
    UPROPERTY(SaveGame) int32 Level     = 1;
    UPROPERTY(SaveGame) int32 CurrentXP = 0;
    UPROPERTY(SaveGame) uint8 Assignment = 2;   // EPartyAssignment::Bench

    // Leveled base stats (so a reloaded L5 isn't reset to L1 stats).
    UPROPERTY(SaveGame) float BaseMaxHP   = 0.f;
    UPROPERTY(SaveGame) float BaseAttack  = 0.f;
    UPROPERTY(SaveGame) float BaseDefense = 0.f;
    UPROPERTY(SaveGame) float BaseSpeed   = 0.f;

    UPROPERTY(SaveGame) TObjectPtr<UCharacterWeaponDataAsset> MainWeapon = nullptr;
    UPROPERTY(SaveGame) TObjectPtr<UCharacterWeaponDataAsset> Gun        = nullptr;
    UPROPERTY(SaveGame) TObjectPtr<UCharacterArmorDataAsset>  Armor      = nullptr;
    UPROPERTY(SaveGame) TArray<TObjectPtr<UCharacterChipDataAsset>> Chips;

    // Skill tree + currency.
    UPROPERTY(SaveGame) TSet<FName> UnlockedNodes;
    UPROPERTY(SaveGame) TArray<FName> EquippedNodes;
    UPROPERTY(SaveGame) int32 SkillCoins = 0;
    UPROPERTY(SaveGame) int32 StatCoins  = 0;
};

USTRUCT()
struct FVisitedLevelSave
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) FName Level;
    UPROPERTY(SaveGame) TArray<FName> CheckpointIds;
};

/**
 * UJrpgSaveGame
 *
 * The on-disk snapshot of a run: party, economy, owned inventory (+ upgrade
 * levels), world-state done-keys, visited + last-rested checkpoints, and which
 * level the player was in. Written by USaveSubsystem.
 */
UCLASS()
class JRPGCOMBAT_API UJrpgSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    // Slot metadata (for the load menu).
    UPROPERTY(SaveGame) FString DisplayName;
    UPROPERTY(SaveGame) FDateTime Timestamp;
    UPROPERTY(SaveGame) FName SavedLevel;            // canonical level to open on load
    UPROPERTY(SaveGame) FTransform SavedPawnTransform; // where the player was standing

    // Party.
    UPROPERTY(SaveGame) TArray<FPartyMemberSave> Members;

    // Charge pool.
    UPROPERTY(SaveGame) int32 HealCharges   = 0;
    UPROPERTY(SaveGame) int32 ReviveCharges = 0;
    UPROPERTY(SaveGame) int32 APCharges     = 0;

    // Economy.
    UPROPERTY(SaveGame) int32 Gold = 0;
    UPROPERTY(SaveGame) TObjectPtr<UCraftingMaterialDataAsset> PrimaryMaterial = nullptr;
    UPROPERTY(SaveGame) TArray<FMaterialSave> Materials;

    // Owned inventory (with upgrade levels).
    UPROPERTY(SaveGame) TArray<FOwnedWeaponSave> OwnedWeapons;
    UPROPERTY(SaveGame) TArray<FOwnedArmorSave>  OwnedArmors;
    UPROPERTY(SaveGame) TArray<FOwnedChipSave>   OwnedChips;

    // World progress.
    UPROPERTY(SaveGame) TArray<FName> WorldStateKeys;
    UPROPERTY(SaveGame) TArray<FVisitedLevelSave> Visited;

    // Respawn anchor.
    UPROPERTY(SaveGame) bool bHasLastRested = false;
    UPROPERTY(SaveGame) FName LastRestedLevel;
    UPROPERTY(SaveGame) FName LastRestedCheckpointId;
    UPROPERTY(SaveGame) FTransform LastRestedTransform;
};
