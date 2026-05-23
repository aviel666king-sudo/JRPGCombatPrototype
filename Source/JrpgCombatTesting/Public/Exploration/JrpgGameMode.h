#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "JrpgGameMode.generated.h"

class ACombatantBase;
class ABattleManager;
class ABattleArena;
class AEnemyEncounter;
class AExplorationPawn;

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
    void SetPlayerParty(const TArray<ACombatantBase*>& InParty) { PlayerParty = InParty; }

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

protected:

    virtual void BeginPlay() override;

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

    /** Position of the exploration pawn at the moment combat started. */
    FVector  PawnReturnLocation = FVector::ZeroVector;
    FRotator PawnReturnRotation = FRotator::ZeroRotator;

    UPROPERTY()
    TObjectPtr<AExplorationPawn> CachedExplorationPawn;
};
