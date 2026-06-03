#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyEncounter.h"
#include "Core/BattleManager.h"
#include "Components/ProtocolManagerComponent.h"
#include "CombatTypes.h"
#include "Core/BattleArena.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Player/PlayerCombatant.h"
#include "Characters/Enemy/EnemyCombatant.h"
#include "Core/DangerManager.h"
#include "Roster/RosterSubsystem.h"
#include "Persistence/WorldStateSubsystem.h"
#include "Travel/JrpgTravelSubsystem.h"
#include "Travel/TravelArrivalPoint.h"
#include "UI/DefeatScreenWidget.h"
#include "UI/VictoryScreenWidget.h"
#include "Misc/Paths.h"
#include "Equipment/CharacterWeaponDataAsset.h"
#include "Equipment/CharacterChipDataAsset.h"
#include "Equipment/CharacterArmorDataAsset.h"
#include "Equipment/CraftingMaterialDataAsset.h"
#include "Engine/GameInstance.h"
#include "UI/CombatHUDWidget.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"  // TActorIterator

// Defined further down — forward-declared so HandleBattleEnded can use it.
static void ShowToast(const FString& Msg, FColor Color, float Duration = 2.5f);

namespace
{
    /** Canonical short level name (strips PIE prefix + path). */
    FName CanonicalLevelName(const UWorld* World)
    {
        if (!World) { return NAME_None; }
        FString MapName = World->GetMapName();
        MapName.RemoveFromStart(World->StreamingLevelsPrefix);
        return FName(*FPaths::GetBaseFilename(MapName));
    }
}

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

    // Camp/hub has no placed party — spawn it from records next tick (after the
    // level BP has had a chance to call SetPlayerParty in combat levels).
    GetWorldTimerManager().SetTimerForNextTick(this, &AJrpgGameMode::SpawnPartyFromRecordsIfNeeded);
}

