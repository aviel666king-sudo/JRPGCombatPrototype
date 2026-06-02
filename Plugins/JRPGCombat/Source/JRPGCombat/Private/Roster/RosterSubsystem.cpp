#include "Roster/RosterSubsystem.h"
#include "Characters/Player/PlayerCombatant.h"
#include "Characters/Base/CombatantBase.h"
#include "Equipment/CharacterWeaponDataAsset.h"
#include "Equipment/CharacterArmorDataAsset.h"
#include "Equipment/CharacterChipDataAsset.h"

#include "Equipment/CraftingMaterialDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
    // Weapon tier-up cost to REACH tier index t: [D,C,B,A,S,S+].
    const int32 WeaponGold[6] = { 0, 60, 120, 220, 380, 600 };
    const int32 WeaponMat [6] = { 0,  4,   8,  14,  22,  32 };
    // Chip / armor level-up cost to REACH level L (2 or 3).
    const int32 LevelGold[4]  = { 0, 0,  80, 160 };
    const int32 LevelMat [4]  = { 0, 0,   6,  12 };
}

// -----------------------------------------------------------------------------
//  Seeding + HP sync
// -----------------------------------------------------------------------------

void URosterSubsystem::SeedFromParty(const TArray<ACombatantBase*>& Party)
{
    if (bSeeded) { return; }

    int32 PartyCount = 0;
    for (ACombatantBase* Base : Party)
    {
        APlayerCombatant* PC = Cast<APlayerCombatant>(Base);
        if (!PC) { continue; }

        FPartyMemberRecord Rec;
        Rec.CharacterClass = PC->GetClass();
        Rec.DisplayName    = PC->DisplayName;
        Rec.Tagline        = PC->Tagline;
        Rec.Level          = PC->Level;
        Rec.MaxHP          = PC->GetMaxHP();
        Rec.CurrentHP      = PC->GetCurrentHP();
        Rec.Attack         = PC->GetStatValue(EUpgradeStat::Attack);
        Rec.Defense        = PC->GetStatValue(EUpgradeStat::Defense);
        Rec.Speed          = PC->GetStatValue(EUpgradeStat::Speed);
        Rec.MainWeapon     = PC->MainWeapon;
        Rec.Gun            = PC->Gun;
        Rec.Armor          = PC->Armor;
        Rec.Chips          = PC->Chips;
        Rec.EquippedSkillCount = PC->GetEquippedSkillCount();
        Rec.Assignment     = (PartyCount < MaxPartySize) ? EPartyAssignment::Party1
                                                         : EPartyAssignment::Bench;
        ++PartyCount;

        // Everything a member starts with is "owned" (switchable).
        AddOwnedWeapon(PC->MainWeapon);
        AddOwnedWeapon(PC->Gun);
        AddOwnedArmor(PC->Armor);
        for (const TObjectPtr<UCharacterChipDataAsset>& Chip : PC->Chips)
        {
            AddOwnedChip(Chip);
        }

        Members.Add(MoveTemp(Rec));
    }

    HealCharges   = MaxHealCharges;
    ReviveCharges = MaxReviveCharges;
    APCharges     = MaxAPCharges;

    bSeeded = Members.Num() > 0;

    UE_LOG(LogTemp, Log, TEXT("[Roster] Seeded %d party member(s)."), Members.Num());
}

void URosterSubsystem::RestoreHPToParty(const TArray<ACombatantBase*>& Party)
{
    for (ACombatantBase* Base : Party)
    {
        const int32 Idx = FindRecordForActor(Base);
        if (Idx == INDEX_NONE) { continue; }

        // Push the record's loadout (which may have been changed via the roster
        // screen while away) onto the actor, then restore saved HP.
        if (APlayerCombatant* PC = Cast<APlayerCombatant>(Base))
        {
            ApplyRecordToActor(Idx, PC);
        }

        const FPartyMemberRecord& Rec = Members[Idx];
        const float Live = Base->GetCurrentHP();
        const float Want = Rec.CurrentHP;

        if (Want > Live)      { Base->RestoreResource(EResourceType::HP, Want - Live); }
        else if (Want < Live) { Base->SpendResource(EResourceType::HP, Live - Want); }
    }
}

APlayerCombatant* URosterSubsystem::FindLiveActor(const FPartyMemberRecord& Rec) const
{
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World || !Rec.CharacterClass) { return nullptr; }

    for (TActorIterator<APlayerCombatant> It(World); It; ++It)
    {
        if (*It && (*It)->GetClass() == Rec.CharacterClass) { return *It; }
    }
    return nullptr;
}

