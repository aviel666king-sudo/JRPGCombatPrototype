#include "Core/DangerManager.h"
#include "Stats/Stats.h"

void UDangerManager::IncrementDanger()
{
    const int32 OldLevel = CurrentLevel;
    CurrentLevel = FMath::Clamp(CurrentLevel + 1, 0, MaxLevel);
    if (CurrentLevel != OldLevel)
    {
        DecayAccumulator = 0.f;
        UE_LOG(LogTemp, Log, TEXT("[Danger] Level %d → %d (multiplier x%.2f)"),
            OldLevel, CurrentLevel, GetStatMultiplier());
    }
}

void UDangerManager::ResetDanger()
{
    if (CurrentLevel != 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[Danger] Reset (was %d)"), CurrentLevel);
    }
    CurrentLevel = 0;
    DecayAccumulator = 0.f;
}

void UDangerManager::SetChaseActive(bool bActive)
{
    bChaseActive = bActive;
}

void UDangerManager::SetInBattle(bool bActive)
{
    bInBattle = bActive;
}

void UDangerManager::Tick(float DeltaTime)
{
    // Decay only when out of combat — no chase, no battle.
    if (bChaseActive || bInBattle)
    {
        DecayAccumulator = 0.f;
        return;
    }

    DecayAccumulator += DeltaTime;
    if (DecayAccumulator >= DecaySeconds)
    {
        DecayAccumulator = 0.f;
        const int32 OldLevel = CurrentLevel;
        CurrentLevel = FMath::Max(0, CurrentLevel - 1);
        if (CurrentLevel != OldLevel)
        {
            UE_LOG(LogTemp, Log, TEXT("[Danger] Decay %d → %d (multiplier x%.2f)"),
                OldLevel, CurrentLevel, GetStatMultiplier());
        }
    }
}

TStatId UDangerManager::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UDangerManager, STATGROUP_Tickables);
}
