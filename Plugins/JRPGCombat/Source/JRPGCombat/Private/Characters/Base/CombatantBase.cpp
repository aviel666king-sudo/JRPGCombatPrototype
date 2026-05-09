// Full replacement — adds Revive() while keeping everything else identical.
// Only the Revive() function is new; all other implementations are unchanged.

#include "Characters/Base/CombatantBase.h"
#include "Characters/Player/PlayerCombatant.h"
#include "Components/AbilityManagerComponent.h"
#include "Components/StatusEffectManagerComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "UObject/ConstructorHelpers.h"
#include "TimerManager.h"

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

    // -------------------------------------------------------------------------
    //  Damage number widget stack — auto-set up for every combatant
    //  Pool of slots stacked vertically so multiple hits can pop at once
    //  (multi-hit attacks, status-tick damage during the same animation, etc.)
    // -------------------------------------------------------------------------
    static ConstructorHelpers::FClassFinder<UUserWidget> DefaultDamageWidget(
        TEXT("/Game/Combat/WBP_DamageNumber.WBP_DamageNumber_C"));
    if (DefaultDamageWidget.Succeeded())
    {
        DamageWidgetClass = DefaultDamageWidget.Class;
    }

    constexpr int32 NumDamageWidgetSlots = 4;
    DamageWidgets.Reserve(NumDamageWidgetSlots);

    for (int32 i = 0; i < NumDamageWidgetSlots; ++i)
    {
        const FName SlotName = *FString::Printf(TEXT("DamageWidget_%d"), i);
        UWidgetComponent* W = CreateDefaultSubobject<UWidgetComponent>(SlotName);
        W->SetupAttachment(CapsuleComponent);
        // Stagger the slots vertically — slot 0 is the base height, each
        // subsequent slot sits one StackSpacing higher. BeginPlay re-applies
        // these in case a BP overrode DamageWidgetBaseZ / StackSpacing.
        W->SetRelativeLocation(FVector(0.f, 0.f, 200.f + i * 60.f));
        W->SetWidgetSpace(EWidgetSpace::Screen);
        W->SetDrawAtDesiredSize(true);
        W->SetVisibility(false);
        W->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        if (DamageWidgetClass)
        {
            W->SetWidgetClass(DamageWidgetClass);
        }
        DamageWidgets.Add(W);
    }
}

void ACombatantBase::BeginPlay()
{
    Super::BeginPlay();

    // Apply BP-overridden settings (constructor defaults may have been changed
    // via Class Defaults on a derived BP), and ensure all slots start hidden.
    DamageWidgetHideTimers.SetNum(DamageWidgets.Num());

    for (int32 i = 0; i < DamageWidgets.Num(); ++i)
    {
        if (UWidgetComponent* W = DamageWidgets[i])
        {
            W->SetRelativeLocation(FVector(
                0.f, 0.f, DamageWidgetBaseZ + i * DamageWidgetStackSpacing));

            if (DamageWidgetClass && W->GetWidgetClass() != DamageWidgetClass)
            {
                W->SetWidgetClass(DamageWidgetClass);
            }
            W->SetVisibility(false);
        }
    }
}

// -----------------------------------------------------------------------------
//  Damage number widget — fired automatically by ApplyDamage
// -----------------------------------------------------------------------------

namespace
{
    // Helper: write a uint8/int enum value to either an FByteProperty or
    // FEnumProperty on the given UObject by name. BP-defined enums can be
    // backed by either property type depending on UE version, so we try both.
    void SetEnumPropertyValue(UObject* Obj, const TCHAR* PropName, int64 Value)
    {
        UClass* C = Obj->GetClass();
        if (FByteProperty* BP = FindFProperty<FByteProperty>(C, PropName))
        {
            BP->SetPropertyValue_InContainer(Obj, static_cast<uint8>(Value));
        }
        else if (FEnumProperty* EP = FindFProperty<FEnumProperty>(C, PropName))
        {
            if (FNumericProperty* Underlying = EP->GetUnderlyingProperty())
            {
                void* Ptr = EP->ContainerPtrToValuePtr<void>(Obj);
                Underlying->SetIntPropertyValue(Ptr, Value);
            }
        }
    }
}