void AJrpgGameMode::SpawnPartyFromRecordsIfNeeded()
{
    if (PlayerParty.Num() > 0) { return; }   // a level placed its own party

    UGameInstance* GI = GetGameInstance();
    URosterSubsystem* Roster = GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
    UJrpgTravelSubsystem* Travel = GI ? GI->GetSubsystem<UJrpgTravelSubsystem>() : nullptr;
    UWorld* World = GetWorld();
    if (!Roster || !Roster->IsSeeded() || !Travel || !World) { return; }

    // Only in the camp hub — combat levels place their own party.
    if (CanonicalLevelName(World) != Travel->CampLevelName) { return; }

    FVector SpawnLoc = CachedExplorationPawn ? CachedExplorationPawn->GetActorLocation()
                                             : FVector::ZeroVector;

    TArray<ACombatantBase*> Spawned;
    for (int32 i = 0; i < Roster->GetMemberCount(); ++i)
    {
        const FPartyMemberRecord& Rec = Roster->GetMember(i);
        if (!Rec.CharacterClass) { continue; }

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        if (APlayerCombatant* PC = World->SpawnActor<APlayerCombatant>(
                Rec.CharacterClass, SpawnLoc, FRotator::ZeroRotator, Params))
        {
            PC->SetActorHiddenInGame(true);
            PC->SetActorEnableCollision(false);
            Spawned.Add(PC);
        }
    }

    if (Spawned.Num() > 0)
    {
        SetPlayerParty(Spawned);   // seeds (no-op) + restores records (incl skills)
        UE_LOG(LogTemp, Log, TEXT("[JrpgGameMode] Camp: spawned %d party members from records."),
            Spawned.Num());
    }
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
    //  Elite-slot model: every encounter has 3 base enemies (EnemyClasses) +
    //  1 elite (EliteEnemyClass) that's reserved EXCLUSIVELY for merged
    //  fights. In a solo fight the elite stays hidden.
    //
    //  When merging fires we collect the elite from EVERY participating
    //  encounter (triggerer + each merged neighbour). The final roster is:
    //    No merge   → 3 base (elite stays hidden)
    //    Any merge  → all participating elites first, then triggerer's base
    //                 fills any leftover slots. Capped at MaxMergedEnemies.
    //
    //  Worked examples (Cap = 3):
    //    1 encounter, no merge   → [base0, base1, base2]
    //    2 encounters (1 merge)  → [elite_A, elite_B, base0]
    //    3 encounters (2 merges) → [elite_A, elite_B, elite_C]
    //    4+ encounters           → first 3 elites (rest dropped)
    TArray<TSubclassOf<ACombatantBase>> ParticipatingElites;
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

            // Neighbour is consumed regardless of whether it has an elite set.
            MergedEncounters.Add(Neighbour);

            // Contribute its elite if authored. Null elite = encounter is
            // consumed but adds no enemy to the fight.
            if (Neighbour->EliteEnemyClass)
            {
                ParticipatingElites.Add(Neighbour->EliteEnemyClass);
            }
        }

        // If any merge happened, the triggerer's elite joins too.
        if (MergedEncounters.Num() > 0 && Encounter->EliteEnemyClass)
        {
            // Triggerer's elite goes first so it leads visually in the slot list.
            ParticipatingElites.Insert(Encounter->EliteEnemyClass, 0);
        }
    }

    // Compose final roster.
    TArray<TSubclassOf<ACombatantBase>> Roster;
    const bool bAnyMerge = (MergedEncounters.Num() > 0);

    if (!bAnyMerge)
    {
        // Solo fight — base enemies only, elite stays hidden.
        for (TSubclassOf<ACombatantBase> Cls : Encounter->GetEnemyClasses())
        {
            if (Roster.Num() >= MaxMergedEnemies) { break; }
            if (Cls) { Roster.Add(Cls); }
        }
    }
    else
    {
        // Merged fight — elites first.
        for (TSubclassOf<ACombatantBase> Cls : ParticipatingElites)
        {
            if (Roster.Num() >= MaxMergedEnemies) { break; }
            if (Cls) { Roster.Add(Cls); }
        }
        // Then fill remaining slots with triggerer's base.
        for (TSubclassOf<ACombatantBase> Cls : Encounter->GetEnemyClasses())
        {
            if (Roster.Num() >= MaxMergedEnemies) { break; }
            if (Cls) { Roster.Add(Cls); }
        }
    }

    if (bAnyMerge)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[JrpgGameMode] AMBUSH — %d encounter(s) merged. Elites in fight: %d. Roster size %d."),
            MergedEncounters.Num(), ParticipatingElites.Num(), Roster.Num());
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

    ActiveEncounter         = Encounter;
    bLastEncounterInitiative = bPlayerHasInitiative;
    WorldMode               = EWorldMode::InCombat;

    // Snapshot party HP + shared charges so a post-defeat Retry restores the
    // exact conditions this fight began with.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URosterSubsystem* RosterSub = GI->GetSubsystem<URosterSubsystem>())
        {
            RosterSub->SnapshotBattleEntry(PlayerParty);
        }
    }

    // Snapshot each member's level + within-level XP so the victory screen can
    // animate the bar from the pre-fight state and flag level-ups.
    PreLevels.Reset();
    PreXP.Reset();
    for (ACombatantBase* P : PlayerParty)
    {
        const APlayerCombatant* PC = Cast<APlayerCombatant>(P);
        PreLevels.Add(PC ? PC->Level : 1);
        PreXP.Add(PC ? PC->CurrentXP : 0);
    }

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
        // Result accumulators for the victory screen.
        TArray<FVictoryMemberXP> VictoryMembers;
        TArray<FString> SpoilLines;
        TArray<FString> LootNames;
        int32 GoldGained = 0, MaterialGained = 0;

        // ── Award drops to the persistent roster BEFORE we destroy the
        //    encounter + spawned enemies below. ─────────────────────────────
        if (UGameInstance* GI = GetGameInstance())
        {
            if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
            {
                const int32 NumEnemies = FMath::Max(1, SpawnedEnemies.Num());
                GoldGained = NumEnemies * GoldPerEnemy;
                Roster->AddGold(GoldGained);
                if (DefaultDropMaterial)
                {
                    MaterialGained = NumEnemies * MaterialPerEnemy;
                    Roster->AddMaterial(DefaultDropMaterial, MaterialGained);
                }

                auto AwardDrops = [&](AEnemyEncounter* Enc)
                {
                    if (!Enc) { return; }
                    // Only award items the player doesn't already own — no dups.
                    if (Enc->WeaponDrop && !Roster->OwnsWeapon(Enc->WeaponDrop))
                    { Roster->AddOwnedWeapon(Enc->WeaponDrop); LootNames.Add(Enc->WeaponDrop->DisplayName.ToString()); }
                    for (const TObjectPtr<UCharacterChipDataAsset>& C : Enc->ChipDrops)
                    { if (C && !Roster->OwnsChip(C)) { Roster->AddOwnedChip(C); LootNames.Add(C->DisplayName.ToString()); } }
                    for (const TObjectPtr<UCharacterArmorDataAsset>& A : Enc->ArmorDrops)
                    { if (A && !Roster->OwnsArmor(A)) { Roster->AddOwnedArmor(A); LootNames.Add(A->DisplayName.ToString()); } }
                };
                AwardDrops(ActiveEncounter);
                for (const TObjectPtr<AEnemyEncounter>& M : MergedEncounters) { AwardDrops(M); }
            }
        }

        // ── Build the victory-screen data. XP is granted equally across the
        //    party in the BattleManager; recompute that same value from the
        //    slain enemies (still alive here) so the bar can replay the fill
        //    from each member's pre-fight level + XP. ─────────────────────────
        {
            int32 TotalXP = 0;
            for (const TObjectPtr<ACombatantBase>& E : SpawnedEnemies)
            {
                if (const AEnemyCombatant* En = Cast<AEnemyCombatant>(E.Get()))
                { TotalXP += FMath::Max(0, En->XPReward); }
            }
            const int32 PerMember = TotalXP / FMath::Max(1, PlayerParty.Num());

            for (int32 i = 0; i < PlayerParty.Num(); ++i)
            {
                APlayerCombatant* P = Cast<APlayerCombatant>(PlayerParty[i]);
                if (!P) { continue; }

                FVictoryMemberXP Row;
                Row.Name       = P->DisplayName.IsEmpty()
                    ? FText::FromString(P->GetName()) : P->DisplayName;
                Row.StartLevel = PreLevels.IsValidIndex(i) ? PreLevels[i] : P->Level;
                Row.StartXP    = PreXP.IsValidIndex(i) ? PreXP[i] : 0;
                Row.XPGained   = PerMember;

                // Thresholds for each level the bar might roll through, ending
                // on the in-progress level (so the last entry is the bar's cap).
                int32 Remaining = Row.StartXP + Row.XPGained;
                int32 L = Row.StartLevel;
                do
                {
                    const int32 Thr = APlayerCombatant::XPRequiredForLevel(L);
                    Row.Thresholds.Add(Thr);
                    Remaining -= Thr;
                    ++L;
                } while (Remaining >= 0 && Row.Thresholds.Num() < 50);

                VictoryMembers.Add(MoveTemp(Row));
            }

            SpoilLines.Add(FString::Printf(TEXT("Gold:  +%d"), GoldGained));
            if (MaterialGained > 0)
            {
                SpoilLines.Add(FString::Printf(TEXT("%s:  +%d"),
                    DefaultDropMaterial ? *DefaultDropMaterial->DisplayName.ToString() : TEXT("Material"),
                    MaterialGained));
            }
            if (LootNames.Num() > 0)
            {
                SpoilLines.Add(FString::Printf(TEXT("Loot:  %s"), *FString::Join(LootNames, TEXT(", "))));
            }
        }

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

        // Destroy spawned combat enemies — they only exist for the fight.
        for (TObjectPtr<ACombatantBase> Enemy : SpawnedEnemies)
        {
            if (Enemy) { Enemy->Destroy(); }
        }
        SpawnedEnemies.Reset();

        // Retire the world encounter(s): bosses (one-time) are recorded + gone
        // for good; regular encounters are disabled and parked in
        // DefeatedEncounters so a checkpoint Rest can respawn them (Souls-like).
        UWorldStateSubsystem* WorldState = GetGameInstance()
            ? GetGameInstance()->GetSubsystem<UWorldStateSubsystem>() : nullptr;
        auto RetireEncounter = [&](AEnemyEncounter* Enc)
        {
            if (!Enc) { return; }
            if (Enc->bOneTimeEncounter)
            {
                if (WorldState) { WorldState->MarkDone(Enc->GetPersistentKey()); }
                Enc->Destroy();
            }
            else
            {
                Enc->SetExplorationActive(false);   // hidden + inert until rest
                DefeatedEncounters.AddUnique(Enc);
            }
        };
        RetireEncounter(ActiveEncounter);
        ActiveEncounter = nullptr;
        for (TObjectPtr<AEnemyEncounter> Merged : MergedEncounters)
        {
            RetireEncounter(Merged);
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
        // home so the player isn't immediately re-overlapped on exit. Skip the
        // ones we just defeated: they stay parked until the next rest.
        for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
        {
            AEnemyEncounter* Enc = *It;
            if (Enc && !DefeatedEncounters.Contains(Enc))
            {
                Enc->ResetToSpawn();
                Enc->SetExplorationActive(true);
            }
        }

        // Keep WorldMode == InCombat while the results panel is up: the world
        // still ticks (so the XP bar animates), but encounters can't start a
        // new fight (BeginEncounter guards on InCombat). Continue flips it back
        // to Exploring. The panel sets UI-only input so the pawn can't move.
        ShowVictoryScreen(VictoryMembers, SpoilLines);
    }
    else
    {
        // Defeat — tear down the combat HUD and show the Retry / Give Up screen.
        // We deliberately DON'T destroy the enemies or the encounter here: Retry
        // re-runs the same fight, Give Up reloads from the checkpoint (which
        // resets the level anyway).
        if (BattleManager && BattleManager->CombatHUD)
        {
            BattleManager->CombatHUD->RemoveFromParent();
            BattleManager->CombatHUD = nullptr;
        }
        ShowDefeatScreen();
    }
}

