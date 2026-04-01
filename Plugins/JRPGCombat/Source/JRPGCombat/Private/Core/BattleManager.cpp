#include "Core/BattleManager.h"
#include "Characters/Base/CombatantBase.h"
#include "Core/TurnOrderManager.h"
#include "Components/AbilityManagerComponent.h"
#include "Components/ProtocolManagerComponent.h"
#include "Abilities/CombatAbility.h"
#include "Abilities/Protocols/Ability_HealingProtocol.h"
#include "Abilities/Protocols/Ability_RevivalProtocol.h"
#include "Abilities/Protocols/Ability_APProtocol.h"
#include "UI/CombatHUDWidget.h"
#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"

ABattleManager::ABattleManager()
{
    PrimaryActorTick.bCanEverTick = false;

    ProtocolManager = CreateDefaultSubobject<UProtocolManagerComponent>(TEXT("ProtocolManager"));
}

void ABattleManager::BeginPlay()
{
    Super::BeginPlay();
}

// -----------------------------------------------------------------------------
//  Setup
// -----------------------------------------------------------------------------

void ABattleManager::StartBattle(const TArray<ACombatantBase*>& PlayerParty,
                                 const TArray<ACombatantBase*>& EnemyParty)
{
    AllCombatants.Reset();
    for (ACombatantBase* C : PlayerParty) { if (C) { AllCombatants.Add(C); } }
    for (ACombatantBase* C : EnemyParty)  { if (C) { AllCombatants.Add(C); } }
    Phase_Initialize();
}

void ABattleManager::RequestPlayerAbility(int32 AbilityIndex, const TArray<ACombatantBase*>& Targets)
{
    Phase_ExecutePlayerAction(AbilityIndex, Targets);
}

void ABattleManager::RequestSkipTurn()
{
    if (CurrentPhase != EBattlePhase::AwaitingInput) { return; }
    Phase_EndTurn();
}

// -----------------------------------------------------------------------------
//  Protocol execution
// -----------------------------------------------------------------------------

void ABattleManager::ExecuteProtocol(EProtocolType Type, const TArray<ACombatantBase*>& Targets)
{
    if (CurrentPhase != EBattlePhase::AwaitingInput) { return; }
    if (!ProtocolManager || !ProtocolManager->CanSpend(Type))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] ExecuteProtocol: no charges for protocol %d."),
            (int32)Type);
        return;
    }

    ProtocolManager->SpendCharge(Type);
    SetPhase(EBattlePhase::ExecutingAction);

    // Instantiate a temporary ability object to run the execution pipeline.
    // These are lightweight UObjects — creating one per use is acceptable.
    UCombatAbility* Proto = nullptr;
    switch (Type)
    {
        case EProtocolType::Healing:
            Proto = NewObject<UAbility_HealingProtocol>(this); break;
        case EProtocolType::Revival:
            Proto = NewObject<UAbility_RevivalProtocol>(this); break;
        case EProtocolType::AP:
            Proto = NewObject<UAbility_APProtocol>(this);      break;
    }

    if (Proto && ActiveCombatant)
    {
        // Protocols are support actions — play the cast animation.
        ActiveCombatant->PlayAbilityAnimation(EAbilityCategory::Skill);
        Proto->Execute(ActiveCombatant, Targets);
    }

    if (TryResolveBattleEnd()) { ActiveCombatant = nullptr; return; }

    Phase_EndTurn();
}

// -----------------------------------------------------------------------------
//  Phases
// -----------------------------------------------------------------------------

