#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

class ACombatantBase;

// -----------------------------------------------------------------------------
//  Enums
// -----------------------------------------------------------------------------

UENUM(BlueprintType)
enum class EBattlePhase : uint8
{
    Idle             UMETA(DisplayName = "Idle"),
    Initialization   UMETA(DisplayName = "Initialization"),
    TurnStart        UMETA(DisplayName = "Turn Start"),
    AwaitingInput    UMETA(DisplayName = "Awaiting Input"),
    SelectingTarget  UMETA(DisplayName = "Selecting Target"),
    ExecutingAction  UMETA(DisplayName = "Executing Action"),
    // Target has been confirmed for a skill with a minigame.
    // BM waits for the panel to finish the minigame and call ExecutePendingSkillAfterMinigame.
    MinigameActive   UMETA(DisplayName = "Minigame Active"),
    TurnEnd          UMETA(DisplayName = "Turn End"),
    Victory          UMETA(DisplayName = "Victory"),
    Defeat           UMETA(DisplayName = "Defeat"),
};

UENUM(BlueprintType)
enum class ECombatTeam : uint8
{
    Player  UMETA(DisplayName = "Player"),
    Enemy   UMETA(DisplayName = "Enemy"),
};

UENUM(BlueprintType)
enum class EDamageType : uint8
{
    Physical   UMETA(DisplayName = "Physical"),
    Magical    UMETA(DisplayName = "Magical"),
    Special    UMETA(DisplayName = "Special"),
    TrueDamage UMETA(DisplayName = "True"),
};

// ---------------------------------------------------------------------------
//  Element — the specific elemental subtype of an attack or ability.
//  Physical subtypes: Pierce, Slash, Smash.
//  Magical: Wind, Fire, Ice, Electric, Nature, Light, Dark.
//  Special: Sacrificial, Virus.
//  None = untyped (bypasses element resistance checks).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EElement : uint8
{
    None        UMETA(DisplayName = "None"),
    // Physical
    Pierce      UMETA(DisplayName = "Pierce"),
    Slash       UMETA(DisplayName = "Slash"),
    Smash       UMETA(DisplayName = "Smash"),
    // Magical
    Wind        UMETA(DisplayName = "Wind"),
    Fire        UMETA(DisplayName = "Fire"),
    Ice         UMETA(DisplayName = "Ice"),
    Electric    UMETA(DisplayName = "Electric"),
    Nature      UMETA(DisplayName = "Nature"),
    Light       UMETA(DisplayName = "Light"),
    Dark        UMETA(DisplayName = "Dark"),
    // Special
    Sacrificial UMETA(DisplayName = "Sacrificial"),
    Virus       UMETA(DisplayName = "Virus"),
};

// ---------------------------------------------------------------------------
//  Resistance type — how a combatant reacts to a given element.
//  Normal  = 1.0x damage.
//  Weak    = 1.5x damage.
//  Resist  = 0.5x damage.
//  Block   = 0.0x damage (fully negated).
//  Absorb  = heals the target instead of damaging them (1.0x as healing).
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EResistanceType : uint8
{
    Normal  UMETA(DisplayName = "Normal"),
    Weak    UMETA(DisplayName = "Weak"),
    Resist  UMETA(DisplayName = "Resist"),
    Block   UMETA(DisplayName = "Block"),
    Absorb  UMETA(DisplayName = "Absorb"),
};

UENUM(BlueprintType)
enum class EResourceType : uint8
{
    HP UMETA(DisplayName = "HP"),
    AP UMETA(DisplayName = "AP"),
    MP UMETA(DisplayName = "MP"),
};

UENUM(BlueprintType)
enum class ETargetScope : uint8
{
    Self        UMETA(DisplayName = "Self"),
    SingleEnemy UMETA(DisplayName = "Single Enemy"),
    SingleAlly  UMETA(DisplayName = "Single Ally"),
    AllEnemies  UMETA(DisplayName = "All Enemies"),
    AllAllies   UMETA(DisplayName = "All Allies"),
    // Used by Revival Protocol — targets dead allies only.
    DeadAlly    UMETA(DisplayName = "Dead Ally"),
};

// ---------------------------------------------------------------------------
//  NEW: Ability category — determines which bottom-panel menu slot shows this
//  ability.  Set in each ability's C++ constructor.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EAbilityCategory : uint8
{
    // Appears when the player presses "Melee" — one per character.
    Melee    UMETA(DisplayName = "Melee"),

    // Appears when the player presses "Gun" — one per character.
    Gun      UMETA(DisplayName = "Gun"),

    // Appears in the Skill submenu.
    Skill    UMETA(DisplayName = "Skill"),

    // Internal / uncategorised — does not appear in any player menu.
    None     UMETA(DisplayName = "None"),
};