void AJrpgGameMode::RespawnDefeatedEncounters()
{
    for (TObjectPtr<AEnemyEncounter> Enc : DefeatedEncounters)
    {
        if (Enc)
        {
            Enc->ResetToSpawn();
            Enc->SetExplorationActive(true);
        }
    }
    DefeatedEncounters.Reset();
}

// -----------------------------------------------------------------------------
//  Defeat flow — Retry / Give Up
// -----------------------------------------------------------------------------

void AJrpgGameMode::ShowDefeatScreen()
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) { return; }

    UClass* WidgetClass = DefeatWidgetClass ? DefeatWidgetClass.Get() : UDefeatScreenWidget::StaticClass();
    DefeatWidget = CreateWidget<UDefeatScreenWidget>(PC, WidgetClass);
    if (!DefeatWidget) { return; }

    // Tell the player where Give Up will send them.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
        {
            if (Roster->HasLastRested())
            {
                DefeatWidget->GiveUpLabel = FText::FromString(FString::Printf(
                    TEXT("Return to %s"), *Roster->GetLastRestedCheckpointId().ToString()));
            }
        }
    }

    DefeatWidget->OnRetryRequested  = [this]() { RetryBattle(); };
    DefeatWidget->OnGiveUpRequested = [this]() { GiveUpToLastCheckpoint(); };
    DefeatWidget->AddToViewport(100);

    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(DefeatWidget->TakeWidget());
    PC->SetInputMode(Mode);
}