void ABattleManager::Phase_Initialize()
{
    SetPhase(EBattlePhase::Initialization);

    if (ProtocolManager) { ProtocolManager->InitializeCharges(); }

    TurnOrderManager = NewObject<UTurnOrderManager>(this);

    TArray<ACombatantBase*> Raw;
    for (ACombatantBase* C : AllCombatants)
    {
        if (!C) { continue; }
        C->InitializeForBattle();
        Raw.Add(C);
    }

    TurnOrderManager->BuildTurnOrder(Raw);

    // ── Face combatants toward the opposing team ──────────────────────────────
    {
        FVector PlayerCenter = FVector::ZeroVector;
        int32   PlayerCount  = 0;
        FVector EnemyCenter  = FVector::ZeroVector;
        int32   EnemyCount   = 0;

        for (ACombatantBase* C : Raw)
        {
            if (C->GetTeam() == ECombatTeam::Player) { PlayerCenter += C->GetActorLocation(); ++PlayerCount; }
            else                                      { EnemyCenter  += C->GetActorLocation(); ++EnemyCount;  }
        }

        if (PlayerCount > 0) PlayerCenter /= static_cast<float>(PlayerCount);
        if (EnemyCount  > 0) EnemyCenter  /= static_cast<float>(EnemyCount);

        for (ACombatantBase* C : Raw)
        {
            const FVector LookAt = (C->GetTeam() == ECombatTeam::Player) ? EnemyCenter : PlayerCenter;
            const FVector Dir    = (LookAt - C->GetActorLocation()).GetSafeNormal2D();
            if (!Dir.IsNearlyZero())
            {
                C->SetActorRotation(Dir.Rotation());
            }
        }
    }

    Phase_StartNextTurn();
}

void ABattleManager::Phase_StartNextTurn()
{
    if (CheckVictory()) { SetPhase(EBattlePhase::Victory);  OnBattleEnded.Broadcast(true);  return; }
    if (CheckDefeat())  { SetPhase(EBattlePhase::Defeat);   OnBattleEnded.Broadcast(false); return; }

    SetPhase(EBattlePhase::TurnStart);

    if (TurnOrderManager->IsQueueEmpty())
    {
        ++TurnNumber;
        TArray<ACombatantBase*> Living;
        for (ACombatantBase* C : AllCombatants)
        {
            if (C && !C->IsDead()) { Living.Add(C); }
        }
        TurnOrderManager->BuildTurnOrder(Living);
    }

    ActiveCombatant = TurnOrderManager->PopNextCombatant();
    if (!ActiveCombatant) { return; }

    ActiveCombatant->OnTurnStart();
    OnCombatantTurnStarted.Broadcast(ActiveCombatant);

    if (TryResolveBattleEnd()) { ActiveCombatant = nullptr; return; }

    if (ActiveCombatant->IsDead()) { Phase_EndTurn(); return; }

    if (IsPlayerTurn())
    {
        OnTurnOwnerChanged(true);
        RebuildPlayerCursorList(ActiveCombatant);
        SetPhase(EBattlePhase::AwaitingInput);
    }
    else
    {
        // Clear player cursor during enemy turn.
        ActivePlayerCombatant = nullptr;
        PlayerCursorList.Empty();
        PlayerCursorIndex = 0;

        OnTurnOwnerChanged(false);

        // Broadcast which enemy is now acting so HUD can show orange border.
        OnEnemyActingChanged.Broadcast(ActiveCombatant);

        GetWorldTimerManager().SetTimer(
            EnemyPreActionTimerHandle,
            this,
            &ABattleManager::Phase_ExecuteEnemyAction,
            EnemyPreActionDelay,
            false);
    }
}

void ABattleManager::Phase_ExecutePlayerAction(int32 AbilityIndex,
                                                const TArray<ACombatantBase*>& Targets)
{
    if (!ActiveCombatant || CurrentPhase != EBattlePhase::AwaitingInput) { return; }

    SetPhase(EBattlePhase::ExecutingAction);

    // Apply the minigame result multiplier to the ability before execution.
    // If no minigame ran, PendingDamageMultiplier is 1.0 — no effect.
    if (UCombatAbility* Ability = ActiveCombatant->AbilityManager->GetAbility(AbilityIndex))
    {
        Ability->ActiveMultiplier = PendingDamageMultiplier;
    }
    PendingDamageMultiplier = 1.0f; // always reset, regardless of success

    // Trigger the matching animation before the ability resolves.
    if (const UCombatAbility* Ability = ActiveCombatant->AbilityManager->GetAbility(AbilityIndex))
    {
        ActiveCombatant->PlayAbilityAnimation(Ability->AbilityCategory);
    }

    const bool bActivated = ActiveCombatant->AbilityManager->TryActivateAbility(AbilityIndex, Targets);
    if (!bActivated)
    {
        SetPhase(EBattlePhase::AwaitingInput);
        return;
    }

    if (TryResolveBattleEnd()) { ActiveCombatant = nullptr; return; }

    const UCombatAbility* Used = ActiveCombatant->AbilityManager->GetAbility(AbilityIndex);
    if (!Used || Used->bEndsTurn) { Phase_EndTurn(); }
    else                          { SetPhase(EBattlePhase::AwaitingInput); }
}

