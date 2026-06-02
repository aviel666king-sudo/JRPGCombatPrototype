#include "Exploration/Checkpoint.h"
#include "Exploration/EnemyEncounter.h"
#include "Exploration/JrpgGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Characters/Base/CombatantBase.h"
#include "Core/DangerManager.h"
#include "Core/BattleManager.h"
#include "Components/ProtocolManagerComponent.h"
#include "Travel/VisitedCheckpointRegistry.h"
#include "Roster/RosterSubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"

namespace
{
    /** Canonical short level name (strips PIE prefixes and any path). */
    FName GetCanonicalLevelName(const UWorld* World)
    {
        if (!World) { return NAME_None; }
        FString MapName = World->GetMapName();
        MapName.RemoveFromStart(World->StreamingLevelsPrefix);
        return FName(*FPaths::GetBaseFilename(MapName));
    }
}

ACheckpoint::ACheckpoint()
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
    InteractSphere->SetupAttachment(RootComponent);
    InteractSphere->InitSphereRadius(InteractRadius);
    InteractSphere->SetCollisionProfileName(TEXT("Trigger"));
    InteractSphere->SetGenerateOverlapEvents(true);
}

void ACheckpoint::BeginPlay()
{
    Super::BeginPlay();

    // Keep the collision sphere in sync with the editable radius.
    if (InteractSphere)
    {
        InteractSphere->SetSphereRadius(InteractRadius);
        InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &ACheckpoint::HandleBeginOverlap);
        InteractSphere->OnComponentEndOverlap.AddDynamic(this, &ACheckpoint::HandleEndOverlap);

        // Initial-overlap sweep: if the player pawn spawned already inside
        // the sphere (e.g. PlayerStart next to the camp checkpoint), UE
        // doesn't fire OnBeginOverlap for that pre-existing overlap. Without
        // this check, L/E/etc. wouldn't work until the player walks out and
        // back in.
        TArray<AActor*> Overlapping;
        InteractSphere->GetOverlappingActors(Overlapping, APawn::StaticClass());
        for (AActor* Actor : Overlapping)
        {
            if (Actor == UGameplayStatics::GetPlayerPawn(this, 0))
            {
                bPlayerInRange = true;
                break;
            }
        }
    }
}

void ACheckpoint::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
    if (bPlayerInRange)
    {
        DrawInteractPrompt();
    }
#endif
}

void ACheckpoint::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
                                     UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (APawn* Pawn = Cast<APawn>(OtherActor))
    {
        if (Pawn == UGameplayStatics::GetPlayerPawn(this, 0))
        {
            bPlayerInRange = true;
            UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Player entered range — press E to rest"));
        }
    }
}

void ACheckpoint::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
                                   UPrimitiveComponent*, int32)
{
    if (APawn* Pawn = Cast<APawn>(OtherActor))
    {
        if (Pawn == UGameplayStatics::GetPlayerPawn(this, 0))
        {
            bPlayerInRange = false;
        }
    }
}