void AJrpgGameMode::DismissDefeatScreen()
{
    if (DefeatWidget)
    {
        DefeatWidget->RemoveFromParent();
        DefeatWidget = nullptr;
    }
    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AJrpgGameMode::ShowVictoryScreen(const TArray<FVictoryMemberXP>& Members,
                                     const TArray<FString>& SpoilLines)
{
    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) { return; }

    UClass* WidgetClass = VictoryWidgetClass ? VictoryWidgetClass.Get() : UVictoryScreenWidget::StaticClass();
    VictoryWidget = CreateWidget<UVictoryScreenWidget>(PC, WidgetClass);
    if (!VictoryWidget) { ContinueAfterVictory(); return; }

    VictoryWidget->Members    = Members;
    VictoryWidget->SpoilLines = SpoilLines;
    VictoryWidget->OnContinueRequested = [this]() { ContinueAfterVictory(); };
    VictoryWidget->AddToViewport(100);

    PC->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(VictoryWidget->TakeWidget());
    PC->SetInputMode(Mode);
}

void AJrpgGameMode::ContinueAfterVictory()
{
    if (VictoryWidget)
    {
        VictoryWidget->RemoveFromParent();
        VictoryWidget = nullptr;
    }

    // Now hand control back to exploration.
    WorldMode = EWorldMode::Exploring;

    if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->bShowMouseCursor = false;
        PC->SetInputMode(FInputModeGameOnly());
    }
}

