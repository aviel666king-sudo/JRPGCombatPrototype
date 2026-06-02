#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "CombatTypes.h"  // EProtocolType
#include "UI/VictoryScreenWidget.h"  // FVictoryMemberXP
#include "JrpgGameMode.generated.h"

class ACombatantBase;
class ABattleManager;
class ABattleArena;
class AEnemyEncounter;
class AExplorationPawn;
class UCraftingMaterialDataAsset;

UENUM(BlueprintType)
enum class EWorldMode : uint8
{
    Exploring  UMETA(DisplayName = "Exploring"),
    InCombat   UMETA(DisplayName = "In Combat"),
};

/**
 * AJrpgGameMode
 *
 * Owns the high-level mode of the game:
 *   - Exploring: AExplorationPawn is possessed, the player walks around
 *   - InCombat:  AExplorationPawn is hidden + disabled, ABattleManager runs
 *
 * Holds references to the placed party-combatant actors (BP_PlayerFencer etc.)
 * and the placed BP_BattleManager. AEnemyEncounter actors call BeginEncounter()
 * when the player overlaps them.
 *
 * Configure all references in the BP_JrpgGameMode Class Defaults, OR assign
 * them in the level via a simple level-blueprint setup that calls the
 * Set* helpers below. The level-blueprint approach is recommended because
 * the references are level-specific actors.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBATTESTING_API AJrpgGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:

    AJrpgGameMode();

    // -------------------------------------------------------------------------
    //  References — set these from the Level Blueprint (BeginPlay) since they
    //  point at actors placed in the specific level.
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "JRPG|Setup")
    void SetBattleManager(ABattleManager* InBattleManager) { BattleManager = InBattleManager; }

    UFUNCTION(BlueprintCallable, Category = "JRPG|Setup")
    void SetPlayerParty(const TArray<ACombatantBase*>& InParty);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|Setup")
    ABattleManager* GetBattleManager() const { return BattleManager; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|Setup")
    const TArray<ACombatantBase*>& GetPlayerParty() const { return PlayerParty; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|State")
    EWorldMode GetWorldMode() const { return WorldMode; }

    // -------------------------------------------------------------------------
    //  Encounter flow
    // -------------------------------------------------------------------------

    /**
     * Called by AEnemyEncounter when the player overlaps it.
     *
     * bPlayerHasInitiative = true means the player got the drop on the enemy
     * (they walked into a stunned encounter, or landed a cone shot). The
     * fastest player party member then acts first. Otherwise the fastest
     * enemy gets the first turn (default ambush behaviour).
     *
     * Steps:
     *   1. Snapshot pawn position so we can return after combat
     *   2. Hide / disable the exploration pawn
     *   3. Spawn enemies from the encounter's class list at arena enemy slots
     *   4. Show the player party at arena player slots
     *   5. Pick the priority combatant by speed
     *   6. Call BattleManager->StartBattleAtArena()
     */
    UFUNCTION(BlueprintCallable, Category = "JRPG|Encounter")
    void BeginEncounter(AEnemyEncounter* Encounter, bool bPlayerHasInitiative = false);

    /**
     * Reward a stealth instant-kill. Grants a RANDOM 20–30% of the encounter's
     * XP (lead member only) and of its gold + material, and drops the
     * encounter's loot (weapon / chips / armor) in FULL — owned items are
     * skipped so there are no duplicates. Called by AEnemyEncounter::Assassinate
     * right before the encounter is destroyed.
     */
    UFUNCTION(BlueprintCallable, Category = "JRPG|Encounter")
    void AwardAssassinationRewards(AEnemyEncounter* Encounter);

    /** Post-defeat: restart the SAME fight with the exact entry conditions
     *  (party HP + charges snapshotted when the fight began). */
    UFUNCTION(BlueprintCallable, Category = "JRPG|Encounter")
    void RetryBattle();

    /** Post-defeat: heal up and reload from the last rested checkpoint. */
    UFUNCTION(BlueprintCallable, Category = "JRPG|Encounter")
    void GiveUpToLastCheckpoint();

    /** Re-activate every regular encounter defeated since the last rest
     *  (Souls-like respawn). Called by ACheckpoint::Rest. Bosses stay dead. */
    UFUNCTION(BlueprintCallable, Category = "JRPG|Encounter")
    void RespawnDefeatedEncounters();

    // -------------------------------------------------------------------------
    //  Overworld protocol use
    // -------------------------------------------------------------------------

    /**
     * Spend one Healing Protocol charge to fully restore every living party
     * member to MaxHP. Bound to H by ExplorationPawn.
     *
     * Returns true if the charge was spent. Returns false (no charge spent) if:
     *   - Battle is active
     *   - ProtocolManager unavailable
     *   - No Healing charges remaining
     *   - All party members already at full HP (waste guard)
     *
     * NOTE: Dead members are NOT revived by overworld heal — that's what the
     * Revival protocol / camp rest / win auto-revive is for.
     */
    UFUNCTION(BlueprintCallable, Category = "JRPG|Protocols")
    bool UseHealingProtocolOverworld();

    // -------------------------------------------------------------------------
    //  HUD data getters
    //
    //  Indexed accessors so WBP_ExplorationHUD can bind each party slot / charge
    //  counter directly with no struct-breaking in the graph. Slot index 0..2.
    //  All are safe to call with out-of-range indices (return empty / 0).
    // -------------------------------------------------------------------------

    /** Number of party members currently registered. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    int32 GetPartySize() const { return PlayerParty.Num(); }

    /** True if a living-or-dead member exists at this slot. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    bool IsPartyMemberValid(int32 Index) const;

    /** Member display name, or empty text if the slot is invalid. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    FText GetPartyMemberName(int32 Index) const;

    /** Member level, or 0 if invalid / not a player combatant. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    int32 GetPartyMemberLevel(int32 Index) const;

    /** 0..1 HP fraction for a ProgressBar. 0 if invalid. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    float GetPartyMemberHPPercent(int32 Index) const;

    /** "84 / 120" style HP label. Empty if invalid. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    FText GetPartyMemberHPText(int32 Index) const;

    /** True if this member is downed (0 HP). HUD can grey the row / show a skull. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    bool IsPartyMemberDead(int32 Index) const;

    /** Current charges of a protocol in the shared party pool. 0 if no manager. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    int32 GetProtocolCharges(EProtocolType Type) const;

    /** Max charges of a protocol. 0 if no manager. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    int32 GetProtocolMaxCharges(EProtocolType Type) const;

    /** "Heal 2/2" style label for a protocol counter. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "JRPG|HUD")
    FText GetProtocolChargesText(EProtocolType Type) const;

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** Bound to BattleManager->OnBattleEnded — handles cleanup + return to exploration. */
    UFUNCTION()
    void HandleBattleEnded(bool bVictory);

    // -------------------------------------------------------------------------
    //  References (set by Level Blueprint via Set* helpers)
    // -------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "JRPG|Refs")
    TObjectPtr<ABattleManager> BattleManager;

    UPROPERTY(BlueprintReadOnly, Category = "JRPG|Refs")
    TArray<TObjectPtr<ACombatantBase>> PlayerParty;

    // -------------------------------------------------------------------------
    //  State
    // -------------------------------------------------------------------------

    UPROPERTY(BlueprintReadOnly, Category = "JRPG|State")
    EWorldMode WorldMode = EWorldMode::Exploring;

    /** The encounter that triggered the current combat. Cleared on victory. */
    UPROPERTY()
    TObjectPtr<AEnemyEncounter> ActiveEncounter;

    /** Initiative of the current/last fight — replayed verbatim on Retry. */
    bool bLastEncounterInitiative = false;

    /** Each member's level + within-level XP at fight start — drives the
     *  animated victory XP bar / level-up callouts. */
    TArray<int32> PreLevels;
    TArray<int32> PreXP;

    /** Optional designer-supplied defeat screen. Falls back to the C++
     *  UDefeatScreenWidget when unset. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|UI")
    TSubclassOf<class UUserWidget> DefeatWidgetClass;

    UPROPERTY()
    TObjectPtr<class UDefeatScreenWidget> DefeatWidget;

    /** Optional designer-supplied victory screen. Falls back to the C++
     *  UVictoryScreenWidget when unset. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|UI")
    TSubclassOf<class UUserWidget> VictoryWidgetClass;

    UPROPERTY()
    TObjectPtr<class UVictoryScreenWidget> VictoryWidget;

    /** Build + show the defeat screen with Retry / Give Up. */
    void ShowDefeatScreen();

    /** Remove the defeat screen if it's up. */
    void DismissDefeatScreen();

    /** Build + show the victory results panel (animated XP rows + spoils). */
    void ShowVictoryScreen(const TArray<FVictoryMemberXP>& Members,
                           const TArray<FString>& SpoilLines);

    /** Continue button — dismiss the victory panel + restore game input. */
    void ContinueAfterVictory();

    /** Neighbouring encounters pulled into the active fight by the merging
     *  mechanic. Destroyed alongside ActiveEncounter on victory. */
    UPROPERTY()
    TArray<TObjectPtr<AEnemyEncounter>> MergedEncounters;

    /** Global merging radius (cm). When the player is caught, every encounter
     *  inside this radius of the triggering encounter is pulled into the
     *  fight. Set per-encounter bAllowMerging=false to opt specific enemies
     *  out (bosses, story fights). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|Merging",
              meta = (ClampMin = "0.0"))
    float GlobalMergeRadius = 1000.f;

    /** Total enemy roster cap — even if N neighbours are in GlobalMergeRadius,
     *  we never spawn more than this many combatants. Match this to the
     *  largest ABattleArena's enemy-slot count (currently 3). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|Merging",
              meta = (ClampMin = "1"))
    int32 MaxMergedEnemies = 3;

    /** Enemies spawned for the current combat. Destroyed on victory. */
    UPROPERTY()
    TArray<TObjectPtr<ACombatantBase>> SpawnedEnemies;

    /** Regular encounters defeated since the last rest — disabled (not
     *  destroyed) so RespawnDefeatedEncounters can bring them back. Bosses are
     *  never added here. Session-only; a level reload re-places them anyway. */
    UPROPERTY()
    TArray<TObjectPtr<AEnemyEncounter>> DefeatedEncounters;

    // -------------------------------------------------------------------------
    //  Drops — awarded to the persistent roster on victory.
    // -------------------------------------------------------------------------

    /** Material every defeated enemy drops (Metal Scraps). Set in BP defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|Drops")
    TObjectPtr<UCraftingMaterialDataAsset> DefaultDropMaterial;

    /** Gold awarded per defeated enemy. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|Drops",
              meta = (ClampMin = "0"))
    int32 GoldPerEnemy = 25;

    /** DefaultDropMaterial awarded per defeated enemy. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "JRPG|Drops",
              meta = (ClampMin = "0"))
    int32 MaterialPerEnemy = 3;

    /** Position of the exploration pawn at the moment combat started. */
    FVector  PawnReturnLocation = FVector::ZeroVector;
    FRotator PawnReturnRotation = FRotator::ZeroRotator;

    UPROPERTY()
    TObjectPtr<AExplorationPawn> CachedExplorationPawn;
};