void ACombatantBase::ShowDamageNumber(const FDamagePayload& Payload)
{
    if (DamageWidgets.Num() == 0) return;

    // Find the lowest unused slot (so newer numbers appear at the bottom of
    // the stack and existing visible numbers naturally end up "above" them).
    int32 SlotIndex = INDEX_NONE;
    for (int32 i = 0; i < DamageWidgets.Num(); ++i)
    {
        if (DamageWidgets[i] && !DamageWidgets[i]->IsVisible())
        {
            SlotIndex = i;
            break;
        }
    }

    // All slots busy — overwrite the oldest (round-robin cursor).
    if (SlotIndex == INDEX_NONE)
    {
        SlotIndex = NextDamageWidgetIndex % DamageWidgets.Num();
    }
    NextDamageWidgetIndex = (SlotIndex + 1) % DamageWidgets.Num();

    UWidgetComponent* DW = DamageWidgets[SlotIndex];
    if (!DW) return;

    UUserWidget* W = DW->GetUserWidgetObject();
    if (!W)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ShowDamageNumber] %s slot %d has no UserWidget instance — "
                 "WidgetClass=%s. Did the BP override DamageWidgets to empty or "
                 "change WidgetClass to None?"),
            *GetName(), SlotIndex,
            DW->GetWidgetClass() ? *DW->GetWidgetClass()->GetName() : TEXT("None"));
        return;
    }

    UE_LOG(LogTemp, Warning,
        TEXT("[ShowDamageNumber] %s slot %d widget=%s sending Damage=%.1f Element=%d Resist=%d"),
        *GetName(), SlotIndex, *W->GetClass()->GetName(),
        Payload.ResolvedDamage,
        static_cast<int32>(Payload.Element),
        static_cast<int32>(Payload.HitResistance));

    // DamageAmount — UE5 BP "float" can be either FFloatProperty (32-bit)
    // or FDoubleProperty (64-bit) depending on settings; try both.
    if (FFloatProperty* FloatProp = FindFProperty<FFloatProperty>(W->GetClass(), TEXT("DamageAmount")))
    {
        FloatProp->SetPropertyValue_InContainer(W, Payload.ResolvedDamage);
    }
    else if (FDoubleProperty* DoubleProp = FindFProperty<FDoubleProperty>(W->GetClass(), TEXT("DamageAmount")))
    {
        DoubleProp->SetPropertyValue_InContainer(W, static_cast<double>(Payload.ResolvedDamage));
    }
    else
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[ShowDamageNumber] DamageAmount property not found on %s — "
                 "check the variable name in the widget BP."),
            *W->GetClass()->GetName());
    }

    // Element / Resistance (enums — BP enums can be Byte- or Enum-property)
    SetEnumPropertyValue(W, TEXT("Element"), static_cast<int64>(Payload.Element));
    SetEnumPropertyValue(W, TEXT("Resistance"), static_cast<int64>(Payload.HitResistance));

    // Trigger PlayPopup on the widget BP
    if (UFunction* Func = W->FindFunction(TEXT("PlayPopup")))
    {
        W->ProcessEvent(Func, nullptr);
    }

    DW->SetVisibility(true);

    // Schedule per-slot hide
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DamageWidgetHideTimers[SlotIndex]);

        TWeakObjectPtr<ACombatantBase> WeakThis(this);
        const int32 CapturedSlot = SlotIndex;

        World->GetTimerManager().SetTimer(
            DamageWidgetHideTimers[SlotIndex],
            FTimerDelegate::CreateLambda([WeakThis, CapturedSlot]()
            {
                if (ACombatantBase* Self = WeakThis.Get())
                {
                    Self->HideDamageWidgetSlot(CapturedSlot);
                }
            }),
            DamageWidgetVisibleDuration,
            false);
    }
}

void ACombatantBase::HideDamageWidgetSlot(int32 SlotIndex)
{
    if (DamageWidgets.IsValidIndex(SlotIndex) && DamageWidgets[SlotIndex])
    {
        DamageWidgets[SlotIndex]->SetVisibility(false);
    }
}

// -----------------------------------------------------------------------------
//  Battle setup
// -----------------------------------------------------------------------------

void ACombatantBase::InitializeForBattle()
{
    // HP persists between battles — only initialize on first call (Resources
    // map is empty for fresh-spawned enemies, populated for persistent player
    // party actors). RestPoint actors (Phase D) are responsible for refilling
    // HP back to max outside of battle.
    if (!Resources.Contains(EResourceType::HP))
    {
        Resources.Add(EResourceType::HP, FResourcePool(BaseStats.MaxHP));
    }
    else
    {
        // Existing HP carries over — but if MaxHP changed (e.g. danger scaling
        // raised an enemy's MaxHP), bump the cap so Current is still valid.
        FResourcePool& HP = Resources[EResourceType::HP];
        HP.Max = BaseStats.MaxHP;
        HP.Current = FMath::Min(HP.Current, HP.Max);
    }

    // AP resets every battle — it's the per-battle action currency.
    {
        FResourcePool APPool;
        APPool.Max     = BaseStats.MaxAP;
        APPool.Current = FMath::Clamp(BaseStats.StartingAP, 0.f, BaseStats.MaxAP);
        Resources.Add(EResourceType::AP, APPool);
    }

    // MP also resets to 0 each battle (built up during combat by minigames etc.).
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
        // Fire UI feedback so an "ABSORB" tag can pop with the heal amount.
        BP_OnDamageResolved(Payload);
        ShowDamageNumber(Payload);
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

    // Fire UI feedback — payload now has ResolvedDamage, HitResistance,
    // Element, and Source for the floating-number widget to read.
    BP_OnDamageResolved(Payload);
    ShowDamageNumber(Payload);
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
