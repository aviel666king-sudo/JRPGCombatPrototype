#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyEncounter.h"
#include "Core/BattleManager.h"
#include "Core/BattleArena.h"
#include "Characters/Base/CombatantBase.h"
#include "UI/CombatHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"  // TActorIterator

AJrpgGameMode::AJrpgGameMode()
{
    // Default to the exploration pawn. Blueprint subclasses can override this
    // (e.g. set DefaultPawnClass to BP_ExplorationPawn).
    DefaultPawnClass = AExplorationPawn::StaticClass();
}

void AJrpgGameMode::BeginPlay()
{
    Super::BeginPlay();

    // Cache the player's exploration pawn reference for show/hide later.
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        CachedExplorationPawn = Cast<AExplorationPawn>(PC->GetPawn());

        // Disable the engine's auto-camera-target system. UE will otherwise
        // try to auto-pick a CameraActor with "Auto-Activate For Player 0"
        // every time the controller's state changes, fighting our explicit
        // SetViewTarget calls. We manage view targets ourselves (BattleManager
        // for combat cameras, this GameMode for the exploration pawn).
        PC->bAutoManageActiveCameraTarget = false;
    }

    // Hide every combatant placed in the level at startup. Player party
    // members are revealed when an encounter triggers; orphan placed enemies
    // (leftover from the pre-encounter test setup) stay hidden forever.
    //
    // Critical fix: their collision was blocking the exploration gun trace,
    // making it impossible to actually shoot the encounter cube — the trace
    // would hit a placed combatant first and call Stun on the wrong actor
    // (or null-cast and do nothing).
    int32 HiddenCount = 0;
    for (TActorIterator<ACombatantBase> It(GetWorld()); It; ++It)
    {
        ACombatantBase* C = *It;
        if (!C) { continue; }
        C->SetActorHiddenInGame(true);
        C->SetActorEnableCollision(false);
        ++HiddenCount;
    }

    UE_LOG(LogTemp, Log, TEXT("[JrpgGameMode] BeginPlay. WorldMode = Exploring. Hid %d level combatants."),
        HiddenCount);
}

