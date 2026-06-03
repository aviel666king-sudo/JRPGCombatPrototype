#include "Persistence/SaveSubsystem.h"

#include "Persistence/JrpgSaveGame.h"
#include "Persistence/WorldStateSubsystem.h"
#include "Roster/RosterSubsystem.h"
#include "Travel/VisitedCheckpointRegistry.h"
#include "Travel/JrpgTravelSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

FName USaveSubsystem::CanonicalLevelName() const
{
    if (const UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
    {
        FString MapName = World->GetMapName();
        MapName.RemoveFromStart(World->StreamingLevelsPrefix);
        return FName(*FPaths::GetBaseFilename(MapName));
    }
    return NAME_None;
}

void USaveSubsystem::GatherInto(UJrpgSaveGame* Save)
{
    if (!Save) { return; }
    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
    {
        Roster->SyncFromLiveActors();   // reflect live HP/level/XP before capture
        Roster->CaptureToSave(*Save);
    }

    if (UWorldStateSubsystem* WS = GI->GetSubsystem<UWorldStateSubsystem>())
    {
        Save->WorldStateKeys = WS->GetDoneKeys().Array();
    }

    if (UVisitedCheckpointRegistry* Reg = GI->GetSubsystem<UVisitedCheckpointRegistry>())
    {
        Save->Visited.Reset();
        for (const TPair<FName, TArray<FName>>& P : Reg->GetAllVisited())
        {
            FVisitedLevelSave V; V.Level = P.Key; V.CheckpointIds = P.Value;
            Save->Visited.Add(MoveTemp(V));
        }
    }

    Save->SavedLevel = CanonicalLevelName();

    if (UWorld* World = GI->GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                Save->SavedPawnTransform = Pawn->GetActorTransform();
            }
        }
    }

    Save->Timestamp   = FDateTime::Now();
    Save->DisplayName = FString::Printf(TEXT("%s  -  %s"),
        *Save->SavedLevel.ToString(), *Save->Timestamp.ToString(TEXT("%Y.%m.%d %H:%M")));
}

void USaveSubsystem::ApplyFrom(UJrpgSaveGame* Save)
{
    if (!Save) { return; }
    UGameInstance* GI = GetGameInstance();
    if (!GI) { return; }

    if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
    {
        Roster->ApplyFromSave(*Save);
    }
    if (UWorldStateSubsystem* WS = GI->GetSubsystem<UWorldStateSubsystem>())
    {
        WS->SetDoneKeys(TSet<FName>(Save->WorldStateKeys));
    }
    if (UVisitedCheckpointRegistry* Reg = GI->GetSubsystem<UVisitedCheckpointRegistry>())
    {
        TMap<FName, TArray<FName>> Map;
        for (const FVisitedLevelSave& V : Save->Visited) { Map.Add(V.Level, V.CheckpointIds); }
        Reg->SetAllVisited(Map);
    }
}

bool USaveSubsystem::SaveToSlot(const FString& Slot)
{
    if (Slot.IsEmpty()) { return false; }

    UJrpgSaveGame* Save = Cast<UJrpgSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UJrpgSaveGame::StaticClass()));
    if (!Save) { return false; }

    GatherInto(Save);

    const bool bOk = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    UE_LOG(LogTemp, Log, TEXT("[Save] SaveToSlot '%s' -> %s"), *Slot, bOk ? TEXT("OK") : TEXT("FAILED"));
    return bOk;
}

bool USaveSubsystem::LoadFromSlot(const FString& Slot)
{
    UJrpgSaveGame* Save = Cast<UJrpgSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Save) { UE_LOG(LogTemp, Warning, TEXT("[Save] LoadFromSlot '%s' — no save"), *Slot); return false; }

    ApplyFrom(Save);
    ActiveSlot = Slot;

    // Travel to the saved level + position. The roster is already applied (and
    // flagged just-loaded), so the destination level's SetPlayerParty restores
    // the loadout and full-heals on arrival.
    if (UJrpgTravelSubsystem* Travel = GetGameInstance()->GetSubsystem<UJrpgTravelSubsystem>())
    {
        Travel->TravelToLevelAtTransform(Save->SavedLevel, Save->SavedPawnTransform);
    }
    else
    {
        UGameplayStatics::OpenLevel(this, Save->SavedLevel);
    }
    return true;
}

bool USaveSubsystem::DoesSlotExist(const FString& Slot) const
{
    return UGameplayStatics::DoesSaveGameExist(Slot, 0);
}

bool USaveSubsystem::DeleteSlot(const FString& Slot)
{
    return UGameplayStatics::DeleteGameInSlot(Slot, 0);
}

UJrpgSaveGame* USaveSubsystem::PeekSlot(const FString& Slot) const
{
    return Cast<UJrpgSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
}
