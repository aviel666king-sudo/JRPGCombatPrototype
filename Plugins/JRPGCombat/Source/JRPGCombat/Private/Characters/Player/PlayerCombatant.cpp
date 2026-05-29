#include "Characters/Player/PlayerCombatant.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"
#include "Progression/SkillTreeDataAsset.h"
#include "Abilities/CombatAbility.h"

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

void APlayerCombatant::DebugLevelUp()
{
    LevelUp();
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

// -----------------------------------------------------------------------------
//  Skill tree
// -----------------------------------------------------------------------------

void APlayerCombatant::BeginPlay()
{
    Super::BeginPlay();

    // If no tree asset was assigned in the editor, build the character's
    // default tree in C++ (subclasses override PopulateDefaultSkillTree).
    if (!SkillTree)
    {
        SkillTree = NewObject<USkillTreeDataAsset>(this);
        PopulateDefaultSkillTree(SkillTree);
    }

    // Grant the character's starting skills (unlocked + auto-equipped).
    TArray<FName> StartingNodes;
    GetStartingSkillNodes(StartingNodes);
    for (const FName& NodeId : StartingNodes)
    {
        if (SkillTree && SkillTree->FindNode(NodeId))
        {
            UnlockedNodes.Add(NodeId);
            if (CanEquipMore()) { EquippedNodes.AddUnique(NodeId); }
        }
    }
}

bool APlayerCombatant::ArePrerequisitesMet(FName NodeId) const
{
    if (!SkillTree) { return false; }
    const FSkillNode* Node = SkillTree->FindNode(NodeId);
    if (!Node) { return false; }

    for (const FName& Prereq : Node->Prerequisites)
    {
        if (!UnlockedNodes.Contains(Prereq)) { return false; }
    }
    return true;
}

bool APlayerCombatant::CanUnlockNode(FName NodeId) const
{
    if (!SkillTree) { return false; }
    const FSkillNode* Node = SkillTree->FindNode(NodeId);
    if (!Node) { return false; }
    if (UnlockedNodes.Contains(NodeId)) { return false; }   // already owned
    if (!ArePrerequisitesMet(NodeId)) { return false; }
    return SkillCoins >= Node->SkillCoinCost;
}

bool APlayerCombatant::TryUnlockNode(FName NodeId)
{
    if (!CanUnlockNode(NodeId)) { return false; }

    const FSkillNode* Node = SkillTree->FindNode(NodeId);   // non-null per CanUnlockNode
    SkillCoins -= Node->SkillCoinCost;
    UnlockedNodes.Add(NodeId);

    // Convenience: auto-equip a freshly unlocked skill if there's a free slot.
    if (CanEquipMore()) { EquippedNodes.AddUnique(NodeId); }

    const FString Name = DisplayName.IsEmpty() ? GetName() : DisplayName.ToString();
    UE_LOG(LogTemp, Log, TEXT("[SkillTree] %s unlocked node '%s' (%d SkillCoins left)"),
        *Name, *NodeId.ToString(), SkillCoins);
    return true;
}

void APlayerCombatant::GetUnlockedAbilityClasses(TArray<TSubclassOf<UCombatAbility>>& Out) const
{
    if (!SkillTree) { return; }
    for (const FSkillNode& Node : SkillTree->Nodes)
    {
        if (UnlockedNodes.Contains(Node.NodeId) && Node.AbilityClass)
        {
            Out.AddUnique(Node.AbilityClass);
        }
    }
}

// -----------------------------------------------------------------------------
//  Skill loadout (max MaxEquippedSkills equipped)
// -----------------------------------------------------------------------------

bool APlayerCombatant::TryEquipNode(FName NodeId)
{
    if (!UnlockedNodes.Contains(NodeId)) { return false; }   // can't equip what you don't own
    if (EquippedNodes.Contains(NodeId))  { return true; }    // already equipped
    if (!CanEquipMore())                 { return false; }   // loadout full
    EquippedNodes.Add(NodeId);
    return true;
}

void APlayerCombatant::UnequipNode(FName NodeId)
{
    EquippedNodes.Remove(NodeId);
}

bool APlayerCombatant::ToggleEquipNode(FName NodeId)
{
    if (EquippedNodes.Contains(NodeId))
    {
        EquippedNodes.Remove(NodeId);
        return true;
    }
    return TryEquipNode(NodeId);
}

void APlayerCombatant::GetEquippedAbilityClasses(TArray<TSubclassOf<UCombatAbility>>& Out) const
{
    if (!SkillTree) { return; }
    for (const FName& NodeId : EquippedNodes)
    {
        if (const FSkillNode* Node = SkillTree->FindNode(NodeId))
        {
            if (Node->AbilityClass) { Out.AddUnique(Node->AbilityClass); }
        }
    }
}
