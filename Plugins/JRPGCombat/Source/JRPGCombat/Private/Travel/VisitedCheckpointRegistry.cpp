#include "Travel/VisitedCheckpointRegistry.h"

void UVisitedCheckpointRegistry::RegisterVisited(FName LevelName, FName CheckpointId)
{
    if (LevelName == NAME_None || CheckpointId == NAME_None) { return; }

    TArray<FName>& List = Visited.FindOrAdd(LevelName);
    if (!List.Contains(CheckpointId))
    {
        List.Add(CheckpointId);
        UE_LOG(LogTemp, Log, TEXT("[CheckpointRegistry] Registered '%s' in level '%s' (total: %d)"),
            *CheckpointId.ToString(), *LevelName.ToString(), List.Num());
    }
}

bool UVisitedCheckpointRegistry::HasVisited(FName LevelName, FName CheckpointId) const
{
    if (const TArray<FName>* List = Visited.Find(LevelName))
    {
        return List->Contains(CheckpointId);
    }
    return false;
}

void UVisitedCheckpointRegistry::GetVisitedInLevel(FName LevelName, TArray<FName>& OutCheckpointIds) const
{
    OutCheckpointIds.Reset();
    if (const TArray<FName>* List = Visited.Find(LevelName))
    {
        OutCheckpointIds = *List;
    }
}

void UVisitedCheckpointRegistry::ClearAll()
{
    Visited.Reset();
    UE_LOG(LogTemp, Log, TEXT("[CheckpointRegistry] Cleared all visited checkpoints"));
}
