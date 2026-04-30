// Full replacement — adds Revive() while keeping everything else identical.
// Only the Revive() function is new; all other implementations are unchanged.

#include "Characters/Base/CombatantBase.h"
#include "Characters/Player/PlayerCombatant.h"
#include "Components/AbilityManagerComponent.h"
#include "Components/StatusEffectManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"

ACombatantBase::ACombatantBase()
{
    PrimaryActorTick.bCanEverTick = false;

    // -------------------------------------------------------------------------
    //  Visual setup
    // -------------------------------------------------------------------------

    CapsuleComponent = CreateDefaultSubobject<UCapsuleComponent>(TEXT("CapsuleComponent"));
    CapsuleComponent->SetCapsuleHalfHeight(90.0f);
    CapsuleComponent->SetCapsuleRadius(30.0f);
    // QueryOnly so gun aim line traces can hit combatants without
    // interfering with physics/movement (none needed for turn-based).
    CapsuleComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CapsuleComponent->SetCollisionObjectType(ECC_Pawn);
    CapsuleComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
    CapsuleComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    RootComponent = CapsuleComponent;

    Mesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Mesh"));
    Mesh->SetupAttachment(CapsuleComponent);
    // Standard UE5 mannequin offset: mesh base sits at capsule bottom, facing forward
    Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
    Mesh->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // ── Shared anim blueprint (same for all combatants) ───────────────────────
    {
        static ConstructorHelpers::FClassFinder<UAnimInstance> AnimBP(
            TEXT("/Game/Characters/Mannequins/Anims/ABP_CombatCharacter.ABP_CombatCharacter_C"));
        if (AnimBP.Succeeded()) { Mesh->SetAnimInstanceClass(AnimBP.Class); }
    }

    // ── Shared montages (same for all combatants) ─────────────────────────────
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> Attack(
            TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/AM_Attack.AM_Attack"));
        if (Attack.Succeeded()) { AttackMontage = Attack.Object; }
    }
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> Cast(
            TEXT("/Game/Characters/Mannequins/Anims/Unarmed/Attack/AM_Cast.AM_Cast"));
        if (Cast.Succeeded()) { CastMontage = Cast.Object; }
    }
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> HitReact(
            TEXT("/Game/Characters/Mannequins/Anims/Rifle/HitReact/AM_HitReact.AM_HitReact"));
        if (HitReact.Succeeded()) { HitReactMontage = HitReact.Object; }
    }
    {
        static ConstructorHelpers::FObjectFinder<UAnimMontage> Death(
            TEXT("/Game/Characters/Mannequins/Anims/Death/AM_Death.AM_Death"));
        if (Death.Succeeded()) { DeathMontage = Death.Object; }
    }

    // -------------------------------------------------------------------------
    //  Combat components
    // -------------------------------------------------------------------------

    AbilityManager      = CreateDefaultSubobject<UAbilityManagerComponent>(TEXT("AbilityManager"));
    StatusEffectManager = CreateDefaultSubobject<UStatusEffectManagerComponent>(TEXT("StatusEffectManager"));
}

void ACombatantBase::BeginPlay()
{
    Super::BeginPlay();
}

// -----------------------------------------------------------------------------
//  Battle setup
// -----------------------------------------------------------------------------

void ACombatantBase::InitializeForBattle()
{
    Resources.Reset();

    Resources.Add(EResourceType::HP, FResourcePool(BaseStats.MaxHP));

    {
        FResourcePool APPool;
        APPool.Max     = BaseStats.MaxAP;
        APPool.Current = FMath::Clamp(BaseStats.StartingAP, 0.f, BaseStats.MaxAP);
        Resources.Add(EResourceType::AP, APPool);
    }

    Resources.Add(EResourceType::MP, FResourcePool(0.f));

    AbilityManager->InitializeAbilities(this);
    StatusEffectManager->ClearAllEffects();
}

// -----------------------------------------------------------------------------
//  Turn callbacks
// -----------------------------------------------------------------------------

void ACombatantBase::OnTurnStart_Implementation()
{
    StatusEffectManager->NotifyTurnStart();
}

void ACombatantBase::OnTurnEnd_Implementation()
{
    AbilityManager->TickAllCooldowns();
}

// -----------------------------------------------------------------------------
//  Element resistance helpers
// -----------------------------------------------------------------------------

EResistanceType ACombatantBase::GetResistanceType(EElement Element) const
{
    if (Element == EElement::None) { return EResistanceType::Normal; }
    if (const EResistanceType* Found = ElementResistances.Find(Element))
    {
        return *Found;
    }
    return EResistanceType::Normal;
}

