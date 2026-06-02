#include "Persistence/WorldStateSubsystem.h"

#include "Engine/World.h"
#include "Misc/Paths.h"

void UWorldStateSubsystem::MarkDone(FName Key)
{
    if (Key != NAME_None) { DoneKeys.Add(Key); }
}

bool UWorldStateSubsystem::IsDone(FName Key) const
{
    return Key != NAME_None && DoneKeys.Contains(Key);
}

FName UWorldStateSubsystem::MakeKey(const UObject* WorldContextObject, FName Category, FName Id)
{
    FString Level = TEXT("?");
    if (WorldContextObject)
    {
        if (const UWorld* World = WorldContextObject->GetWorld())
        {
            FString MapName = World->GetMapName();
            MapName.RemoveFromStart(World->StreamingLevelsPrefix);
            Level = FPaths::GetBaseFilename(MapName);
        }
    }
    return FName(*FString::Printf(TEXT("%s/%s/%s"),
        *Category.ToString(), *Level, *Id.ToString()));
}

void UWorldStateSubsystem::ClearAll()
{
    DoneKeys.Reset();
}
