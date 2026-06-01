#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CombatTypes.h"            // FCombatStats
#include "RosterSubsystem.generated.h"

class APlayerCombatant;
class ACombatantBase;
class UCharacterWeaponDataAsset;
class UCharacterArmorDataAsset;
class UCharacterChipDataAsset;

/** Which party a character is slotted into. Bench = owned but not deployed. */
UENUM(BlueprintType)
enum class EPartyAssignment : uint8
{
    Party1   UMETA(DisplayName = "Party 1"),
    Party2   UMETA(DisplayName = "Party 2"),
    Bench    UMETA(DisplayName = "Bench"),
};

/**
 * FPartyMemberRecord
 *
 * The PERSISTENT state of one party member, living on the GameInstance so it
 * survives level travel. Seeded once from the placed party actors, then kept up
 * to date (HP saved on level exit, restored on level entry). The roster screen
 * reads these records directly, so it works in the Open World / Camp where no
 * combatant actors are placed.
 */
USTRUCT()
struct FPartyMemberRecord
{
    GENERATED_BODY()

    UPROPERTY()
    TSubclassOf<APlayerCombatant> CharacterClass;

    UPROPERTY()
    FText DisplayName;

    UPROPERTY()
    FText Tagline;

    UPROPERTY()
    int32 Level = 1;

    UPROPERTY()
    float CurrentHP = 0.f;

    UPROPERTY()
    float MaxHP = 0.f;

    UPROPERTY()
    float Attack = 0.f;

    UPROPERTY()
    float Defense = 0.f;

    UPROPERTY()
    float Speed = 0.f;

    UPROPERTY()
    TObjectPtr<UCharacterWeaponDataAsset> MainWeapon;

    UPROPERTY()
    TObjectPtr<UCharacterWeaponDataAsset> Gun;

    UPROPERTY()
    TObjectPtr<UCharacterArmorDataAsset> Armor;

    UPROPERTY()
    TArray<TObjectPtr<UCharacterChipDataAsset>> Chips;

    UPROPERTY()
    int32 EquippedSkillCount = 0;

    UPROPERTY()
    EPartyAssignment Assignment = EPartyAssignment::Bench;

    bool IsDead() const { return CurrentHP <= 0.f; }
};

/**
 * URosterSubsystem
 *
 * Persistent party store + management layer. This is the single source of truth
 * for WHO is in the party, their current HP, their loadout, their party
 * assignment, and the shared heal/revive/AP charge pool — all of which now
 * survive level travel (Level -> Open World -> Camp and back).
 *
 * Lifecycle:
 *   - SeedFromParty(): first time the placed party is registered (from
 *     AJrpgGameMode::SetPlayerParty in the starting level), copy each actor's
 *     state into a record. Runs once.
 *   - RestoreHPToParty(): on entering a level that has placed party actors, push
 *     the saved CurrentHP back onto them.
 *   - SaveHPFromParty(): on leaving a level, read live actor HP back into the
 *     records. Records for members without a live actor are left untouched.
 *
 * In the Open World / Camp there are no placed actors, so the records simply
 * provide the roster display + the heal-charge pool for overworld healing.
 */
UCLASS()
class JRPGCOMBAT_API URosterSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    static constexpr int32 MaxPartySize = 3;

    // -------------------------------------------------------------------------
    //  Seeding + HP sync
    // -------------------------------------------------------------------------

    /** True once the party has been seeded from a starting level. */
    bool IsSeeded() const { return bSeeded; }

    /** Copy the placed party actors into persistent records (first time only).
     *  Also stocks the charge pool to its starting values. */
    void SeedFromParty(const TArray<ACombatantBase*>& Party);

    /** Push saved CurrentHP from records onto matching live actors (by class). */
    void RestoreHPToParty(const TArray<ACombatantBase*>& Party);

    /** Read live actor CurrentHP back into matching records (by class). */
    void SaveHPFromParty(const TArray<ACombatantBase*>& Party);

    // -------------------------------------------------------------------------
    //  Records access (read by the roster screen)
    // -------------------------------------------------------------------------

    int32 GetMemberCount() const { return Members.Num(); }
    const FPartyMemberRecord& GetMember(int32 Index) const { return Members[Index]; }
    bool IsValidMember(int32 Index) const { return Members.IsValidIndex(Index); }

    EPartyAssignment GetAssignment(int32 Index) const;
    bool SetAssignment(int32 Index, EPartyAssignment NewAssignment);
    int32 CountInParty(EPartyAssignment Party) const;

    // -------------------------------------------------------------------------
    //  Heal-charge pool (shared across the party, persists across levels)
    // -------------------------------------------------------------------------

    int32 GetHealCharges() const   { return HealCharges; }
    int32 GetReviveCharges() const { return ReviveCharges; }
    int32 GetAPCharges() const     { return APCharges; }
    int32 GetMaxHealCharges() const   { return MaxHealCharges; }
    int32 GetMaxReviveCharges() const { return MaxReviveCharges; }
    int32 GetMaxAPCharges() const     { return MaxAPCharges; }

    /** Restock all charges to their maxima (checkpoint rest). Also full-heals
     *  every living record + matching live actors. */
    void RestockAndHeal(const TArray<ACombatantBase*>& LiveParty);

    /** Overworld heal: spend one heal charge to fully restore every LIVING
     *  member (records + any matching live actors). Returns false if no charge
     *  remains or everyone is already full / dead. Works in any level. */
    bool UseHealingCharge(const TArray<ACombatantBase*>& LiveParty);

    // -------------------------------------------------------------------------
    //  Inventory discovery (used by the gear-switch UI — Part 2)
    // -------------------------------------------------------------------------

    void GetAvailableMainWeapons(UClass* CharacterClass, TArray<UCharacterWeaponDataAsset*>& Out) const;
    void GetAvailableGuns(UClass* CharacterClass, TArray<UCharacterWeaponDataAsset*>& Out) const;
    void GetAvailableArmors(TArray<UCharacterArmorDataAsset*>& Out) const;
    void GetAvailableChips(TArray<UCharacterChipDataAsset*>& Out) const;

private:

    UPROPERTY()
    TArray<FPartyMemberRecord> Members;

    bool bSeeded = false;

    int32 HealCharges = 0,   MaxHealCharges = 2;
    int32 ReviveCharges = 0, MaxReviveCharges = 1;
    int32 APCharges = 0,     MaxAPCharges = 2;

    /** Find the record index whose class matches the actor's class, or INDEX_NONE. */
    int32 FindRecordForActor(const ACombatantBase* Actor) const;

    template <typename T>
    void LoadAllAssetsOfClass(TArray<T*>& Out) const;

    void GetAvailableWeapons(UClass* CharacterClass, bool bGun, TArray<UCharacterWeaponDataAsset*>& Out) const;
};