float ACombatantBase::GetElementMultiplier(EElement Element) const
{
    switch (GetResistanceType(Element))
    {
        case EResistanceType::Weak:   return 1.5f;
        case EResistanceType::Normal: return 1.0f;
        case EResistanceType::Resist: return 0.5f;
        case EResistanceType::Block:  return 0.0f;
        case EResistanceType::Absorb: return 0.0f; // Handled specially in ApplyDamage.
    }
    return 1.0f;
}

// -----------------------------------------------------------------------------
//  Damage — C++ pipeline
// -----------------------------------------------------------------------------

void ACombatantBase::ApplyDamage(FDamagePayload& Payload)
{
    if (IsDead()) { return; }

    float Damage = Payload.BaseDamage;

    // Outgoing multiplier from attacker (e.g. stance boosts on Fencer).
    if (Payload.Source)
    {
        Damage *= Payload.Source->GetOutgoingDamageMultiplier();
    }

    // Incoming multiplier on this target (e.g. Defensive stance reduction).
    Damage *= GetIncomingDamageMultiplier();

    // Sync payload so status effect hooks can read and modify the damage.
    Payload.BaseDamage = Damage;

    // Status effect modifiers — hooks receive the full payload.
    if (Payload.Source && Payload.Source->StatusEffectManager)
    {
        Payload.Source->StatusEffectManager->NotifyBeforeDealDamage(Payload);
    }
    StatusEffectManager->NotifyBeforeTakeDamage(Payload);

    // Re-read damage — an effect may have modified Payload.BaseDamage.
    Damage = Payload.BaseDamage;

    // --- Element resistance ---
    // Resolve the target's reaction and store it for UI feedback.
    const EResistanceType Resistance = GetResistanceType(Payload.Element);
    Payload.HitResistance = Resistance;

    if (Resistance == EResistanceType::Absorb)
    {
        // Hit heals the target instead. Skip all damage logic.
        UE_LOG(LogTemp, Log, TEXT("[Element] %s absorbed %s — healed for %.1f."),
            *GetName(), *UEnum::GetValueAsString(Payload.Element), Damage);
        ApplyHealing(Damage, Payload.Source.Get());
        Payload.ResolvedDamage = 0.f;
        if (Payload.Source && Payload.Source->StatusEffectManager)
        {
            Payload.Source->StatusEffectManager->NotifyDealDamage(Payload);
        }
        return;
    }

    Damage *= GetElementMultiplier(Payload.Element);

    if (Resistance != EResistanceType::Normal)
    {
        UE_LOG(LogTemp, Log, TEXT("[Element] %s hit %s with %s — %s (x%.2f) → %.1f"),
            Payload.Source ? *Payload.Source->GetName() : TEXT("?"),
            *GetName(),
            *UEnum::GetValueAsString(Payload.Element),
            *UEnum::GetValueAsString(Resistance),
            GetElementMultiplier(Payload.Element),
            Damage);
    }

    // Defense subtraction (non-true damage only).
    if (Payload.DamageType != EDamageType::TrueDamage)
    {
        Damage = FMath::Max(0.f, Damage - BaseStats.Defense);
    }

    Damage = FMath::Max(0.f, Damage);
    Payload.ResolvedDamage = Damage;

    SpendResource(EResourceType::HP, Damage);

    BP_OnDamageTaken(Payload.Source, Damage, Payload.DamageType);

    // Play hit react or death animation automatically on every hit.
    PlayReactionAnimation();

    StatusEffectManager->NotifyAfterTakeDamage(Payload);
    if (Payload.Source && Payload.Source->StatusEffectManager)
    {
        Payload.Source->StatusEffectManager->NotifyDealDamage(Payload);
    }
}

// -----------------------------------------------------------------------------
//  Damage — Blueprint API
// -----------------------------------------------------------------------------

void ACombatantBase::BP_ApplyDamage(ACombatantBase* Source, float BaseDamage, EDamageType DamageType)
{
    FDamagePayload Payload;
    Payload.Source     = Source;
    Payload.BaseDamage = BaseDamage;
    Payload.DamageType = DamageType;
    ApplyDamage(Payload);
}

// -----------------------------------------------------------------------------
//  Healing — C++ pipeline
// -----------------------------------------------------------------------------

void ACombatantBase::ApplyHealing(float Amount, ACombatantBase* Source)
{
    // Hunger effect blocks healing.
    if (StatusEffectManager && !StatusEffectManager->CanReceiveHealing()) return;

    if (Source && Source->StatusEffectManager)
    {
        Source->StatusEffectManager->NotifyBeforeDealHealing(Amount);
    }

    StatusEffectManager->NotifyBeforeReceiveHealing(Amount);

    const float Clamped = FMath::Max(0.f, Amount);
    RestoreResource(EResourceType::HP, Clamped);

    BP_OnHealingReceived(Source, Clamped);

    StatusEffectManager->NotifyAfterReceiveHealing(Clamped);

    if (Source && Source->StatusEffectManager)
    {
        Source->StatusEffectManager->NotifyAfterDealHealing(Clamped);
    }
}