void AJrpgGameMode::BeginEncounter(AEnemyEncounter* Encounter, bool bPlayerHasInitiative)
{
    if (WorldMode == EWorldMode::InCombat)
    {
        UE_LOG(LogTemp, Warning, TEXT("[JrpgGameMode] BeginEncounter ignored — already in combat."));
        return;
    }
    if (!Encounter)
    {
        UE_LOG(LogTemp, Warning, TEXT("[JrpgGameMode] BeginEncounter called with null encounter."));
        return;
    }
    if (!BattleManager)
    {
        UE_LOG(LogTemp, Error, TEXT("[JrpgGameMode] BeginEncounter: no BattleManager set. "
                                     "Call SetBattleManager from the Level Blueprint."));
        return;
    }
    if (PlayerParty.Num() == 0)
    {
        UE_LOG(LogTemp, Error, TEXT("[JrpgGameMode] BeginEncounter: PlayerParty is empty. "
                                     "Call SetPlayerParty from the Level Blueprint."));
        return;
    }

    ABattleArena* Arena = Encounter->GetAssignedArena();
    if (!Arena)
    {
        UE_LOG(LogTemp, Error, TEXT("[JrpgGameMode] Encounter %s has no AssignedArena set."),
            *Encounter->GetName());
        return;
    }

    // ── 1. Snapshot exploration pawn position so we can return after combat ──
    if (CachedExplorationPawn)
    {
        PawnReturnLocation = CachedExplorationPawn->GetActorLocation();
        PawnReturnRotation = CachedExplorationPawn->GetActorRotation();

        // Hide & disable so the player can't move during combat.
        CachedExplorationPawn->SetActorHiddenInGame(true);
        CachedExplorationPawn->SetActorEnableCollision(false);
        if (UPawnMovementComponent* Move = CachedExplorationPawn->GetMovementComponent())
        {
            Move->StopMovementImmediately();
        }
    }

    // ── 2. Build the enemy roster (with merging if the player was caught) ────
    //
    //  Two-list build:
    //    BaseRoster      = triggerer's EnemyClasses (the "main" enemies)
    //    Reinforcements  = concatenation of every merged neighbour's
    //                       EliteVariantClasses (fallback: their EnemyClasses)
    //
    //  Final roster is then composed by PREFERRING reinforcements over
    //  duplicate base entries (so the elite always shows up if there's one)
    //  while ALWAYS reserving at least one slot for the triggerer so they
    //  appear in their own fight. Hard-capped at MaxMergedEnemies.
    //
    //  This makes the merge result independent of TActorIterator order — no
    //  more "last neighbour wins the swap slot" silent overwrites.
    TArray<TSubclassOf<ACombatantBase>> BaseRoster     = Encounter->GetEnemyClasses();
    TArray<TSubclassOf<ACombatantBase>> Reinforcements;
    MergedEncounters.Reset();

    if (!bPlayerHasInitiative && Encounter->bAllowMerging && GlobalMergeRadius > 0.f)
    {
        const FVector OriginLoc  = Encounter->GetActorLocation();
        const float   MergeRadSq = GlobalMergeRadius * GlobalMergeRadius;

        for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
        {
            AEnemyEncounter* Neighbour = *It;
            if (!Neighbour || Neighbour == Encounter) { continue; }
            if (!Neighbour->bAllowMerging)            { continue; }
            if (FVector::DistSquared(OriginLoc, Neighbour->GetActorLocation()) > MergeRadSq)
            {
                continue;
            }

            // What does THIS neighbour contribute? Its own elite-variant list
            // if authored, otherwise its base EnemyClasses. The contribution
            // belongs to the neighbour, NOT to the triggerer — that's what
            // makes the merge symmetric and predictable to author.
            const TArray<TSubclassOf<ACombatantBase>>& Contribution =
                Neighbour->EliteVariantClasses.Num() > 0
                    ? Neighbour->EliteVariantClasses
                    : Neighbour->GetEnemyClasses();

            if (Contribution.Num() == 0) { continue; }  // nothing to bring

            MergedEncounters.Add(Neighbour);

            for (TSubclassOf<ACombatantBase> ClsRef : Contribution)
            {
                if (ClsRef) { Reinforcements.Add(ClsRef); }
            }
        }
    }

    // Compose final roster.
    //  - Reserve 1 slot for the triggerer if it has any base classes
    //  - Fill reinforcement slots up to (Cap - ReservedBase)
    //  - Fill remaining slots with the rest of BaseRoster (so 0-reinforcement
    //    fights still use all the base classes)
    TArray<TSubclassOf<ACombatantBase>> Roster;
    {
        const int32 Cap            = MaxMergedEnemies;
        const int32 ReservedBase   = (BaseRoster.Num() > 0) ? 1 : 0;
        const int32 ReinSlotsAvail = FMath::Max(0, Cap - ReservedBase);
        const int32 ReinSlotsUsed  = FMath::Min(Reinforcements.Num(), ReinSlotsAvail);

        // Triggerer leads (slot 0). Skip if it has no base classes.
        if (ReservedBase > 0 && BaseRoster.IsValidIndex(0))
        {
            Roster.Add(BaseRoster[0]);
        }

        // Then reinforcements.
        for (int32 i = 0; i < ReinSlotsUsed; ++i)
        {
            Roster.Add(Reinforcements[i]);
        }

        // Then fill any leftover slots with the rest of the triggerer's base.
        for (int32 i = 1; i < BaseRoster.Num() && Roster.Num() < Cap; ++i)
        {
            if (BaseRoster[i]) { Roster.Add(BaseRoster[i]); }
        }
    }

    if (MergedEncounters.Num() > 0)
    {
        const int32 NumReinforcements = MergedEncounters.Num();
        UE_LOG(LogTemp, Warning,
            TEXT("[JrpgGameMode] AMBUSH — %d encounter(s) merged. Final roster size %d."),
            NumReinforcements, Roster.Num());

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 4.f, FColor(255, 40, 40),
                FString::Printf(TEXT("AMBUSH! %d reinforcement%s joined the fight"),
                    NumReinforcements, NumReinforcements == 1 ? TEXT("") : TEXT("s")),
                /*bNewerOnTop=*/true, FVector2D(1.6f, 1.6f));
        }
    }

    // ── 3. Spawn enemies from the (possibly merged) roster ───────────────────
    SpawnedEnemies.Reset();
    UWorld* World = GetWorld();
    if (World)
    {
        for (TSubclassOf<ACombatantBase> EnemyClass : Roster)
        {
            if (!EnemyClass) { continue; }

            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            // Spawn at the encounter's location for now — BattleManager teleports
            // them to the arena's enemy slots inside Phase_Initialize.
            ACombatantBase* Enemy = World->SpawnActor<ACombatantBase>(
                EnemyClass,
                Encounter->GetActorLocation(),
                Encounter->GetActorRotation(),
                Params);

            if (Enemy) { SpawnedEnemies.Add(Enemy); }
        }
    }

    // ── 3. Make sure player party actors are visible (in case they were hidden)
    for (ACombatantBase* P : PlayerParty)
    {
        if (P)
        {
            P->SetActorHiddenInGame(false);
            P->SetActorEnableCollision(true);
        }
    }

    // ── 4. Build untyped arrays for the BM API ───────────────────────────────
    TArray<ACombatantBase*> PlayerArray;
    PlayerArray.Reserve(PlayerParty.Num());
    for (TObjectPtr<ACombatantBase> P : PlayerParty) { PlayerArray.Add(P.Get()); }

    TArray<ACombatantBase*> EnemyArray;
    EnemyArray.Reserve(SpawnedEnemies.Num());
    for (TObjectPtr<ACombatantBase> E : SpawnedEnemies) { EnemyArray.Add(E.Get()); }

    // ── 5. Hook end-of-battle and kick off ──────────────────────────────────
    if (!BattleManager->OnBattleEnded.IsAlreadyBound(this, &AJrpgGameMode::HandleBattleEnded))
    {
        BattleManager->OnBattleEnded.AddDynamic(this, &AJrpgGameMode::HandleBattleEnded);
    }

    ActiveEncounter = Encounter;
    WorldMode       = EWorldMode::InCombat;

    // Freeze EVERY encounter in the world — including the active one. The
    // fight itself runs on spawned combatants at the arena, not on the
    // encounter actor, so the encounter has no business being visible (its
    // chase label was leaking into the arena view, and its mesh too).
    for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
    {
        if (AEnemyEncounter* Enc = *It)
        {
            Enc->SetExplorationActive(false);
        }
    }

    // Create the combat HUD widget. The old auto-start flow used to do this in
    // the Level Blueprint; now we own it here so the encounter system stays
    // self-contained — every fight gets a HUD without level-bp wiring.
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        BattleManager->CreateAndShowHUD(PC);
    }

    // ── Pick the first-strike combatant by speed ─────────────────────────────
    // bPlayerHasInitiative encodes who got the drop on whom:
    //  - true  → the fastest player party member acts first (cone shot,
    //            walked into a stunned enemy, or future stealth assassin)
    //  - false → the fastest enemy acts first (default contact ambush, per
    //            the GDD's "engages directly or is detected" rule)
    auto FastestOf = [](const TArray<ACombatantBase*>& Candidates) -> ACombatantBase*
    {
        ACombatantBase* Best = nullptr;
        float BestSpeed = -1.f;
        for (ACombatantBase* C : Candidates)
        {
            if (!C || C->IsDead()) { continue; }
            const float S = C->GetEffectiveSpeed();
            if (S > BestSpeed) { BestSpeed = S; Best = C; }
        }
        return Best;
    };

    ACombatantBase* PriorityCombatant = bPlayerHasInitiative
        ? FastestOf(PlayerArray)
        : FastestOf(EnemyArray);

    BattleManager->StartBattleAtArena(Arena, PlayerArray, EnemyArray, PriorityCombatant);

    UE_LOG(LogTemp, Log, TEXT("[JrpgGameMode] Encounter started. Party=%d  Enemies=%d  Arena=%s"),
        PlayerArray.Num(), EnemyArray.Num(), *Arena->GetName());
}

