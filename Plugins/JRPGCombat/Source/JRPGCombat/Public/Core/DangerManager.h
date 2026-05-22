#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "DangerManager.generated.h"

/**
 * UDangerManager
 *
 * Tracks the player's current "danger level" in the overworld and exposes
 * the multiplier that scales enemy stats and chase speed. Lives on the
 * GameInstance so it persists across map loads (zone transitions).
 *
 * Design (see JRPG combat doc):
 *   - 5 levels (0..MaxLevel). Each level adds StepMultiplier (default 0.12).
 *     Level 5 → 1.6x stats and chase speed.
 *   - Decays automatically: 1 level lost per DecaySeconds (default 20) when
 *     the player is "out of combat" — i.e. NOT being chased AND NOT in a
 *     turn-based battle. Both flags are pushed in by the encounter / battle
 *     systems via SetChaseActive / SetInBattle.
 *   - Reset() snaps back to 0 (used by RestPoint actor on interact).
 *   - IncrementDanger() raises the level by 1 (used after non-stealth wins).
 */
UCLASS()
class JRPGCOMBAT_API UDangerManager
    : public UGameInstanceSubsystem
    , public FTickableGameObject
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  Tunables — editable via DefaultGame.ini under [/Script/JRPGCombat.DangerManager]
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Danger")
    int32 MaxLevel = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Danger")
    float StepMultiplier = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Danger")
    float DecaySeconds = 20.f;

    // -------------------------------------------------------------------------
    //  Player level (placeholder until a real XP/level system lands)
    //  Used by AEnemyEncounter::CanBeAssassinated to decide if a stealth
    //  approach should one-shot the encounter (player overleveled) or just
    //  grant turn priority in combat.
    // -------------------------------------------------------------------------

    /** Effective level of the active player party. TODO: replace with real
     *  per-character XP system. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Danger|Level")
    int32 PlayerEffectiveLevel = 1;

    /** Player must equal-or-exceed encounter level by AT LEAST this much for a
     *  stealth approach to instant-kill instead of just starting combat with
     *  priority. 0 = same level kills, 1 = need one above, etc. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Danger|Level",
              meta = (ClampMin = "0"))
    int32 AssassinationLevelGap = 0;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Danger|Level")
    int32 GetPlayerEffectiveLevel() const { return PlayerEffectiveLevel; }

    UFUNCTION(BlueprintCallable, Category = "Danger|Level")
    void SetPlayerEffectiveLevel(int32 NewLevel) { PlayerEffectiveLevel = FMath::Max(1, NewLevel); }

    // -------------------------------------------------------------------------
    //  State queries
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Danger")
    int32 GetCurrentLevel() const { return CurrentLevel; }

    /** 1 + StepMultiplier * Level. Used for enemy stat scaling on InitializeForBattle. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Danger")
    float GetStatMultiplier() const { return 1.f + StepMultiplier * CurrentLevel; }

    /** Same scaling as stats — chase movement speed grows with danger. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Danger")
    float GetChaseSpeedMultiplier() const { return GetStatMultiplier(); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Danger")
    bool IsChaseActive() const { return bChaseActive; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Danger")
    bool IsInBattle() const { return bInBattle; }

    // -------------------------------------------------------------------------
    //  Mutators
    // -------------------------------------------------------------------------

    /** Raise danger by 1 (clamped at MaxLevel). */
    UFUNCTION(BlueprintCallable, Category = "Danger")
    void IncrementDanger();

    /** Snap danger back to 0. RestPoint calls this on interact. */
    UFUNCTION(BlueprintCallable, Category = "Danger")
    void ResetDanger();

    /** Pushed by encounter system: true while ANY encounter is actively chasing the player. */
    UFUNCTION(BlueprintCallable, Category = "Danger")
    void SetChaseActive(bool bActive);

    /** Pushed by battle system: true while a turn-based battle is in progress. */
    UFUNCTION(BlueprintCallable, Category = "Danger")
    void SetInBattle(bool bActive);

    // -------------------------------------------------------------------------
    //  FTickableGameObject — drives the out-of-combat decay timer
    // -------------------------------------------------------------------------

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    virtual bool IsTickable() const override { return CurrentLevel > 0; }
    virtual bool IsTickableInEditor() const override { return false; }
    virtual bool IsTickableWhenPaused() const override { return false; }

private:

    UPROPERTY()
    int32 CurrentLevel = 0;

    UPROPERTY()
    bool bChaseActive = false;

    UPROPERTY()
    bool bInBattle = false;

    /** Seconds accumulated toward the next decay tick. Resets to 0 on each level lost. */
    float DecayAccumulator = 0.f;
};