void ABattleManager::Phase_ExecuteEnemyAction()
{
    if (!ActiveCombatant) { return; }

    SetPhase(EBattlePhase::ExecutingAction);

    const UCombatAbility* Ability = ActiveCombatant->AbilityManager->GetAbility(0);
    const ETargetScope Scope = Ability ? Ability->TargetScope : ETargetScope::SingleEnemy;

    TArray<ACombatantBase*> Candidates = GetValidTargets(Scope, ActiveCombatant);
    if (Candidates.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] Enemy %s: no valid targets, skipping."),
            *ActiveCombatant->GetName());
        Phase_EndTurn();
        return;
    }

    TArray<ACombatantBase*> Targets;
    if (Scope == ETargetScope::AllEnemies || Scope == ETargetScope::AllAllies)
        Targets = Candidates;
    else if (Scope == ETargetScope::Self)
        Targets = { ActiveCombatant };
    else
        Targets = { Candidates[FMath::RandRange(0, Candidates.Num() - 1)] };

    // Trigger animation for the enemy's ability.
    if (Ability)
    {
        ActiveCombatant->PlayAbilityAnimation(Ability->AbilityCategory);
    }

    ActiveCombatant->AbilityManager->TryActivateAbility(0, Targets);

    if (TryResolveBattleEnd()) { ActiveCombatant = nullptr; return; }

    GetWorldTimerManager().SetTimer(
        EnemyPostActionTimerHandle,
        this,
        &ABattleManager::Phase_EndTurn,
        EnemyPostActionDelay,
        false);
}

void ABattleManager::Phase_EndTurn()
{
    SetPhase(EBattlePhase::TurnEnd);

    // Clear target selection state.
    PendingAbilityIndex  = -1;
    bTargetingProtocol   = false;
    TargetCandidates.Empty();
    TargetCandidateIndex = 0;

    // Clear player cursor.
    ActivePlayerCombatant = nullptr;
    PlayerCursorList.Empty();
    PlayerCursorIndex = 0;

    // Clear enemy acting highlight.
    OnEnemyActingChanged.Broadcast(nullptr);

    if (ActiveCombatant) { ActiveCombatant->OnTurnEnd(); }
    ActiveCombatant = nullptr;

    Phase_StartNextTurn();
}

// -----------------------------------------------------------------------------
//  Active-player cursor
// -----------------------------------------------------------------------------

void ABattleManager::SwitchActivePlayer(int32 Direction)
{
    if (CurrentPhase != EBattlePhase::AwaitingInput) { return; }

    const int32 Count = PlayerCursorList.Num();
    if (Count == 0) { return; }

    for (int32 Steps = 0; Steps < Count; ++Steps)
    {
        PlayerCursorIndex = (PlayerCursorIndex + Direction + Count) % Count;
        ACombatantBase* Candidate = PlayerCursorList[PlayerCursorIndex].Get();
        if (Candidate && !Candidate->IsDead())
        {
            ActivePlayerCombatant = Candidate;
            OnActivePlayerChanged.Broadcast(Candidate);
            return;
        }
    }
}

// -----------------------------------------------------------------------------
//  Target selection
// -----------------------------------------------------------------------------

void ABattleManager::BeginTargetSelection(int32 AbilityIndex)
{
    if (!ActiveCombatant || CurrentPhase != EBattlePhase::AwaitingInput) { return; }

    const UCombatAbility* Ability = ActiveCombatant->AbilityManager->GetAbility(AbilityIndex);
    if (!Ability) { return; }

    PendingScope        = Ability->TargetScope;
    PendingAbilityIndex = AbilityIndex;
    bTargetingProtocol  = false;

    TArray<ACombatantBase*> Candidates;
    for (ACombatantBase* C : GetValidTargets(PendingScope, ActiveCombatant))
    {
        if (C) { Candidates.Add(C); }
    }

    if (Candidates.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] BeginTargetSelection: no valid targets for ability %d."),
            AbilityIndex);
        PendingAbilityIndex = -1;
        return;
    }

    const bool bNeedsNav = (PendingScope == ETargetScope::SingleEnemy ||
                            PendingScope == ETargetScope::SingleAlly);
    if (!bNeedsNav) { EnterTargetSelectionWithCandidates(PendingScope, MoveTemp(Candidates)); ConfirmTargetSelection(); return; }

    EnterTargetSelectionWithCandidates(PendingScope, MoveTemp(Candidates));
}