void URosterSubsystem::ApplyRecordToActor(int32 Index, APlayerCombatant* Actor)
{
    if (!Members.IsValidIndex(Index) || !Actor) { return; }
    FPartyMemberRecord& Rec = Members[Index];

    Actor->MainWeapon = Rec.MainWeapon;
    Actor->Gun        = Rec.Gun;
    Actor->Armor      = Rec.Armor;
    Actor->Chips      = Rec.Chips;

    // Re-derive the buffed stats (InitializeForBattle re-applies equipment
    // bonuses; it's HP-persistent so current HP is preserved).
    Actor->InitializeForBattle();

    Rec.MaxHP    = Actor->GetMaxHP();
    Rec.Attack   = Actor->GetStatValue(EUpgradeStat::Attack);
    Rec.Defense  = Actor->GetStatValue(EUpgradeStat::Defense);
    Rec.Speed    = Actor->GetStatValue(EUpgradeStat::Speed);
    Rec.CurrentHP = FMath::Min(Rec.CurrentHP, Rec.MaxHP);
}

// -----------------------------------------------------------------------------
//  Gear switching
// -----------------------------------------------------------------------------

void URosterSubsystem::SetMemberMainWeapon(int32 Index, UCharacterWeaponDataAsset* Weapon)
{
    if (!Members.IsValidIndex(Index)) { return; }
    Members[Index].MainWeapon = Weapon;
    if (APlayerCombatant* Actor = FindLiveActor(Members[Index]))
    {
        ApplyRecordToActor(Index, Actor);   // applies + re-derives stats
    }
}

void URosterSubsystem::SetMemberGun(int32 Index, UCharacterWeaponDataAsset* Gun)
{
    if (!Members.IsValidIndex(Index)) { return; }
    Members[Index].Gun = Gun;
    if (APlayerCombatant* Actor = FindLiveActor(Members[Index]))
    {
        ApplyRecordToActor(Index, Actor);
    }
}

void URosterSubsystem::SetMemberArmor(int32 Index, UCharacterArmorDataAsset* Armor)
{
    if (!Members.IsValidIndex(Index)) { return; }
    Members[Index].Armor = Armor;
    if (APlayerCombatant* Actor = FindLiveActor(Members[Index]))
    {
        ApplyRecordToActor(Index, Actor);
    }
}

void URosterSubsystem::SetMemberChip(int32 Index, int32 ChipSlot, UCharacterChipDataAsset* Chip)
{
    if (!Members.IsValidIndex(Index) || ChipSlot < 0) { return; }

    FPartyMemberRecord& Rec = Members[Index];
    if (!Rec.Chips.IsValidIndex(ChipSlot))
    {
        Rec.Chips.SetNum(ChipSlot + 1);   // pad with nulls up to this slot
    }
    Rec.Chips[ChipSlot] = Chip;

    if (APlayerCombatant* Actor = FindLiveActor(Rec))
    {
        ApplyRecordToActor(Index, Actor);
    }
}

void URosterSubsystem::SetMemberArmorChip(int32 Index, UCharacterChipDataAsset* Chip)
{
    if (!Members.IsValidIndex(Index)) { return; }
    FPartyMemberRecord& Rec = Members[Index];
    if (!Rec.Armor) { return; }                 // nothing to socket into

    Rec.Armor->SocketedChip = Chip;             // the chip rides with the armor asset

    if (APlayerCombatant* Actor = FindLiveActor(Rec))
    {
        ApplyRecordToActor(Index, Actor);
    }
}

void URosterSubsystem::SaveHPFromParty(const TArray<ACombatantBase*>& Party)
{
    for (ACombatantBase* Base : Party)
    {
        const int32 Idx = FindRecordForActor(Base);
        if (Idx == INDEX_NONE) { continue; }

        Members[Idx].CurrentHP = Base->GetCurrentHP();
        Members[Idx].MaxHP     = Base->GetMaxHP();
    }
}

int32 URosterSubsystem::FindRecordForActor(const ACombatantBase* Actor) const
{
    if (!Actor) { return INDEX_NONE; }
    const UClass* Cls = Actor->GetClass();
    for (int32 i = 0; i < Members.Num(); ++i)
    {
        if (Members[i].CharacterClass == Cls) { return i; }
    }
    return INDEX_NONE;
}