void AJrpgGameMode::RetryBattle()
{
    if (!ActiveEncounter) { return; }

    DismissDefeatScreen();

    // Clear out the corpses from the lost fight; BeginEncounter spawns fresh.
    for (TObjectPtr<ACombatantBase> Enemy : SpawnedEnemies)
    {
        if (Enemy) { Enemy->Destroy(); }
    }
    SpawnedEnemies.Reset();

    // Restore the exact party HP + charges captured when the fight began.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
        {
            Roster->RestoreBattleEntry(PlayerParty);
        }
    }

    AEnemyEncounter* Encounter = ActiveEncounter;
    const bool bInitiative = bLastEncounterInitiative;

    // BeginEncounter early-returns while WorldMode == InCombat; drop back to
    // Exploring so the replay runs.
    WorldMode = EWorldMode::Exploring;
    BeginEncounter(Encounter, bInitiative);
}

void AJrpgGameMode::GiveUpToLastCheckpoint()
{
    DismissDefeatScreen();

    UWorld* World = GetWorld();
    UGameInstance* GI = GetGameInstance();
    URosterSubsystem* Roster = GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
    UJrpgTravelSubsystem* Travel = GI ? GI->GetSubsystem<UJrpgTravelSubsystem>() : nullptr;

    // Full heal (records + any live actors) before the reload carries it over.
    if (Roster) { Roster->RestockAndHeal(PlayerParty); }

    const FName CurrentLevel = CanonicalLevelName(World);
    WorldMode = EWorldMode::Exploring;

    // Respawn rule (per-level, Souls-like):
    //   - Rested at a checkpoint IN THIS LEVEL → reload there.
    //   - Otherwise → reload at the level's entry arrival point (the spawn by
    //     the portal you came in through), falling back to PlayerStart.
    const bool bRestedHere = Roster && Roster->HasLastRested()
        && Roster->GetLastRestedLevel() == CurrentLevel;

    if (!Travel)
    {
        UGameplayStatics::OpenLevel(this, CurrentLevel);
        return;
    }

    if (bRestedHere)
    {
        Travel->TravelToLevelAtTransform(CurrentLevel, Roster->GetLastRestedTransform());
        return;
    }

    // No rest in this level — find the entry arrival point's transform (stable
    // across reload since it's placed in the level) and land there.
    FTransform EntryXf;
    bool bFoundEntry = false;
    for (TActorIterator<ATravelArrivalPoint> It(World); It; ++It)
    {
        if (*It) { EntryXf = (*It)->GetActorTransform(); bFoundEntry = true; break; }
    }

    if (bFoundEntry) { Travel->TravelToLevelAtTransform(CurrentLevel, EntryXf); }
    else             { Travel->TravelToLevel(CurrentLevel, NAME_None); }  // PlayerStart fallback
}

