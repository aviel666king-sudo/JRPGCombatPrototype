#include "Characters/Player/PlayerCombatant.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

APlayerCombatant::APlayerCombatant()
{
    // ── Player mesh: Manny ────────────────────────────────────────────────────
    // Set in C++ so the assignment survives C++ recompiles without needing
    // manual Blueprint reassignment after every hot-reload.
    {
        static ConstructorHelpers::FObjectFinder<USkeletalMesh> MannySK(
            TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
        if (MannySK.Succeeded() && Mesh) { Mesh->SetSkeletalMeshAsset(MannySK.Object); }
    }

    // ── Player-exclusive montages ─────────────────────────────────────────────
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> GunMontageFinder(
            TEXT("/Game/Characters/Mannequins/Anims/Pistol/AM_Gun.AM_Gun"));
        if (GunMontageFinder.Succeeded()) { GunMontage = GunMontageFinder.Object; }
    }
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> Parry(
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/AM_Parry.AM_Parry"));
        if (Parry.Succeeded()) { ParryMontage = Parry.Object; }
    }
}

// -----------------------------------------------------------------------------
//  Progression
// -----------------------------------------------------------------------------

int32 APlayerCombatant::GetXPForNextLevel() const
{
    return 100 + 50 * (Level - 1);
}

void APlayerCombatant::GrantXP(int32 Amount)
{
    if (Amount <= 0) { return; }

    CurrentXP += Amount;

    const FString Name = DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
    UE_LOG(LogTemp, Log, TEXT("[XP] %s +%d XP (now %d / %d)"),
        *Name, Amount, CurrentXP, GetXPForNextLevel());

    // Loop multi-level handles big rewards that cover several levels at once.
    while (CurrentXP >= GetXPForNextLevel())
    {
        CurrentXP -= GetXPForNextLevel();
        LevelUp();
    }
}

void APlayerCombatant::LevelUp()
{
    ++Level;

    // Flat stat growth applied to BaseStats. Next InitializeForBattle picks
    // up the new MaxHP (see ACombatantBase::InitializeForBattle — it bumps
    // HP.Max when BaseStats.MaxHP changes).
    BaseStats.MaxHP   += 5.f;
    BaseStats.Attack  += 1.f;
    BaseStats.Defense += 1.f;
    BaseStats.Speed   += 1.f;

    // Currency banked for future skill-tree / stat-shop systems.
    SkillCoins += 2;
    StatCoins  += 3;

    const FString Name = DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
    UE_LOG(LogTemp, Warning,
        TEXT("[XP] %s LEVELED UP to %d! (+5 MaxHP, +1 Atk/Def/Spd, +2 Skill, +3 Stat coins)"),
        *Name, Level);
}

// -----------------------------------------------------------------------------
//  Stat shop
// -----------------------------------------------------------------------------

int32 APlayerCombatant::GetUpgradeCost(EUpgradeStat /*Stat*/) const
{
    return 1;   // flat cost for now; tune per-stat here later
}

float APlayerCombatant::GetUpgradeAmount(EUpgradeStat Stat) const
{
    switch (Stat)
    {
        case EUpgradeStat::MaxHP:   return 10.f;
        case EUpgradeStat::Attack:  return 2.f;
        case EUpgradeStat::Defense: return 2.f;
        case EUpgradeStat::Speed:   return 1.f;
        default:                    return 0.f;
    }
}

float APlayerCombatant::GetStatValue(EUpgradeStat Stat) const
{
    switch (Stat)
    {
        case EUpgradeStat::MaxHP:   return BaseStats.MaxHP;
        case EUpgradeStat::Attack:  return BaseStats.Attack;
        case EUpgradeStat::Defense: return BaseStats.Defense;
        case EUpgradeStat::Speed:   return BaseStats.Speed;
        default:                    return 0.f;
    }
}

bool APlayerCombatant::TryUpgradeStat(EUpgradeStat Stat)
{
    const int32 Cost = GetUpgradeCost(Stat);
    if (StatCoins < Cost) { return false; }

    StatCoins -= Cost;
    const float Amount = GetUpgradeAmount(Stat);

    switch (Stat)
    {
        case EUpgradeStat::MaxHP:   BaseStats.MaxHP   += Amount; break;
        case EUpgradeStat::Attack:  BaseStats.Attack  += Amount; break;
        case EUpgradeStat::Defense: BaseStats.Defense += Amount; break;
        case EUpgradeStat::Speed:   BaseStats.Speed   += Amount; break;
        default: break;
    }

    // Re-apply BaseStats so a MaxHP raise lifts the live HP cap immediately
    // (InitializeForBattle is HP-persistent; current HP carries over).
    if (Stat == EUpgradeStat::MaxHP)
    {
        InitializeForBattle();
    }

    return true;
}