void AJrpgGameMode::HandleBattleEnded(bool bVictory)
{
    UE_LOG(LogTemp, Log, TEXT("[JrpgGameMode] Battle ended. Victory=%s"),
        bVictory ? TEXT("true") : TEXT("false"));

    if (bVictory)
    {
        // Tear down the combat HUD — it added itself to viewport and switched
        // input mode to GameAndUI; we have to undo both for exploration to
        // feel right (mouse hidden, input goes to the pawn).
        if (BattleManager && BattleManager->CombatHUD)
        {
            BattleManager->CombatHUD->RemoveFromParent();
            BattleManager->CombatHUD = nullptr;
        }
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->bShowMouseCursor = false;
        }

        // Destroy spawned enemies — they're gone from the world for good.
        for (TObjectPtr<ACombatantBase> Enemy : SpawnedEnemies)
        {
            if (Enemy) { Enemy->Destroy(); }
        }
        SpawnedEnemies.Reset();

        // Destroy the encounter actor so the player can't trigger it again.
        if (ActiveEncounter) { ActiveEncounter->Destroy(); }
        ActiveEncounter = nullptr;

        // Destroy every neighbour that was merged into this fight — they've
        // been consumed by the ambush. Doing this BEFORE the ResetToSpawn
        // sweep below means the iterator there naturally skips them.
        for (TObjectPtr<AEnemyEncounter> Merged : MergedEncounters)
        {
            if (Merged) { Merged->Destroy(); }
        }
        MergedEncounters.Reset();

        // Hide player party again — they'll show up at the next encounter.
        for (ACombatantBase* P : PlayerParty)
        {
            if (P)
            {
                P->SetActorHiddenInGame(true);
                P->SetActorEnableCollision(false);
            }
        }

        // Restore the exploration pawn at its pre-combat position.
        if (CachedExplorationPawn)
        {
            CachedExplorationPawn->SetActorLocationAndRotation(PawnReturnLocation, PawnReturnRotation);
            CachedExplorationPawn->SetActorHiddenInGame(false);
            CachedExplorationPawn->SetActorEnableCollision(true);

            // Switch the camera back from the arena camera to the pawn.
            if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
            {
                PC->SetViewTargetWithBlend(CachedExplorationPawn, 0.25f, VTBlend_Cubic);
            }
        }

        // Reset every surviving encounter back to spawn AND re-enable them —
        // any enemy that was mid-wander when the fight started is teleported
        // home so the player isn't immediately re-overlapped on exit.
        for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
        {
            AEnemyEncounter* Enc = *It;
            if (Enc)
            {
                Enc->ResetToSpawn();
                Enc->SetExplorationActive(true);
            }
        }

        WorldMode = EWorldMode::Exploring;
    }
    else
    {
        // Defeat — leave things as they are for now. A real implementation would
        // show a Game Over screen. TODO: add Game Over flow.
        UE_LOG(LogTemp, Warning, TEXT("[JrpgGameMode] Defeat — Game Over flow not implemented yet."));
    }
}