void AJrpgGameMode::AwardAssassinationRewards(AEnemyEncounter* Encounter)
{
    if (!Encounter) { return; }

    // Random cut of the would-be rewards: 20–30% this kill.
    const float Frac = FMath::FRandRange(0.20f, 0.30f);

    const int32 NumEnemies = FMath::Max(1, Encounter->GetEnemyClasses().Num());

    // ── XP (reduced, lead member only — a stealth strike, not a party fight) ──
    int32 TotalXP = 0;
    for (TSubclassOf<ACombatantBase> EnemyClass : Encounter->GetEnemyClasses())
    {
        if (!EnemyClass) { continue; }
        if (const AEnemyCombatant* CDO = EnemyClass->GetDefaultObject<AEnemyCombatant>())
        {
            TotalXP += FMath::Max(0, CDO->XPReward);
        }
    }
    const int32 ReducedXP = FMath::FloorToInt(TotalXP * Frac);
    if (ReducedXP > 0 && PlayerParty.Num() > 0)
    {
        if (APlayerCombatant* Lead = Cast<APlayerCombatant>(PlayerParty[0]))
        {
            Lead->GrantXP(ReducedXP);
        }
    }

    // ── Gold + material (reduced) and FULL loot drops ────────────────────────
    int32 GoldAward = 0, MatAward = 0, Dropped = 0;
    UGameInstance* GI = GetGameInstance();
    URosterSubsystem* Roster = GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
    if (Roster)
    {
        GoldAward = FMath::FloorToInt(NumEnemies * GoldPerEnemy * Frac);
        Roster->AddGold(GoldAward);

        if (DefaultDropMaterial)
        {
            MatAward = FMath::FloorToInt(NumEnemies * MaterialPerEnemy * Frac);
            Roster->AddMaterial(DefaultDropMaterial, MatAward);
        }

        // Loot drops are FULL — a stealth kill still yields the bound item
        // (skip anything already owned to avoid duplicates).
        if (Encounter->WeaponDrop && !Roster->OwnsWeapon(Encounter->WeaponDrop))
        { Roster->AddOwnedWeapon(Encounter->WeaponDrop); ++Dropped; }
        for (const TObjectPtr<UCharacterChipDataAsset>& C : Encounter->ChipDrops)
        { if (C && !Roster->OwnsChip(C)) { Roster->AddOwnedChip(C); ++Dropped; } }
        for (const TObjectPtr<UCharacterArmorDataAsset>& A : Encounter->ArmorDrops)
        { if (A && !Roster->OwnsArmor(A)) { Roster->AddOwnedArmor(A); ++Dropped; } }
    }

    FString Msg = FString::Printf(TEXT("Assassination!  +%d XP   +%d Gold   +%d %s  (%.0f%%)"),
        ReducedXP, GoldAward, MatAward,
        DefaultDropMaterial ? *DefaultDropMaterial->DisplayName.ToString() : TEXT("Material"),
        Frac * 100.f);
    if (Dropped > 0) { Msg += TEXT("   + loot!"); }
    ShowToast(Msg, FColor::Cyan);

    // Recompute aggregate party level in case the lead leveled up.
    if (UDangerManager* DM = GI ? GI->GetSubsystem<UDangerManager>() : nullptr)
    {
        int32 MaxLevel = 1;
        for (ACombatantBase* C : PlayerParty)
        {
            if (APlayerCombatant* P = Cast<APlayerCombatant>(C))
            {
                MaxLevel = FMath::Max(MaxLevel, P->Level);
            }
        }
        DM->SetPlayerEffectiveLevel(MaxLevel);
    }
}

// -----------------------------------------------------------------------------
//  Overworld protocol use
// -----------------------------------------------------------------------------

// Temporary on-screen feedback. Removed once the proper HUD lands.
static void ShowToast(const FString& Msg, FColor Color, float Duration)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, Duration, Color, Msg);
    }
}

bool AJrpgGameMode::UseHealingProtocolOverworld()
{
    if (WorldMode != EWorldMode::Exploring)
    {
        UE_LOG(LogTemp, Log, TEXT("[Protocol] Cannot use healing protocol during combat."));
        return false;
    }

    // Healing now spends from the PERSISTENT party charge pool (URosterSubsystem),
    // so it works in any level — including the Open World / Camp, where there's
    // no placed BattleManager. PlayerParty may be empty in those levels; the
    // subsystem just heals the records in that case.
    UGameInstance* GI = GetGameInstance();
    URosterSubsystem* Roster = GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
    if (!Roster)
    {
        ShowToast(TEXT("Heal failed: no roster"), FColor::Red);
        return false;
    }

    const bool bHealed = Roster->UseHealingCharge(PlayerParty);
    if (bHealed)
    {
        ShowToast(FString::Printf(TEXT("Party healed!  Heal charges: %d / %d"),
            Roster->GetHealCharges(), Roster->GetMaxHealCharges()), FColor::Green);
    }
    else if (Roster->GetHealCharges() <= 0)
    {
        ShowToast(TEXT("No healing charges left"), FColor::Red);
    }
    else
    {
        ShowToast(TEXT("Party is at full HP"), FColor::Yellow);
    }
    return bHealed;
}

