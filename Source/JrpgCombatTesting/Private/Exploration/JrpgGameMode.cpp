#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyEncounter.h"
#include "Core/BattleManager.h"
#include "Components/ProtocolManagerComponent.h"
#include "CombatTypes.h"
#include "Core/BattleArena.h"
#include "Characters/Base/CombatantBase.h"
#include "Characters/Player/PlayerCombatant.h"
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

// -----------------------------------------------------------------------------
//  Overworld protocol use
// -----------------------------------------------------------------------------

// Temporary on-screen feedback. Removed once the proper HUD lands.
static void ShowToast(const FString& Msg, FColor Color, float Duration = 2.5f)
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
    if (!BattleManager || !BattleManager->ProtocolManager)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Protocol] No ProtocolManager available."));
        ShowToast(TEXT("Heal failed: no ProtocolManager"), FColor::Red);
        return false;
    }

    UProtocolManagerComponent* Pool = BattleManager->ProtocolManager;
    if (!Pool->CanSpend(EProtocolType::Healing))
    {
        UE_LOG(LogTemp, Log, TEXT("[Protocol] No Healing charges remaining."));
        ShowToast(TEXT("No healing charges left"), FColor::Red);
        return false;
    }

    // Waste guard — refuse if everyone living is already full.
    bool bAnyNeedsHealing = false;
    for (ACombatantBase* P : PlayerParty)
    {
        if (P && P->GetCurrentHP() > 0.f && P->GetMissingHP() > 0.f)
        {
            bAnyNeedsHealing = true;
            break;
        }
    }
    if (!bAnyNeedsHealing)
    {
        UE_LOG(LogTemp, Log, TEXT("[Protocol] No one needs healing."));
        ShowToast(TEXT("Party is at full HP"), FColor::Yellow);
        return false;
    }

    for (ACombatantBase* P : PlayerParty)
    {
        if (!P || P->GetCurrentHP() <= 0.f) { continue; }
        const float Missing = P->GetMissingHP();
        if (Missing > 0.f)
        {
            P->ApplyHealing(Missing, nullptr);
            UE_LOG(LogTemp, Log, TEXT("[Protocol] Healed %s -> %.0f / %.0f"),
                *P->GetName(), P->GetCurrentHP(), P->GetMaxHP());
        }
    }

    Pool->SpendCharge(EProtocolType::Healing);
    const int32 Remaining = Pool->GetCurrentCharges(EProtocolType::Healing);
    const int32 Maximum   = Pool->GetMaxCharges(EProtocolType::Healing);
    UE_LOG(LogTemp, Log, TEXT("[Protocol] Healing charge spent. %d remaining."), Remaining);
    ShowToast(FString::Printf(TEXT("Party healed!  Heal charges: %d / %d"), Remaining, Maximum),
              FColor::Green);
    return true;
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
