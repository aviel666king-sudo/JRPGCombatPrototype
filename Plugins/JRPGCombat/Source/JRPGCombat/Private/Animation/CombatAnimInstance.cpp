#include "Animation/CombatAnimInstance.h"
#include "Characters/Base/CombatantBase.h"
#include "Animation/AnimMontage.h"
#include "Core/BattleManager.h"
#include "EngineUtils.h"

// -----------------------------------------------------------------------------
//  UAnimInstance overrides
// -----------------------------------------------------------------------------

void UCombatAnimInstance::NativeInitializeAnimation()
{
    Super::NativeInitializeAnimation();

    OwnerCombatant = Cast<ACombatantBase>(GetOwningActor());

    OnMontageEnded.AddDynamic(this, &UCombatAnimInstance::HandleMontageEnded);
}

void UCombatAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Only the player character needs gun aim data — skip enemies.
    if (!OwnerCombatant || OwnerCombatant->GetTeam() != ECombatTeam::Player) { return; }

    UWorld* World = GetWorld();
    if (!World) { return; }

    for (TActorIterator<ABattleManager> It(World); It; ++It)
    {
        bIsGunAiming  = (*It)->IsGunAimActive();
        GunAimYaw     = bIsGunAiming ? (*It)->GetGunAimYaw()   : 0.f;
        GunAimPitch   = bIsGunAiming ? (*It)->GetGunAimPitch() : 0.f;
        break;
    }
}

void UCombatAnimInstance::NativeUninitializeAnimation()
{
    OnMontageEnded.RemoveDynamic(this, &UCombatAnimInstance::HandleMontageEnded);

    Super::NativeUninitializeAnimation();
}

// -----------------------------------------------------------------------------
//  API
// -----------------------------------------------------------------------------

void UCombatAnimInstance::PlayCombatMontage(ECombatAnimState Action)
{
    UAnimMontage* MontageToPlay = nullptr;

    switch (Action)
    {
    case ECombatAnimState::Attack:   MontageToPlay = AttackMontage;   break;
    case ECombatAnimState::Casting:  MontageToPlay = CastMontage;     break;
    case ECombatAnimState::Parry:    MontageToPlay = ParryMontage;    break;
    case ECombatAnimState::Gun:      MontageToPlay = GunMontage;      break;
    case ECombatAnimState::HitReact: MontageToPlay = HitReactMontage; break;
    case ECombatAnimState::Death:    MontageToPlay = DeathMontage;    break;
    default: break;
    }

    if (!MontageToPlay)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[CombatAnimInstance] PlayCombatMontage: no montage assigned for state %d on %s."),
            (int32)Action,
            OwnerCombatant ? *OwnerCombatant->GetName() : TEXT("Unknown"));
        return;
    }

    CombatState = Action;
    Montage_Play(MontageToPlay);
}

bool UCombatAnimInstance::IsPlayingActionMontage() const
{
    return IsAnyMontagePlaying();
}

// -----------------------------------------------------------------------------
//  Private
// -----------------------------------------------------------------------------

void UCombatAnimInstance::HandleMontageEnded(UAnimMontage* Montage, bool bInterrupted)
{
    // Return to idle after any action montage finishes (except death, which holds).
    if (CombatState != ECombatAnimState::Death)
    {
        CombatState = ECombatAnimState::Idle;
    }
}
