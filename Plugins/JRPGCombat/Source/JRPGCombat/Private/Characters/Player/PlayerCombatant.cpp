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