void ACombatantBase::BP_ApplyHealing(ACombatantBase* Source, float Amount)
{
    ApplyHealing(Amount, Source);
}

// -----------------------------------------------------------------------------
//  NEW: Revival
// -----------------------------------------------------------------------------

void ACombatantBase::Revive(float HPPercent)
{
    // Only revive if actually dead.
    if (!IsDead()) { return; }

    const float RestoreAmount = GetMaxHP() * FMath::Clamp(HPPercent, 0.f, 1.f);

    // Set HP directly — bypass the healing pipeline (no effect notifications,
    // no "deal healing" delegate) so that revival is clean and not inflated by
    // any healing-amplification status effects that may be active.
    if (FResourcePool* Pool = Resources.Find(EResourceType::HP))
    {
        Pool->Current = FMath::Clamp(RestoreAmount, 1.f, Pool->Max);
    }

    UE_LOG(LogTemp, Log, TEXT("[CombatantBase] %s revived with %.1f HP (%.0f%% of max)."),
        *GetName(), RestoreAmount, HPPercent * 100.f);
}

// -----------------------------------------------------------------------------
//  Resources
// -----------------------------------------------------------------------------

void ACombatantBase::SpendResource(EResourceType Type, float Amount)
{
    if (FResourcePool* Pool = Resources.Find(Type))
    {
        Pool->Current = FMath::Clamp(Pool->Current - Amount, 0.f, Pool->Max);
    }
}

void ACombatantBase::RestoreResource(EResourceType Type, float Amount)
{
    // Despair effect blocks AP gain.
    if (Type == EResourceType::AP && StatusEffectManager && !StatusEffectManager->CanGainAP())
        return;

    if (FResourcePool* Pool = Resources.Find(Type))
    {
        Pool->Current = FMath::Clamp(Pool->Current + Amount, 0.f, Pool->Max);
    }
}

bool ACombatantBase::CanAffordCost(const FAbilityCost& Cost) const
{
    return GetCurrentResource(Cost.ResourceType) >= Cost.Amount;
}

float ACombatantBase::GetCurrentResource(EResourceType Type) const
{
    if (const FResourcePool* Pool = Resources.Find(Type))
    {
        return Pool->Current;
    }
    return 0.f;
}

float ACombatantBase::GetMaxResource(EResourceType Type) const
{
    if (const FResourcePool* Pool = Resources.Find(Type))
    {
        return Pool->Max;
    }
    return 0.f;
}

float ACombatantBase::GetHealthPercent() const
{
    const float Max = GetMaxHP();
    return Max > 0.f ? GetCurrentHP() / Max : 0.f;
}

float ACombatantBase::GetAPPercent() const
{
    const float Max = GetMaxAP();
    return Max > 0.f ? GetCurrentAP() / Max : 0.f;
}

// -----------------------------------------------------------------------------
//  State queries
// -----------------------------------------------------------------------------

bool ACombatantBase::IsDead() const
{
    const FResourcePool* Pool = Resources.Find(EResourceType::HP);
    if (!Pool) { return false; }
    return Pool->Current <= 0.f;
}

float ACombatantBase::GetEffectiveSpeed() const
{
    return BaseStats.Speed * StatusEffectManager->GetSpeedMultiplier();
}

// -----------------------------------------------------------------------------
//  Animation
// -----------------------------------------------------------------------------

void ACombatantBase::PlayMontage(UAnimMontage* Montage)
{
    if (!Montage || !Mesh) { return; }
    UAnimInstance* AnimInst = Mesh->GetAnimInstance();
    if (!AnimInst) { return; }
    AnimInst->Montage_Play(Montage);
}

void ACombatantBase::PlayAbilityAnimation(EAbilityCategory Category)
{
    switch (Category)
    {
    case EAbilityCategory::Melee:
        PlayMontage(AttackMontage);
        break;
    case EAbilityCategory::Gun:
        if (APlayerCombatant* PC = Cast<APlayerCombatant>(this))
            PlayMontage(PC->GunMontage);
        break;
    case EAbilityCategory::Skill:
        PlayMontage(CastMontage);
        break;
    default:
        PlayMontage(AttackMontage);
        break;
    }
}

void ACombatantBase::PlayReactionAnimation()
{
    if (IsDead())
        PlayMontage(DeathMontage);
    else
        PlayMontage(HitReactMontage);
}