void ABattleManager::BeginProtocolTargetSelection(EProtocolType Type)
{
    if (!ActiveCombatant || CurrentPhase != EBattlePhase::AwaitingInput) { return; }
    if (!ProtocolManager || !ProtocolManager->CanSpend(Type))
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] BeginProtocolTargetSelection: no charges."));
        return;
    }

    bTargetingProtocol  = true;
    PendingProtocolType = Type;
    PendingAbilityIndex = -1;  // Not an ability slot.

    // Choose scope by protocol type.
    ETargetScope Scope = ETargetScope::SingleAlly;
    if (Type == EProtocolType::Revival) { Scope = ETargetScope::DeadAlly; }
    PendingScope = Scope;

    TArray<ACombatantBase*> Candidates;
    for (ACombatantBase* C : GetValidTargets(Scope, ActiveCombatant))
    {
        if (C) { Candidates.Add(C); }
    }

    if (Candidates.IsEmpty())
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] BeginProtocolTargetSelection: no valid targets."));
        bTargetingProtocol = false;
        return;
    }

    // For Revival only: if there is exactly one dead ally, auto-confirm
    // (no choice to make). For Healing and AP, always show the selection UI
    // so the player can choose which ally to target.
    const bool bAutoConfirm = (Type == EProtocolType::Revival && Candidates.Num() == 1);
    EnterTargetSelectionWithCandidates(Scope, MoveTemp(Candidates));
    if (bAutoConfirm) { ConfirmTargetSelection(); }
}

void ABattleManager::NavigateTargets(int32 Direction)
{
    if (CurrentPhase != EBattlePhase::SelectingTarget || TargetCandidates.IsEmpty()) { return; }

    const int32 Num = TargetCandidates.Num();
    for (int32 Steps = 0; Steps < Num; ++Steps)
    {
        TargetCandidateIndex = (TargetCandidateIndex + Direction + Num) % Num;
        ACombatantBase* C = TargetCandidates[TargetCandidateIndex].Get();
        if (!C) { continue; }

        // For DeadAlly scope: only stop on dead units.
        // For all other scopes: only stop on living units.
        const bool bValid = (PendingScope == ETargetScope::DeadAlly)
            ? C->IsDead()
            : !C->IsDead();

        if (bValid) { break; }
    }

    OnTargetSelectionChanged.Broadcast(GetCurrentTarget());
}

void ABattleManager::ConfirmTargetSelection()
{
    TArray<ACombatantBase*> FinalTargets;

    switch (PendingScope)
    {
        case ETargetScope::Self:
            FinalTargets = { ActiveCombatant };
            break;

        case ETargetScope::AllEnemies:
        case ETargetScope::AllAllies:
            for (auto& C : TargetCandidates) { if (C && !C->IsDead()) { FinalTargets.Add(C); } }
            break;

        case ETargetScope::DeadAlly:
            for (auto& C : TargetCandidates) { if (C && C->IsDead()) { FinalTargets.Add(C); } }
            // For single dead-ally selection, only use the currently highlighted one.
            if (FinalTargets.Num() > 1)
            {
                ACombatantBase* Picked = GetCurrentTarget();
                FinalTargets = Picked ? TArray<ACombatantBase*>{ Picked } : TArray<ACombatantBase*>{};
            }
            break;

        default: // SingleEnemy, SingleAlly
        {
            ACombatantBase* Picked = GetCurrentTarget();
            if (Picked && !Picked->IsDead()) { FinalTargets.Add(Picked); }
            break;
        }
    }

    const bool bWasProtocol = bTargetingProtocol;
    const EProtocolType ProtoType = PendingProtocolType;
    const int32 AbilIdx = PendingAbilityIndex;

    // Clear selection state before executing.
    PendingAbilityIndex  = -1;
    bTargetingProtocol   = false;
    TargetCandidates.Empty();
    TargetCandidateIndex = 0;
    CurrentPhase = EBattlePhase::AwaitingInput;

    if (bWasProtocol)
    {
        ExecuteProtocol(ProtoType, FinalTargets);
    }
    else
    {
        // Check if this is a Skill ability that has a minigame configured.
        const UCombatAbility* Ability = ActiveCombatant
            ? ActiveCombatant->AbilityManager->GetAbility(AbilIdx)
            : nullptr;

        const bool bNeedsMinigame = Ability
            && Ability->AbilityCategory == EAbilityCategory::Skill
            && Ability->MinigameClass != nullptr;

        if (bNeedsMinigame)
        {
            // Store confirmed targets and ability index for after the minigame.
            PendingSkillAbilityIndex = AbilIdx;
            PendingSkillTargets.Reset();
            for (ACombatantBase* T : FinalTargets) { if (T) PendingSkillTargets.Add(T); }

            // Enter minigame phase — blocks normal input until the panel calls
            // ExecutePendingSkillAfterMinigame().
            SetPhase(EBattlePhase::MinigameActive);

            // Signal the panel to create and show the minigame widget.
            OnSkillMinigameShouldStart.Broadcast(AbilIdx);
        }
        else
        {
            Phase_ExecutePlayerAction(AbilIdx, FinalTargets);
        }
    }
}

