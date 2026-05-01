#include "Exploration/EnemyEncounter.h"
#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyEncounter::AEnemyEncounter()
{
    PrimaryActorTick.bCanEverTick = false;

    // -------------------------------------------------------------------------
    //  Mesh — visual placeholder, set the actual mesh in BP defaults
    // -------------------------------------------------------------------------

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Mesh->SetCollisionObjectType(ECC_WorldDynamic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // -------------------------------------------------------------------------
    //  Trigger sphere — fires when the exploration pawn overlaps
    // -------------------------------------------------------------------------

    TriggerSphere = CreateDefaultSubobject<USphereComponent>(TEXT("TriggerSphere"));
    TriggerSphere->SetupAttachment(RootComponent);
    TriggerSphere->InitSphereRadius(150.f);
    TriggerSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    TriggerSphere->SetCollisionObjectType(ECC_WorldDynamic);
    TriggerSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    TriggerSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    TriggerSphere->SetGenerateOverlapEvents(true);

    TriggerSphere->OnComponentBeginOverlap.AddDynamic(this, &AEnemyEncounter::HandleTriggerOverlap);
}

void AEnemyEncounter::HandleTriggerOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
                                            AActor* OtherActor,
                                            UPrimitiveComponent* /*OtherComp*/,
                                            int32 /*OtherBodyIndex*/,
                                            bool /*bFromSweep*/,
                                            const FHitResult& /*SweepResult*/)
{
    // Only react to the player's exploration pawn, not arbitrary actors.
    if (!Cast<AExplorationPawn>(OtherActor)) { return; }

    AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM)
    {
        UE_LOG(LogTemp, Error, TEXT("[EnemyEncounter] No AJrpgGameMode found — cannot start combat. "
                                     "Did you set GameMode override to BP_JrpgGameMode in World Settings?"));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s triggered by %s"),
        *GetName(), *OtherActor->GetName());

    GM->BeginEncounter(this);
}