// -----------------------------------------------------------------------------
//  Party assignment
// -----------------------------------------------------------------------------

EPartyAssignment URosterSubsystem::GetAssignment(int32 Index) const
{
    return Members.IsValidIndex(Index) ? Members[Index].Assignment : EPartyAssignment::Bench;
}

bool URosterSubsystem::SetAssignment(int32 Index, EPartyAssignment NewAssignment)
{
    if (!Members.IsValidIndex(Index)) { return false; }
    if (Members[Index].Assignment == NewAssignment) { return true; }

    if (NewAssignment != EPartyAssignment::Bench &&
        CountInParty(NewAssignment) >= MaxPartySize)
    {
        return false;   // party full
    }

    Members[Index].Assignment = NewAssignment;
    return true;
}

int32 URosterSubsystem::CountInParty(EPartyAssignment Party) const
{
    int32 Count = 0;
    for (const FPartyMemberRecord& Rec : Members)
    {
        if (Rec.Assignment == Party) { ++Count; }
    }
    return Count;
}

// -----------------------------------------------------------------------------
//  Charges + healing
// -----------------------------------------------------------------------------

void URosterSubsystem::RestockAndHeal(const TArray<ACombatantBase*>& LiveParty)
{
    HealCharges   = MaxHealCharges;
    ReviveCharges = MaxReviveCharges;
    APCharges     = MaxAPCharges;

    // Full-heal + revive every record.
    for (FPartyMemberRecord& Rec : Members)
    {
        Rec.CurrentHP = Rec.MaxHP;
    }

    // Mirror onto any live actors present in this level.
    for (ACombatantBase* Base : LiveParty)
    {
        if (!Base) { continue; }
        if (Base->GetCurrentHP() <= 0.f) { Base->Revive(1.f); }
        const float Missing = Base->GetMissingHP();
        if (Missing > 0.f) { Base->RestoreResource(EResourceType::HP, Missing); }
    }
}

bool URosterSubsystem::UseHealingCharge(const TArray<ACombatantBase*>& LiveParty)
{
    if (HealCharges <= 0) { return false; }

    // Only spend if at least one LIVING member is below max.
    bool bAnyHealable = false;
    for (const FPartyMemberRecord& Rec : Members)
    {
        if (!Rec.IsDead() && Rec.CurrentHP < Rec.MaxHP) { bAnyHealable = true; break; }
    }
    if (!bAnyHealable) { return false; }

    --HealCharges;

    // Heal living records to full.
    for (FPartyMemberRecord& Rec : Members)
    {
        if (!Rec.IsDead()) { Rec.CurrentHP = Rec.MaxHP; }
    }

    // Mirror onto living live actors.
    for (ACombatantBase* Base : LiveParty)
    {
        if (!Base || Base->GetCurrentHP() <= 0.f) { continue; }
        const float Missing = Base->GetMissingHP();
        if (Missing > 0.f) { Base->RestoreResource(EResourceType::HP, Missing); }
    }

    UE_LOG(LogTemp, Log, TEXT("[Roster] Spent heal charge (%d left)."), HealCharges);
    return true;
}

// -----------------------------------------------------------------------------
//  Owned inventory
// -----------------------------------------------------------------------------

void URosterSubsystem::AddOwnedWeapon(UCharacterWeaponDataAsset* W)
{
    if (W) { OwnedWeapons.AddUnique(W); }
}
void URosterSubsystem::AddOwnedArmor(UCharacterArmorDataAsset* A)
{
    if (A) { OwnedArmors.AddUnique(A); }
}
void URosterSubsystem::AddOwnedChip(UCharacterChipDataAsset* C)
{
    if (C) { OwnedChips.AddUnique(C); }
}

bool URosterSubsystem::OwnsWeapon(UCharacterWeaponDataAsset* W) const
{
    return W && OwnedWeapons.Contains(W);
}
bool URosterSubsystem::OwnsArmor(UCharacterArmorDataAsset* A) const
{
    return A && OwnedArmors.Contains(A);
}
bool URosterSubsystem::OwnsChip(UCharacterChipDataAsset* C) const
{
    return C && OwnedChips.Contains(C);
}

// -----------------------------------------------------------------------------
//  Battle-entry snapshot + last rested checkpoint
// -----------------------------------------------------------------------------