// ---------------------------------------------------------------------------
//  NEW: Combat HUD menu state — drives which bottom panel is shown.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class ECombatMenuState : uint8
{
    // Enemy turn or no turn — panel faded, all input disabled.
    Inactive        UMETA(DisplayName = "Inactive"),

    // Root panel: Melee / Gun / Skill / Protocol / Skip Turn.
    MainMenu        UMETA(DisplayName = "Main Menu"),

    // Skill submenu: list of Skill-category abilities for the acting character.
    SkillMenu       UMETA(DisplayName = "Skill Menu"),

    // Protocol submenu: Healing / Revival / AP protocols with charge counts.
    ProtocolMenu    UMETA(DisplayName = "Protocol Menu"),

    // Player is navigating target candidates (enemy or ally).
    SelectingTarget UMETA(DisplayName = "Selecting Target"),

    // A skill minigame overlay is active. All panel input is suppressed;
    // the minigame widget owns focus and handles SPACE directly.
    // The panel transitions here from SkillMenu and returns to
    // SelectingTarget (or MainMenu for Self-scope skills) on completion.
    MinigameActive  UMETA(DisplayName = "Minigame Active"),
};

// ---------------------------------------------------------------------------
//  Minigame outcome zones — used internally by DiamondTimingMinigame.
//  Skills read the resulting float multiplier; this enum is for logging /
//  Blueprint inspection only.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EMinigameZone : uint8
{
    // Blue strip inside the red side — 110% damage (damage skills only).
    BlueStrip  UMETA(DisplayName = "Blue Strip"),

    // Red side (outside blue strip) — 100%.
    RedSide    UMETA(DisplayName = "Red Side"),

    // Either side adjacent to the red side — 75% (or 3 turns for support).
    Adjacent   UMETA(DisplayName = "Adjacent"),

    // Side directly opposite the red side — 50% (or 2 turns for support).
    Opposite   UMETA(DisplayName = "Opposite"),

    // Player did not press before rotation completed — 50%.
    NoPress    UMETA(DisplayName = "No Press"),
};

// ---------------------------------------------------------------------------
//  NEW: Which of the three party protocols.
// ---------------------------------------------------------------------------
UENUM(BlueprintType)
enum class EProtocolType : uint8
{
    Healing  UMETA(DisplayName = "Healing Protocol"),
    Revival  UMETA(DisplayName = "Revival Protocol"),
    AP       UMETA(DisplayName = "AP Protocol"),
};

// -----------------------------------------------------------------------------
//  Structs
// -----------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FCombatStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxHP = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float MaxAP = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float StartingAP = 5.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float Attack = 20.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float Defense = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    float Speed = 10.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CritChance = 0.0f;
};

USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FResourcePool
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Resource")
    float Current = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Resource")
    float Max = 0.f;

    FResourcePool() = default;
    explicit FResourcePool(float InMax) : Current(InMax), Max(InMax) {}
    FResourcePool(float InCurrent, float InMax) : Current(InCurrent), Max(InMax) {}
};

USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FAbilityCost
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost")
    EResourceType ResourceType = EResourceType::AP;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cost", meta = (ClampMin = "0"))
    float Amount = 0.f;
};

USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FDamagePayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    TObjectPtr<ACombatantBase> Source;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    float BaseDamage = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    EDamageType DamageType = EDamageType::Physical;

    /** Elemental subtype of this hit. EElement::None bypasses resistance checks. */
    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    EElement Element = EElement::None;

    /** Filled by ApplyDamage — the target's reaction to this element (Weak/Resist/etc.).
     *  Use this in UI to display feedback labels. */
    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    EResistanceType HitResistance = EResistanceType::Normal;

    UPROPERTY(BlueprintReadOnly, Category = "Damage")
    float ResolvedDamage = 0.f;
};

USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FTurnEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Turn")
    TObjectPtr<class ACombatantBase> Combatant = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "Turn")
    float Initiative = 0.f;
};

// ---------------------------------------------------------------------------
//  NEW: Snapshot of one protocol's state passed to the Protocol submenu.
// ---------------------------------------------------------------------------
USTRUCT(BlueprintType)
struct JRPGCOMBAT_API FProtocolInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Protocol")
    EProtocolType Type = EProtocolType::Healing;

    UPROPERTY(BlueprintReadOnly, Category = "Protocol")
    FText DisplayName;

    // Current charges remaining in the shared party pool.
    UPROPERTY(BlueprintReadOnly, Category = "Protocol")
    int32 Charges = 0;

    // Maximum charges this protocol can hold.
    UPROPERTY(BlueprintReadOnly, Category = "Protocol")
    int32 MaxCharges = 0;
};
