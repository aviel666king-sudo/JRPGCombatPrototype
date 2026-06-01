#include "Roster/RosterSubsystem.h"
#include "Characters/Player/PlayerCombatant.h"
#include "Characters/Base/CombatantBase.h"
#include "Equipment/CharacterWeaponDataAsset.h"
#include "Equipment/CharacterArmorDataAsset.h"
#include "Equipment/CharacterChipDataAsset.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/ARFilter.h"

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

        const FPartyMemberRecord& Rec = Members[Idx];
        const float Live = Base->GetCurrentHP();
        const float Want = Rec.CurrentHP;

        if (Want > Live)      { Base->RestoreResource(EResourceType::HP, Want - Live); }
        else if (Want < Live) { Base->SpendResource(EResourceType::HP, Live - Want); }
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
//  Inventory discovery
// -----------------------------------------------------------------------------

template <typename T>
void URosterSubsystem::LoadAllAssetsOfClass(TArray<T*>& Out) const
{
    Out.Reset();
    FAssetRegistryModule& ARM =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    FARFilter Filter;
    Filter.ClassPaths.Add(T::StaticClass()->GetClassPathName());
    Filter.bRecursiveClasses = true;

    TArray<FAssetData> Assets;
    ARM.Get().GetAssets(Filter, Assets);

    for (const FAssetData& Data : Assets)
    {
        if (T* Loaded = Cast<T>(Data.GetAsset())) { Out.Add(Loaded); }
    }
}

void URosterSubsystem::GetAvailableWeapons(UClass* CharacterClass, bool bGun,
                                           TArray<UCharacterWeaponDataAsset*>& Out) const
{
    Out.Reset();
    TArray<UCharacterWeaponDataAsset*> All;
    LoadAllAssetsOfClass<UCharacterWeaponDataAsset>(All);

    for (UCharacterWeaponDataAsset* W : All)
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
    LoadAllAssetsOfClass<UCharacterArmorDataAsset>(Out);
}

void URosterSubsystem::GetAvailableChips(TArray<UCharacterChipDataAsset*>& Out) const
{
    LoadAllAssetsOfClass<UCharacterChipDataAsset>(Out);
}
