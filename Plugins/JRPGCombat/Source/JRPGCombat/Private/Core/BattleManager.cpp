#include "Core/BattleManager.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/CapsuleComponent.h"
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
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

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

    // ── Cache PlayerController early so camera calls work on Turn 1 ──────────
    if (!CachedPlayerController)
    {
        CachedPlayerController = GetWorld()->GetFirstPlayerController();
    }

    // ── Use whatever camera the Level Blueprint already set as the base ───────
    if (!BaseCameraActor && CachedPlayerController)
    {
        BaseCameraActor = Cast<ACameraActor>(CachedPlayerController->GetViewTarget());
    }

    // ── Auto-discover spawn points and named cameras ──────────────────────────
    {
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            AActor* A = *It;
            FString N = A->GetActorNameOrLabel();

            // Spawn pads
            if (N.StartsWith(TEXT("PartySlot"))) PlayerSpawnPoints.AddUnique(A);
            if (N.StartsWith(TEXT("EnemySlot"))) EnemySpawnPoints.AddUnique(A);

            // Named runtime cameras (repositioned dynamically — level placement doesn't matter)
            if (ACameraActor* Cam = Cast<ACameraActor>(A))
            {
                if      (N.Equals(TEXT("CAM_CharacterFocus"))) CharacterFocusCameraActor = Cam;
                else if (N.Equals(TEXT("CAM_EnemyCursor")))    EnemyCursorCameraActor    = Cam;
                else if (N.Equals(TEXT("CAM_GunAim")))         GunAimCameraActor         = Cam;
            }
        }
    }

    // ── Teleport combatants to their spawn points ─────────────────────────────
    {
        TArray<ACombatantBase*> Players, Enemies;
        for (ACombatantBase* C : Raw)
        {
            if (C->GetTeam() == ECombatTeam::Player) Players.Add(C);
            else                                      Enemies.Add(C);
        }

        auto TeleportToSpawn = [this](ACombatantBase* C, AActor* SpawnPoint)
        {
            const FVector PadLoc = SpawnPoint->GetActorLocation();

            // Trace straight down from high above to find whatever surface
            // is directly above the pad's XY position (pad top or floor tile).
            FHitResult Hit;
            GetWorld()->LineTraceSingleByChannel(
                Hit,
                FVector(PadLoc.X, PadLoc.Y, 5000.f),
                FVector(PadLoc.X, PadLoc.Y, -500.f),
                ECC_WorldStatic,
                FCollisionQueryParams(TEXT("SpawnTrace"), false, C));

            const float SurfaceZ  = Hit.bBlockingHit ? Hit.ImpactPoint.Z : PadLoc.Z;
            const float HalfHeight = C->CapsuleComponent
                ? C->CapsuleComponent->GetScaledCapsuleHalfHeight()
                : 90.f;

            C->SetActorLocation(FVector(PadLoc.X, PadLoc.Y, SurfaceZ + HalfHeight));
        };

        for (int32 i = 0; i < Players.Num(); ++i)
        {
            if (PlayerSpawnPoints.IsValidIndex(i) && PlayerSpawnPoints[i])
                TeleportToSpawn(Players[i], PlayerSpawnPoints[i]);
        }

        for (int32 i = 0; i < Enemies.Num(); ++i)
        {
            if (EnemySpawnPoints.IsValidIndex(i) && EnemySpawnPoints[i])
                TeleportToSpawn(Enemies[i], EnemySpawnPoints[i]);
        }
    }

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
        // Blend camera to frame the active player character.
        FocusCameraOnPlayer(ActiveCombatant);

        OnTurnOwnerChanged(true);
        RebuildPlayerCursorList(ActiveCombatant);
        SetPhase(EBattlePhase::AwaitingInput);
    }
    else
    {
        // Enemy turn — return to the static overview camera.
        ReturnCameraToBase();

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

    // Snap camera to the newly highlighted target if targeting a single enemy.
    if (PendingScope == ETargetScope::SingleEnemy)
    {
        FocusCameraOnEnemy(GetCurrentTarget(), 0.20f);
    }
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

    // Return camera to the active player character.
    if (ActiveCombatant) { FocusCameraOnPlayer(ActiveCombatant, 0.40f); }
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

    // Cache the player controller for camera blending and gun aim.
    CachedPlayerController = PC;

    CombatHUD = CreateWidget<UCombatHUDWidget>(PC, HUDClass);
    if (CombatHUD)
    {
        CombatHUD->AddToViewport();
        CombatHUD->InitializeHUD(this);
    }

    // Allow both UI and game input simultaneously so RMB reaches the HUD widget.
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    InputMode.SetHideCursorDuringCapture(false);
    PC->SetInputMode(InputMode);
    PC->bShowMouseCursor = true;

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

// -----------------------------------------------------------------------------
//  Camera system
// -----------------------------------------------------------------------------

void ABattleManager::FocusCameraOnPlayer(ACombatantBase* Player, float BlendTime)
{
    if (!CharacterFocusCameraActor || !Player || !CachedPlayerController) { return; }
    PositionCharacterFocusCamera(Player);
    CachedPlayerController->SetViewTargetWithBlend(
        CharacterFocusCameraActor, BlendTime, VTBlend_Cubic);
}

void ABattleManager::FocusCameraOnEnemy(ACombatantBase* Enemy, float BlendTime)
{
    if (!EnemyCursorCameraActor || !Enemy || !CachedPlayerController) { return; }
    PositionEnemyCursorCamera(Enemy);
    CachedPlayerController->SetViewTargetWithBlend(
        EnemyCursorCameraActor, BlendTime, VTBlend_Cubic);
}

void ABattleManager::ReturnCameraToBase(float BlendTime)
{
    if (!BaseCameraActor || !CachedPlayerController) { return; }
    CachedPlayerController->SetViewTargetWithBlend(
        BaseCameraActor, BlendTime, VTBlend_Cubic);
}

void ABattleManager::PositionCharacterFocusCamera(ACombatantBase* Target)
{
    if (!CharacterFocusCameraActor || !Target) { return; }

    const FVector CharLoc = Target->GetActorLocation();
    const FVector Forward = Target->GetActorForwardVector(); // toward enemies
    const FVector Right   = Target->GetActorRightVector();

    // Place camera behind the character, to their right (so character appears
    // slightly left-of-centre in frame), and elevated for a good JRPG angle.
    const FVector CamPos = CharLoc
        - Forward * 200.f
        + Right   * 150.f
        + FVector(0.f, 0.f, 160.f);

    const FVector  LookAt  = CharLoc + FVector(0.f, 0.f, 50.f);
    const FRotator CamRot  = (LookAt - CamPos).Rotation();

    CharacterFocusCameraActor->SetActorLocationAndRotation(CamPos, CamRot);
}

void ABattleManager::PositionEnemyCursorCamera(ACombatantBase* Target)
{
    if (!EnemyCursorCameraActor || !Target) { return; }

    const FVector EnemyLoc = Target->GetActorLocation();
    const FVector EnemyFwd = Target->GetActorForwardVector(); // toward players

    // Position the camera between the two parties, slightly in front of the
    // enemy (on the player-team side), at head height.
    const FVector CamPos = EnemyLoc
        + EnemyFwd * 220.f
        + FVector(0.f, 0.f, 90.f);

    const FVector  LookAt = EnemyLoc + FVector(0.f, 0.f, 30.f);
    const FRotator CamRot = (LookAt - CamPos).Rotation();

    EnemyCursorCameraActor->SetActorLocationAndRotation(CamPos, CamRot);
}

void ABattleManager::PositionGunAimCamera(ACombatantBase* Player)
{
    if (!GunAimCameraActor || !Player) { return; }

    const FVector CharLoc = Player->GetActorLocation();
    const FVector Forward = Player->GetActorForwardVector();
    const FVector Right   = Player->GetActorRightVector();

    // Over-the-shoulder: behind and to the right of the character, at shoulder height.
    const FVector CamPos = CharLoc
        - Forward * 80.f
        + Right   * 55.f
        + FVector(0.f, 0.f, 75.f);

    // Look toward enemies (character's forward direction).
    GunAimCameraActor->SetActorLocationAndRotation(CamPos, Forward.Rotation());
    GunAimBaseRotation = Forward.Rotation();
}

// -----------------------------------------------------------------------------
//  Gun aim mode
// -----------------------------------------------------------------------------

void ABattleManager::BeginGunAimMode()
{
    if (bGunAimActive) { return; }
    if (!ActiveCombatant || ActiveCombatant->GetTeam() != ECombatTeam::Player) { return; }
    if (CurrentPhase != EBattlePhase::AwaitingInput) { return; }

    bGunAimActive     = true;
    GunAimYawOffset   = 0.f;
    GunAimPitchOffset = 0.f;

    PositionGunAimCamera(ActiveCombatant);

    if (GunAimCameraActor && CachedPlayerController)
    {
        CachedPlayerController->SetViewTargetWithBlend(
            GunAimCameraActor, 0.20f, VTBlend_Cubic);
        CachedPlayerController->bShowMouseCursor = false;
    }

    OnGunAimChanged.Broadcast(true);
}

void ABattleManager::EndGunAimMode()
{
    if (!bGunAimActive) { return; }

    bGunAimActive     = false;
    GunAimYawOffset   = 0.f;
    GunAimPitchOffset = 0.f;

    // Restore cursor visibility.
    if (CachedPlayerController)
    {
        CachedPlayerController->bShowMouseCursor = true;
    }

    // Return camera to the acting player character.
    if (ActiveCombatant) { FocusCameraOnPlayer(ActiveCombatant, 0.30f); }
    else                 { ReturnCameraToBase(0.30f); }

    OnGunAimChanged.Broadcast(false);
}

void ABattleManager::UpdateGunAimRotation(float DeltaYaw, float DeltaPitch)
{
    if (!bGunAimActive || !GunAimCameraActor) { return; }

    GunAimYawOffset = FMath::Clamp(
        GunAimYawOffset   + DeltaYaw   * GunAimSensitivity,
        -GunAimYawLimit,   GunAimYawLimit);

    GunAimPitchOffset = FMath::Clamp(
        GunAimPitchOffset + DeltaPitch * GunAimSensitivity,
        GunAimPitchMin,    GunAimPitchMax);

    const FRotator NewRot(
        GunAimBaseRotation.Pitch + GunAimPitchOffset,
        GunAimBaseRotation.Yaw   + GunAimYawOffset,
        0.f);

    GunAimCameraActor->SetActorRotation(NewRot);
}

void ABattleManager::FireGunAimShot()
{
    if (!bGunAimActive || !ActiveCombatant || !GunAimCameraActor) { return; }

    // Find the gun ability on the active combatant.
    UAbilityManagerComponent* AM = ActiveCombatant->AbilityManager;
    if (!AM) { return; }

    int32 GunAbilIdx = -1;
    for (int32 i = 0; i < AM->GetAbilityCount(); ++i)
    {
        const UCombatAbility* A = AM->GetAbility(i);
        if (A && A->AbilityCategory == EAbilityCategory::Gun)
        {
            GunAbilIdx = i;
            break;
        }
    }

    if (GunAbilIdx == -1)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] FireGunAimShot: no Gun ability on %s."),
            *ActiveCombatant->GetName());
        EndGunAimMode();
        return;
    }

    const UCombatAbility* GunAbil = AM->GetAbility(GunAbilIdx);
    if (!GunAbil) { EndGunAimMode(); return; }

    // Check affordability.
    bool bCanAfford = true;
    for (const FAbilityCost& Cost : GunAbil->Costs)
    {
        if (!ActiveCombatant->CanAffordCost(Cost)) { bCanAfford = false; break; }
    }
    if (!bCanAfford)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BattleManager] FireGunAimShot: cannot afford cost."));
        EndGunAimMode();
        return;
    }

    // Line trace from the aim camera forward.
    FHitResult Hit;
    const FVector TraceStart = GunAimCameraActor->GetActorLocation();
    const FVector TraceEnd   = TraceStart + GunAimCameraActor->GetActorForwardVector() * 8000.f;

    FCollisionQueryParams Params(TEXT("GunShot"), /*bTraceComplex=*/false);
    Params.AddIgnoredActor(this);
    Params.AddIgnoredActor(ActiveCombatant);

    ACombatantBase* HitEnemy = nullptr;
    if (GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, Params))
    {
        if (ACombatantBase* HitC = Cast<ACombatantBase>(Hit.GetActor()))
        {
            if (HitC->GetTeam() == ECombatTeam::Enemy && !HitC->IsDead())
            {
                HitEnemy = HitC;
            }
        }
    }

    // Play gun animation.
    ActiveCombatant->PlayAbilityAnimation(EAbilityCategory::Gun);

    if (HitEnemy)
    {
        // Hit — run the full ability pipeline (handles cost + damage).
        UE_LOG(LogTemp, Log, TEXT("[BattleManager] Gun shot HIT: %s"), *HitEnemy->GetName());
        AM->TryActivateAbility(GunAbilIdx, { HitEnemy });
    }
    else
    {
        // Miss — only spend the resource costs.
        UE_LOG(LogTemp, Log, TEXT("[BattleManager] Gun shot MISSED."));
        for (const FAbilityCost& Cost : GunAbil->Costs)
        {
            ActiveCombatant->SpendResource(Cost.ResourceType, Cost.Amount);
        }
    }

    if (TryResolveBattleEnd()) { EndGunAimMode(); ActiveCombatant = nullptr; return; }

    // Check whether the player can afford another shot.
    // If not, exit aim mode so the HUD reflects the empty-AP state.
    // If yes, stay in aim mode — the player can keep shooting while holding RMB.
    bool bCanAffordNext = true;
    for (const FAbilityCost& Cost : GunAbil->Costs)
    {
        if (!ActiveCombatant->CanAffordCost(Cost)) { bCanAffordNext = false; break; }
    }
    if (!bCanAffordNext)
    {
        EndGunAimMode();
    }
    // GunShot has bEndsTurn = false → player stays in AwaitingInput.
}

// -----------------------------------------------------------------------------
//  (existing helpers continue below)
// -----------------------------------------------------------------------------

void ABattleManager::EnterTargetSelectionWithCandidates(ETargetScope Scope,
                                                         TArray<ACombatantBase*>&& Candidates)
{
    TargetCandidates.Empty();
    for (ACombatantBase* C : Candidates) { if (C) { TargetCandidates.Add(C); } }

    TargetCandidateIndex = 0;
    PendingScope = Scope;

    SetPhase(EBattlePhase::SelectingTarget);
    OnTargetSelectionChanged.Broadcast(GetCurrentTarget());

    // Move camera to the first highlighted target when targeting an enemy.
    if (PendingScope == ETargetScope::SingleEnemy)
    {
        FocusCameraOnEnemy(GetCurrentTarget(), 0.40f);
    }
}