void AJrpgGameMode::SetPlayerParty(const TArray<ACombatantBase*>& InParty)
{
    PlayerParty = InParty;

    // Populate each member's HP/AP resource pools up front so the overworld HUD
    // shows real values (full HP) before the first battle. InitializeForBattle
    // is HP-persistent, so this is safe to call here and again at battle start.
    for (ACombatantBase* Member : PlayerParty)
    {
        if (Member) { Member->InitializeForBattle(); }
    }

    // Persistent party store: seed once from this level's placed party, then
    // restore any HP saved from a previous level so HP carries across travel.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
        {
            Roster->SeedFromParty(PlayerParty);      // no-op after the first time
            Roster->RestoreHPToParty(PlayerParty);   // applies saved HP
            Roster->SetPrimaryMaterial(DefaultDropMaterial);
        }
    }
}

void AJrpgGameMode::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Save live party HP back into the persistent records before this level is
    // torn down (e.g. on travel), so the next level / the roster screen sees the
    // up-to-date HP.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
        {
            Roster->SaveHPFromParty(PlayerParty);
        }
    }

    Super::EndPlay(EndPlayReason);
}

// -----------------------------------------------------------------------------
//  HUD data getters
// -----------------------------------------------------------------------------

bool AJrpgGameMode::IsPartyMemberValid(int32 Index) const
{
    return PlayerParty.IsValidIndex(Index) && PlayerParty[Index] != nullptr;
}

FText AJrpgGameMode::GetPartyMemberName(int32 Index) const
{
    if (!IsPartyMemberValid(Index)) { return FText::GetEmpty(); }
    return PlayerParty[Index]->DisplayName;
}

int32 AJrpgGameMode::GetPartyMemberLevel(int32 Index) const
{
    if (!IsPartyMemberValid(Index)) { return 0; }
    if (const APlayerCombatant* P = Cast<APlayerCombatant>(PlayerParty[Index]))
    {
        return P->Level;
    }
    return 0;
}

float AJrpgGameMode::GetPartyMemberHPPercent(int32 Index) const
{
    if (!IsPartyMemberValid(Index)) { return 0.f; }
    return PlayerParty[Index]->GetHealthPercent();
}

FText AJrpgGameMode::GetPartyMemberHPText(int32 Index) const
{
    if (!IsPartyMemberValid(Index)) { return FText::GetEmpty(); }
    const ACombatantBase* C = PlayerParty[Index];
    return FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),
        C->GetCurrentHP(), C->GetMaxHP()));
}

bool AJrpgGameMode::IsPartyMemberDead(int32 Index) const
{
    if (!IsPartyMemberValid(Index)) { return false; }
    return PlayerParty[Index]->GetCurrentHP() <= 0.f;
}

int32 AJrpgGameMode::GetProtocolCharges(EProtocolType Type) const
{
    if (BattleManager && BattleManager->ProtocolManager)
    {
        return BattleManager->ProtocolManager->GetCurrentCharges(Type);
    }
    return 0;
}

int32 AJrpgGameMode::GetProtocolMaxCharges(EProtocolType Type) const
{
    if (BattleManager && BattleManager->ProtocolManager)
    {
        return BattleManager->ProtocolManager->GetMaxCharges(Type);
    }
    return 0;
}

FText AJrpgGameMode::GetProtocolChargesText(EProtocolType Type) const
{
    FString Label;
    switch (Type)
    {
    case EProtocolType::Healing: Label = TEXT("Heal");    break;
    case EProtocolType::Revival: Label = TEXT("Revive");  break;
    case EProtocolType::AP:      Label = TEXT("AP");       break;
    default:                     Label = TEXT("?");        break;
    }
    return FText::FromString(FString::Printf(TEXT("%s %d/%d"),
        *Label, GetProtocolCharges(Type), GetProtocolMaxCharges(Type)));
}