void ABattleManager::CancelTargetSelection()
{
    if (CurrentPhase != EBattlePhase::SelectingTarget) { return; }

    PendingAbilityIndex  = -1;
    bTargetingProtocol   = false;
    TargetCandidates.Empty();
    TargetCandidateIndex = 0;

    SetPhase(EBattlePhase::AwaitingInput);
}

void ABattleManager::CancelPendingMinigameSkill()
{
    if (CurrentPhase != EBattlePhase::MinigameActive) { return; }

    // Clear the stored targets and ability so nothing executes.
    PendingSkillAbilityIndex = -1;
    PendingSkillTargets.Empty();
    PendingDamageMultiplier = 1.0f;

    // Cancel any pending post-result timer (in case it somehow fired).
    GetWorldTimerManager().ClearTimer(MinigamePostResultTimerHandle);

    // Return to AwaitingInput so the action panel can restore the skill menu.
    SetPhase(EBattlePhase::AwaitingInput);
}

void ABattleManager::ExecutePendingSkillAfterMinigame(float Multiplier)
{
    if (CurrentPhase != EBattlePhase::MinigameActive) { return; }

    PendingDamageMultiplier = Multiplier;

    // Brief delay (~0.4 s) so the player can register the minigame result
    // visually before the skill fires.
    FTimerDelegate Delegate;
    Delegate.BindLambda([this]()
    {
        // Collect raw pointers from the stored TObjectPtrs.
        TArray<ACombatantBase*> Targets;
        for (auto& T : PendingSkillTargets) { if (T.Get()) { Targets.Add(T.Get()); } }

        const int32 AbilIdx = PendingSkillAbilityIndex;

        // Clear pending state before execution so a mid-action crash can't reuse it.
        PendingSkillAbilityIndex = -1;
        PendingSkillTargets.Empty();

        // Phase_ExecutePlayerAction requires AwaitingInput.
        SetPhase(EBattlePhase::AwaitingInput);
        Phase_ExecutePlayerAction(AbilIdx, Targets);
    });

    GetWorldTimerManager().SetTimer(MinigamePostResultTimerHandle, Delegate, 0.4f, false);
}

ACombatantBase* ABattleManager::GetCurrentTarget() const
{
    if (TargetCandidates.IsValidIndex(TargetCandidateIndex))
    {
        return TargetCandidates[TargetCandidateIndex].Get();
    }
    return nullptr;
}

