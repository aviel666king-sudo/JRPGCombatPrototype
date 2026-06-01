#include "Travel/WorldPortal.h"
#include "Travel/JrpgTravelSubsystem.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

AWorldPortal::AWorldPortal()
{
    PrimaryActorTick.bCanEverTick = true;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    InteractSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractSphere"));
    InteractSphere->SetupAttachment(RootComponent);
    InteractSphere->SetSphereRadius(InteractRadius);
    InteractSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    InteractSphere->SetGenerateOverlapEvents(true);
}

void AWorldPortal::BeginPlay()
{
    Super::BeginPlay();

    InteractSphere->SetSphereRadius(InteractRadius);
    InteractSphere->OnComponentBeginOverlap.AddDynamic(this, &AWorldPortal::HandleBeginOverlap);
    InteractSphere->OnComponentEndOverlap.AddDynamic(this, &AWorldPortal::HandleEndOverlap);

    // Initial-overlap sweep: if the player pawn already spawned inside the
    // sphere (e.g. PlayerStart placed right next to the portal), UE doesn't
    // fire OnBeginOverlap for the pre-existing overlap, so bPlayerInRange
    // stays false until the player walks out and back in. Resolve this by
    // checking the current overlap set on BeginPlay.
    TArray<AActor*> Overlapping;
    InteractSphere->GetOverlappingActors(Overlapping, APawn::StaticClass());
    for (AActor* Actor : Overlapping)
    {
        APawn* AsPawn = Cast<APawn>(Actor);
        if (!AsPawn) { continue; }
        APlayerController* PC = AsPawn->GetController<APlayerController>();
        if (PC && PC->IsLocalController())
        {
            bPlayerInRange = true;
            break;
        }
    }
}

void AWorldPortal::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

#if !UE_BUILD_SHIPPING
    if (bPlayerInRange) { DrawInteractPrompt(); }
#endif
}

void AWorldPortal::Use(APawn* User)
{
    if (!bPlayerInRange)
    {
        return;
    }
    if (TargetLevel == NAME_None)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Portal] '%s' has no TargetLevel set"), *GetName());
        return;
    }

    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    UJrpgTravelSubsystem* Travel = GI ? GI->GetSubsystem<UJrpgTravelSubsystem>() : nullptr;
    if (!Travel)
    {
        UE_LOG(LogTemp, Error, TEXT("[Portal] No travel subsystem available"));
        return;
    }

    Travel->TravelToLevel(TargetLevel, TargetArrivalTag);
}

void AWorldPortal::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
                                      AActor* OtherActor,
                                      UPrimitiveComponent* /*OtherComp*/,
                                      int32 /*OtherBodyIndex*/,
                                      bool /*bFromSweep*/,
                                      const FHitResult& /*SweepResult*/)
{
    if (!OtherActor) { return; }
    APawn* AsPawn = Cast<APawn>(OtherActor);
    if (!AsPawn) { return; }

    // Only the locally-controlled player counts as "in range."
    APlayerController* PC = AsPawn->GetController<APlayerController>();
    if (!PC || !PC->IsLocalController()) { return; }

    bPlayerInRange = true;
}

void AWorldPortal::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
                                    AActor* OtherActor,
                                    UPrimitiveComponent* /*OtherComp*/,
                                    int32 /*OtherBodyIndex*/)
{
    if (!OtherActor) { return; }
    APawn* AsPawn = Cast<APawn>(OtherActor);
    if (!AsPawn) { return; }
    APlayerController* PC = AsPawn->GetController<APlayerController>();
    if (!PC || !PC->IsLocalController()) { return; }

    bPlayerInRange = false;
}

#if !UE_BUILD_SHIPPING
void AWorldPortal::DrawInteractPrompt()
{
    if (!GetWorld()) { return; }
    const FVector Base = GetActorLocation() + FVector(0.f, 0.f, 150.f);
    DrawDebugString(GetWorld(), Base, PromptLabel, nullptr,
        FColor(0, 200, 255), 0.f, true);
}
#endif