void ACheckpoint::Rest(APawn* Resting)
{
    UWorld* World = GetWorld();
    if (!World) { return; }

    UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Resting..."));

    // 1. Heal entire party to full + clear status effects.
    AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    if (GM)
    {
        for (ACombatantBase* Member : GM->GetPlayerParty())
        {
            if (!Member) { continue; }
            if (Member->GetCurrentHP() <= 0.f)
            {
                Member->Revive(1.f);
            }
            else
            {
                const float Missing = Member->GetMissingHP();
                if (Missing > 0.f)
                {
                    Member->ApplyHealing(Missing, nullptr);
                }
            }
            UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Healed %s -> %.0f / %.0f"),
                   *Member->GetName(), Member->GetCurrentHP(), Member->GetMaxHP());
        }
    }

    // 2. Restock all protocol charges to max (Healing / Revival / AP).
    if (GM && GM->GetBattleManager() && GM->GetBattleManager()->ProtocolManager)
    {
        GM->GetBattleManager()->ProtocolManager->InitializeCharges();
        UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Protocol charges restocked"));
    }

    // 2b. Restock the PERSISTENT party pool + full-heal/revive the records (and
    //     any live actors). This is the pool the overworld heal + roster screen
    //     read, and it carries across levels.
    if (UGameInstance* GI = World->GetGameInstance())
    {
        if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
        {
            static const TArray<ACombatantBase*> EmptyParty;
            Roster->RestockAndHeal(GM ? GM->GetPlayerParty() : EmptyParty);
            UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Persistent party pool restocked + healed"));
        }
    }

    // 3. Reset danger.
    if (UGameInstance* GI = World->GetGameInstance())
    {
        if (UDangerManager* Danger = GI->GetSubsystem<UDangerManager>())
        {
            Danger->ResetDanger();
            Danger->SetChaseActive(false);
            UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Danger reset"));
        }
    }

    // 4. Reset all encounters back to home (clears chase/investigate/alert).
    //    NOTE: encounters destroyed in previous victories DON'T respawn from
    //    this. Full Souls-like respawn requires a persistence tracker — coming
    //    in Commit B once SaveGame lands.
    int32 ResetCount = 0;
    for (TActorIterator<AEnemyEncounter> It(World); It; ++It)
    {
        if (AEnemyEncounter* Encounter = *It)
        {
            Encounter->ResetToSpawn();
            ++ResetCount;
        }
    }
    UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Reset %d encounters to spawn"), ResetCount);

    // 5. Register this checkpoint as visited so it appears in the fast-travel
    //    list at OTHER checkpoints in the same level. Camp checkpoints are
    //    excluded (you don't fast-travel TO camp, you keybind into it).
    if (!bIsCampCheckpoint)
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (UVisitedCheckpointRegistry* Reg = GI->GetSubsystem<UVisitedCheckpointRegistry>())
            {
                Reg->RegisterVisited(GetCanonicalLevelName(World), GetCheckpointId());
            }
        }
    }

    // 6. Record this as the "last rested" checkpoint so a post-defeat Give Up
    //    sends the player back here (fully healed). Camp checkpoints count too.
    if (UGameInstance* GI = World->GetGameInstance())
    {
        if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
        {
            Roster->SetLastRestedCheckpoint(
                GetCanonicalLevelName(World), GetCheckpointId(), GetActorTransform());
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Checkpoint] Rest complete"));

    // Temporary on-screen feedback. Removed once the proper HUD lands.
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.f, FColor::Cyan,
            TEXT("Rested: party healed, protocols restocked, danger reset"));
    }
}

FName ACheckpoint::GetCheckpointId() const
{
    return CheckpointId != NAME_None ? CheckpointId : GetFName();
}

#if !UE_BUILD_SHIPPING
void ACheckpoint::DrawInteractPrompt()
{
    const FVector Base = GetActorLocation() + FVector(0.f, 0.f, 120.f);

    // The "leave" line changes shape on camp checkpoints.
    const TCHAR* LeavePrompt = bIsCampCheckpoint
        ? TEXT("[L] Leave Camp    [U] Upgrade Gear")
        : TEXT("[L] Leave to Open World    [G] Fast Travel");

    DrawDebugString(GetWorld(), Base, TEXT("[E] Rest    [K] Stat Shop    [J] Skill Tree"),
                    nullptr, FColor::Green, 0.f, true, 1.2f);
    DrawDebugString(GetWorld(), Base + FVector(0.f, 0.f, 20.f), LeavePrompt,
                    nullptr, FColor(0, 200, 255), 0.f, true, 1.2f);
    DrawDebugCircle(GetWorld(), GetActorLocation(), InteractRadius, 32,
                    FColor::Green, false, -1.f, 0, 2.f,
                    FVector(0, 1, 0), FVector(1, 0, 0), false);
}
#endif