void URosterSubsystem::SnapshotBattleEntry(const TArray<ACombatantBase*>& LiveParty)
{
    // Make sure the records reflect the live party's current HP first.
    SaveHPFromParty(LiveParty);

    SnapshotHP.Reset();
    SnapshotHP.Reserve(Members.Num());
    for (const FPartyMemberRecord& Rec : Members)
    {
        SnapshotHP.Add(Rec.CurrentHP);
    }

    SnapshotHeal   = HealCharges;
    SnapshotRevive = ReviveCharges;
    SnapshotAP     = APCharges;

    bHasBattleSnapshot = true;
}

bool URosterSubsystem::RestoreBattleEntry(const TArray<ACombatantBase*>& LiveParty)
{
    if (!bHasBattleSnapshot) { return false; }

    for (int32 i = 0; i < Members.Num(); ++i)
    {
        if (SnapshotHP.IsValidIndex(i))
        {
            Members[i].CurrentHP = FMath::Clamp(SnapshotHP[i], 0.f, Members[i].MaxHP);
        }
    }

    HealCharges   = SnapshotHeal;
    ReviveCharges = SnapshotRevive;
    APCharges     = SnapshotAP;

    // Push restored HP onto any live actors (revives downed members so the
    // retry starts them exactly as they entered).
    for (ACombatantBase* Base : LiveParty)
    {
        const int32 Idx = FindRecordForActor(Base);
        if (Idx == INDEX_NONE || !Base) { continue; }

        const float Want = Members[Idx].CurrentHP;
        if (Base->GetCurrentHP() <= 0.f && Want > 0.f)
        {
            Base->Revive(FMath::Clamp(Want / FMath::Max(1.f, Base->GetMaxHP()), 0.01f, 1.f));
        }
        const float Live = Base->GetCurrentHP();
        if (Want > Live)      { Base->RestoreResource(EResourceType::HP, Want - Live); }
        else if (Want < Live) { Base->SpendResource(EResourceType::HP, Live - Want); }
    }
    return true;
}

void URosterSubsystem::SetLastRestedCheckpoint(FName LevelName, FName CheckpointId, const FTransform& Where)
{
    LastRestedLevel        = LevelName;
    LastRestedCheckpointId = CheckpointId;
    LastRestedTransform    = Where;
    bHasLastRested         = true;
}

void URosterSubsystem::GetAvailableWeapons(UClass* CharacterClass, bool bGun,
                                           TArray<UCharacterWeaponDataAsset*>& Out) const
{
    Out.Reset();
    for (const TObjectPtr<UCharacterWeaponDataAsset>& W : OwnedWeapons)
    {
        if (!W || W->bIsGun != bGun) { continue; }
        if (W->OwnerCharacterClass && CharacterClass &&
            !CharacterClass->IsChildOf(W->OwnerCharacterClass))
        {
            continue;
        }
        Out.Add(W);
    }
}

void URosterSubsystem::GetAvailableMainWeapons(UClass* CharacterClass, TArray<UCharacterWeaponDataAsset*>& Out) const
{
    GetAvailableWeapons(CharacterClass, /*bGun=*/false, Out);
}

void URosterSubsystem::GetAvailableGuns(UClass* CharacterClass, TArray<UCharacterWeaponDataAsset*>& Out) const
{
    GetAvailableWeapons(CharacterClass, /*bGun=*/true, Out);
}

void URosterSubsystem::GetAvailableArmors(TArray<UCharacterArmorDataAsset*>& Out) const
{
    Out.Reset();
    for (const TObjectPtr<UCharacterArmorDataAsset>& A : OwnedArmors) { if (A) Out.Add(A); }
}

void URosterSubsystem::GetAvailableChips(TArray<UCharacterChipDataAsset*>& Out, bool bArmorChips) const
{
    Out.Reset();
    for (const TObjectPtr<UCharacterChipDataAsset>& C : OwnedChips)
    {
        if (C && C->bIsArmorChip == bArmorChips) { Out.Add(C); }
    }
}

// -----------------------------------------------------------------------------
//  Economy
// -----------------------------------------------------------------------------

void URosterSubsystem::SetPrimaryMaterial(UCraftingMaterialDataAsset* Material)
{
    PrimaryMaterial = Material;
    if (Material && !Materials.Contains(Material)) { Materials.Add(Material, 0); }
}

void URosterSubsystem::AddMaterial(UCraftingMaterialDataAsset* Material, int32 Amount)
{
    if (!Material || Amount == 0) { return; }
    int32& Count = Materials.FindOrAdd(Material);
    Count = FMath::Max(0, Count + Amount);
    if (!PrimaryMaterial) { PrimaryMaterial = Material; }
}

