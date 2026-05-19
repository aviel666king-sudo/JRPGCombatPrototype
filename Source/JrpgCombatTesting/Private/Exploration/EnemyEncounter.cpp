#include "Exploration/EnemyEncounter.h"
#include "Exploration/JrpgGameMode.h"
#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyDetectionComponent.h"
#include "Exploration/DetectionMeterWidget.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
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

    // -------------------------------------------------------------------------
    //  Detection (Phase B) — auto-attached so all encounters get vision
    //  without per-BP wiring. Tunables in BP defaults under Detection|Vision.
    // -------------------------------------------------------------------------

    Detection = CreateDefaultSubobject<UEnemyDetectionComponent>(TEXT("Detection"));

    // -------------------------------------------------------------------------
    //  Detection meter UI (Phase B2) — world-space widget floating overhead,
    //  billboarded toward the camera. Class is assigned in BP defaults
    //  (DetectionMeterWidgetClass = WBP_DetectionMeter).
    // -------------------------------------------------------------------------

    DetectionMeterComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("DetectionMeter"));
    DetectionMeterComponent->SetupAttachment(RootComponent);
    DetectionMeterComponent->SetRelativeLocation(FVector(0.f, 0.f, DetectionMeterHeight));

    // World space is far more reliable than Screen — Screen mode has quirky
    // viewport/scale rendering issues that can leave the widget invisible.
    // World space renders as a 3D plane that we ticker-rotate to face the camera.
    DetectionMeterComponent->SetWidgetSpace(EWidgetSpace::World);
    DetectionMeterComponent->SetDrawSize(DetectionMeterDrawSize);

    // Scale down the 3D plane so 200px of widget ≈ 100cm wide in world space.
    DetectionMeterComponent->SetRelativeScale3D(FVector(0.5f));
    DetectionMeterComponent->SetTwoSided(true);  // visible from either side
    DetectionMeterComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    DetectionMeterComponent->SetGenerateOverlapEvents(false);
    DetectionMeterComponent->SetVisibility(true);
}

void AEnemyEncounter::BeginPlay()
{
    Super::BeginPlay();

    // Apply BP-tuned values to the widget component (constructor uses defaults
    // before BP overrides land).
    if (DetectionMeterComponent)
    {
        DetectionMeterComponent->SetRelativeLocation(FVector(0.f, 0.f, DetectionMeterHeight));
        DetectionMeterComponent->SetDrawSize(DetectionMeterDrawSize);

        // Assign the WBP class and bind the widget to our detection component.
        if (DetectionMeterWidgetClass)
        {
            DetectionMeterComponent->SetWidgetClass(DetectionMeterWidgetClass);

            // Force the user widget to spawn now so we can bind it. Without this,
            // GetUserWidgetObject() returns nullptr until the first tick.
            DetectionMeterComponent->InitWidget();

            if (UDetectionMeterWidget* Meter =
                    Cast<UDetectionMeterWidget>(DetectionMeterComponent->GetUserWidgetObject()))
            {
                Meter->BindToDetection(Detection);
                UE_LOG(LogTemp, Warning, TEXT("[EnemyEncounter] %s: meter widget spawned and bound."),
                    *GetName());
            }
            else
            {
                UE_LOG(LogTemp, Warning,
                    TEXT("[EnemyEncounter] %s: DetectionMeterWidgetClass is set but does not derive from UDetectionMeterWidget — meter will not update."),
                    *GetName());
            }
        }
        else
        {
            // No widget class assigned in BP — hide the component entirely so
            // we don't render an empty 2D plate.
            UE_LOG(LogTemp, Warning,
                TEXT("[EnemyEncounter] %s: DetectionMeterWidgetClass is NULL — assign WBP_DetectionMeter in BP defaults."),
                *GetName());
            DetectionMeterComponent->SetVisibility(false);
        }
    }
}

void AEnemyEncounter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Billboard the detection meter toward the player camera so the bar is
    // always legible regardless of where the camera is. World-space widgets
    // need this manually; Screen-space did it automatically but had other
    // rendering issues.
    if (DetectionMeterComponent && DetectionMeterComponent->IsVisible())
    {
        if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
        {
            FVector CamLoc;
            FRotator CamRot;
            PC->GetPlayerViewPoint(CamLoc, CamRot);
            const FVector ToCam = CamLoc - DetectionMeterComponent->GetComponentLocation();
            const FRotator FaceRot = ToCam.Rotation() + FRotator(0.f, 180.f, 0.f);
            DetectionMeterComponent->SetWorldRotation(FaceRot);
        }
    }

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
