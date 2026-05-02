#include "Exploration/EnemyEncounter.h"
#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"

AEnemyEncounter::AEnemyEncounter()
{
    // Tick to count down stun timer. Cheap — almost always early-outs.
    PrimaryActorTick.bCanEverTick = true;

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

void AEnemyEncounter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bIsStunned) { return; }

    StunRemaining -= DeltaTime;
    if (StunRemaining <= 0.f)
    {
        bIsStunned    = false;
        StunRemaining = 0.f;
        UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s recovered from stun."), *GetName());
    }
}

void AEnemyEncounter::Stun(float Duration)
{
    if (Duration <= 0.f) { return; }

    // Refresh — if already stunned just extend if the new duration is longer.
    StunRemaining = FMath::Max(StunRemaining, Duration);
    bIsStunned    = true;

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s stunned for %.1fs"),
        *GetName(), StunRemaining);
}

void AEnemyEncounter::TriggerCombat(bool bPlayerHasInitiative)
{
    AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this));
    if (!GM)
    {
        UE_LOG(LogTemp, Error, TEXT("[EnemyEncounter] No AJrpgGameMode — can't start combat."));
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s triggering combat. PlayerInitiative=%s"),
        *GetName(), bPlayerHasInitiative ? TEXT("true") : TEXT("false"));

    GM->BeginEncounter(this, bPlayerHasInitiative);
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

    // If the player walked into a stunned encounter, they get the drop —
    // their fastest party member acts first. Otherwise it's a standard
    // ambush: the fastest enemy goes first (per the GDD's stealth section).
    const bool bPlayerInitiative = bIsStunned;

    UE_LOG(LogTemp, Log, TEXT("[EnemyEncounter] %s triggered by %s (stunned=%s)"),
        *GetName(), *OtherActor->GetName(), bIsStunned ? TEXT("true") : TEXT("false"));

    TriggerCombat(bPlayerInitiative);
}