int32 URosterSubsystem::GetMaterialCount(UCraftingMaterialDataAsset* Material) const
{
    if (!Material) { return 0; }
    const int32* Found = Materials.Find(Material);
    return Found ? *Found : 0;
}

void URosterSubsystem::RefreshAllMembers()
{
    for (int32 i = 0; i < Members.Num(); ++i)
    {
        if (APlayerCombatant* Actor = FindLiveActor(Members[i]))
        {
            ApplyRecordToActor(i, Actor);
        }
    }
}

// -----------------------------------------------------------------------------
//  Upgrades
// -----------------------------------------------------------------------------

bool URosterSubsystem::GetWeaponUpgradeInfo(UCharacterWeaponDataAsset* W, int32& OutGold, int32& OutMaterial, bool& bOutMaxed) const
{
    OutGold = OutMaterial = 0;
    bOutMaxed = false;
    if (!W) { return false; }

    const int32 Cur = static_cast<int32>(W->CurrentTier);
    const int32 Max = W->bIsGun ? 4 : 5;   // gun caps at S, main at S+
    if (Cur >= Max) { bOutMaxed = true; return true; }

    const int32 Target = Cur + 1;
    OutGold     = WeaponGold[Target];
    OutMaterial = WeaponMat[Target];
    return true;
}

bool URosterSubsystem::TryUpgradeWeapon(UCharacterWeaponDataAsset* W)
{
    int32 NeedGold, NeedMat; bool bMaxed;
    if (!GetWeaponUpgradeInfo(W, NeedGold, NeedMat, bMaxed) || bMaxed) { return false; }
    if (Gold < NeedGold || GetMaterialCount(PrimaryMaterial) < NeedMat) { return false; }

    Gold -= NeedGold;
    if (PrimaryMaterial) { Materials.FindOrAdd(PrimaryMaterial) -= NeedMat; }
    W->CurrentTier = static_cast<EWeaponTier>(static_cast<int32>(W->CurrentTier) + 1);
    RefreshAllMembers();
    return true;
}

bool URosterSubsystem::GetChipUpgradeInfo(UCharacterChipDataAsset* C, int32& OutGold, int32& OutMaterial, bool& bOutMaxed) const
{
    OutGold = OutMaterial = 0;
    bOutMaxed = false;
    if (!C) { return false; }
    if (C->CurrentLevel >= 3) { bOutMaxed = true; return true; }

    const int32 Target = C->CurrentLevel + 1;
    OutGold     = LevelGold[Target];
    OutMaterial = LevelMat[Target];
    return true;
}

bool URosterSubsystem::TryUpgradeChip(UCharacterChipDataAsset* C)
{
    int32 NeedGold, NeedMat; bool bMaxed;
    if (!GetChipUpgradeInfo(C, NeedGold, NeedMat, bMaxed) || bMaxed) { return false; }
    if (Gold < NeedGold || GetMaterialCount(PrimaryMaterial) < NeedMat) { return false; }

    Gold -= NeedGold;
    if (PrimaryMaterial) { Materials.FindOrAdd(PrimaryMaterial) -= NeedMat; }
    ++C->CurrentLevel;
    RefreshAllMembers();
    return true;
}

bool URosterSubsystem::GetArmorUpgradeInfo(UCharacterArmorDataAsset* A, int32& OutGold, int32& OutMaterial, bool& bOutMaxed) const
{
    OutGold = OutMaterial = 0;
    bOutMaxed = false;
    if (!A) { return false; }
    if (A->CurrentLevel >= 3) { bOutMaxed = true; return true; }

    const int32 Target = A->CurrentLevel + 1;
    OutGold     = LevelGold[Target];
    OutMaterial = LevelMat[Target];
    return true;
}

bool URosterSubsystem::TryUpgradeArmor(UCharacterArmorDataAsset* A)
{
    int32 NeedGold, NeedMat; bool bMaxed;
    if (!GetArmorUpgradeInfo(A, NeedGold, NeedMat, bMaxed) || bMaxed) { return false; }
    if (Gold < NeedGold || GetMaterialCount(PrimaryMaterial) < NeedMat) { return false; }

    Gold -= NeedGold;
    if (PrimaryMaterial) { Materials.FindOrAdd(PrimaryMaterial) -= NeedMat; }
    ++A->CurrentLevel;
    RefreshAllMembers();
    return true;
}