TArray<ACombatantBase*> ABattleManager::GetValidTargets(ETargetScope Scope,
                                                         ACombatantBase* Actor) const
{
    TArray<ACombatantBase*> Result;
    if (!Actor) { return Result; }

    switch (Scope)
    {
        case ETargetScope::Self:
            Result.Add(Actor);
            break;

        case ETargetScope::SingleEnemy:
        case ETargetScope::AllEnemies:
            for (const auto& C : AllCombatants)
            {
                if (C && !C->IsDead() && C->GetTeam() != Actor->GetTeam())
                    Result.Add(C.Get());
            }
            break;

        case ETargetScope::SingleAlly:
        case ETargetScope::AllAllies:
            // Include the acting character themselves (self-targeting allowed for protocols).
            for (const auto& C : AllCombatants)
            {
                if (C && !C->IsDead() && C->GetTeam() == Actor->GetTeam())
                    Result.Add(C.Get());
            }
            break;

        case ETargetScope::DeadAlly:
            // Only dead allies (for Revival Protocol).
            for (const auto& C : AllCombatants)
            {
                if (C && C->IsDead() && C->GetTeam() == Actor->GetTeam())
                    Result.Add(C.Get());
            }
            break;
    }

    return Result;
}

// -----------------------------------------------------------------------------
//  Combat HUD
// -----------------------------------------------------------------------------

UCombatHUDWidget* ABattleManager::CreateAndShowHUD(APlayerController* PC)
{
    if (!HUDClass || !PC) { return nullptr; }

    CombatHUD = CreateWidget<UCombatHUDWidget>(PC, HUDClass);
    if (CombatHUD)
    {
        CombatHUD->AddToViewport();
        CombatHUD->InitializeHUD(this);
    }
    return CombatHUD;
}

// -----------------------------------------------------------------------------
//  Win / loss
// -----------------------------------------------------------------------------

bool ABattleManager::CheckVictory() const
{
    for (const ACombatantBase* C : AllCombatants)
    {
        if (C && !C->IsDead() && C->GetTeam() == ECombatTeam::Enemy) { return false; }
    }
    return true;
}

bool ABattleManager::CheckDefeat() const
{
    for (const ACombatantBase* C : AllCombatants)
    {
        if (C && !C->IsDead() && C->GetTeam() == ECombatTeam::Player) { return false; }
    }
    return true;
}

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

bool ABattleManager::TryResolveBattleEnd()
{
    if (CheckVictory()) { SetPhase(EBattlePhase::Victory); OnBattleEnded.Broadcast(true);  return true; }
    if (CheckDefeat())  { SetPhase(EBattlePhase::Defeat);  OnBattleEnded.Broadcast(false); return true; }
    return false;
}

void ABattleManager::SetPhase(EBattlePhase NewPhase)
{
    CurrentPhase = NewPhase;
    OnPhaseChanged.Broadcast(NewPhase);
}

bool ABattleManager::IsPlayerTurn() const
{
    return ActiveCombatant && ActiveCombatant->GetTeam() == ECombatTeam::Player;
}

TArray<ACombatantBase*> ABattleManager::GetAllCombatants() const
{
    TArray<ACombatantBase*> Result;
    for (const auto& C : AllCombatants) { if (C) { Result.Add(C.Get()); } }
    return Result;
}

void ABattleManager::RebuildPlayerCursorList(ACombatantBase* DefaultUnit)
{
    PlayerCursorList.Empty();
    PlayerCursorIndex = 0;

    for (const auto& C : AllCombatants)
    {
        if (C && !C->IsDead() && C->GetTeam() == ECombatTeam::Player)
        {
            PlayerCursorList.Add(C);
        }
    }

    if (PlayerCursorList.IsEmpty()) { ActivePlayerCombatant = nullptr; return; }

    for (int32 i = 0; i < PlayerCursorList.Num(); ++i)
    {
        if (PlayerCursorList[i].Get() == DefaultUnit) { PlayerCursorIndex = i; break; }
    }

    ActivePlayerCombatant = PlayerCursorList[PlayerCursorIndex];
    OnActivePlayerChanged.Broadcast(ActivePlayerCombatant.Get());
}

void ABattleManager::EnterTargetSelectionWithCandidates(ETargetScope Scope,
                                                         TArray<ACombatantBase*>&& Candidates)
{
    TargetCandidates.Empty();
    for (ACombatantBase* C : Candidates) { if (C) { TargetCandidates.Add(C); } }

    TargetCandidateIndex = 0;
    PendingScope = Scope;

    SetPhase(EBattlePhase::SelectingTarget);
    OnTargetSelectionChanged.Broadcast(GetCurrentTarget());
}
