#include "Exploration/LootPickup.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Equipment/CharacterWeaponDataAsset.h"
#include "Equipment/CharacterArmorDataAsset.h"
#include "Equipment/CharacterChipDataAsset.h"
#include "Equipment/CraftingMaterialDataAsset.h"
#include "Roster/RosterSubsystem.h"

ALootPickup::ALootPickup()
{
    PrimaryActorTick.bCanEverTick = true;   // for the in-range "[E] Pick up" prompt

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    RootComponent = Mesh;
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    PickupSphere->SetupAttachment(RootComponent);
    PickupSphere->SetSphereRadius(PickupRadius);
    PickupSphere->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
    PickupSphere->SetGenerateOverlapEvents(true);
}

void ALootPickup::BeginPlay()
{
    Super::BeginPlay();

    // If this pickup only hands out items the player already owns (and no
    // gold / materials), it's redundant — never show it. Lets a level reload
    // skip a piece of loot the player grabbed on a previous visit.
    if (UWorld* World = GetWorld())
    {
        if (UGameInstance* GI = World->GetGameInstance())
        {
            if (URosterSubsystem* Roster = GI->GetSubsystem<URosterSubsystem>())
            {
                if (IsFullyRedundant(Roster))
                {
                    Destroy();
                    return;
                }
            }
        }
    }

    PickupSphere->SetSphereRadius(PickupRadius);
    PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ALootPickup::HandleBeginOverlap);
    PickupSphere->OnComponentEndOverlap.AddDynamic(this, &ALootPickup::HandleEndOverlap);

    // Initial-overlap sweep: report in-range if the player spawned on top.
    TArray<AActor*> Overlapping;
    PickupSphere->GetOverlappingActors(Overlapping, APawn::StaticClass());
    for (AActor* A : Overlapping)
    {
        if (APawn* P = Cast<APawn>(A))
        {
            if (APlayerController* PC = P->GetController<APlayerController>())
            {
                if (PC->IsLocalController()) { bPlayerInRange = true; break; }
            }
        }
    }
}

void ALootPickup::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
#if !UE_BUILD_SHIPPING
    if (bPlayerInRange) { DrawInteractPrompt(); }
#endif
}

void ALootPickup::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
                                     AActor* OtherActor,
                                     UPrimitiveComponent* /*OtherComp*/,
                                     int32 /*OtherBodyIndex*/,
                                     bool /*bFromSweep*/,
                                     const FHitResult& /*SweepResult*/)
{
    APawn* AsPawn = Cast<APawn>(OtherActor);
    if (!AsPawn) { return; }
    APlayerController* PC = AsPawn->GetController<APlayerController>();
    if (PC && PC->IsLocalController()) { bPlayerInRange = true; }
}

void ALootPickup::HandleEndOverlap(UPrimitiveComponent* /*OverlappedComponent*/,
                                   AActor* OtherActor,
                                   UPrimitiveComponent* /*OtherComp*/,
                                   int32 /*OtherBodyIndex*/)
{
    APawn* AsPawn = Cast<APawn>(OtherActor);
    if (!AsPawn) { return; }
    APlayerController* PC = AsPawn->GetController<APlayerController>();
    if (PC && PC->IsLocalController()) { bPlayerInRange = false; }
}

void ALootPickup::Interact()
{
    UWorld* World = GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    URosterSubsystem* Roster = GI ? GI->GetSubsystem<URosterSubsystem>() : nullptr;
    if (!Roster) { return; }

    const FString Summary = GrantTo(Roster);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.5f, FColor::Green,
            FString::Printf(TEXT("Picked up:  %s"), *Summary));
    }
    UE_LOG(LogTemp, Log, TEXT("[LootPickup] %s granted: %s"), *GetName(), *Summary);

    Destroy();
}

#if !UE_BUILD_SHIPPING
void ALootPickup::DrawInteractPrompt()
{
    if (!GetWorld()) { return; }
    const FVector Base = GetActorLocation() + FVector(0.f, 0.f, 90.f);
    DrawDebugString(GetWorld(), Base, TEXT("[E] Pick up"), nullptr, FColor::Green, 0.f, true);
}
#endif

bool ALootPickup::IsFullyRedundant(URosterSubsystem* Roster) const
{
    if (!Roster) { return false; }

    // Gold / materials are never "already owned" — if this pickup gives any,
    // it always has something useful to offer.
    if (GoldGrant > 0)                              { return false; }
    if (MaterialGrant && MaterialGrantCount > 0)    { return false; }

    bool bHasAnyItem = false;
    for (const TObjectPtr<UCharacterWeaponDataAsset>& W : WeaponGrants)
    {
        if (!W) { continue; }
        bHasAnyItem = true;
        if (!Roster->OwnsWeapon(W)) { return false; }
    }
    for (const TObjectPtr<UCharacterArmorDataAsset>& A : ArmorGrants)
    {
        if (!A) { continue; }
        bHasAnyItem = true;
        if (!Roster->OwnsArmor(A)) { return false; }
    }
    for (const TObjectPtr<UCharacterChipDataAsset>& C : ChipGrants)
    {
        if (!C) { continue; }
        bHasAnyItem = true;
        if (!Roster->OwnsChip(C)) { return false; }
    }

    // Redundant only if it had items and the player owns them all.
    return bHasAnyItem;
}

FString ALootPickup::GrantTo(URosterSubsystem* Roster)
{
    TArray<FString> Parts;

    for (const TObjectPtr<UCharacterWeaponDataAsset>& W : WeaponGrants)
    {
        if (W) { Roster->AddOwnedWeapon(W); Parts.Add(W->DisplayName.ToString()); }
    }
    for (const TObjectPtr<UCharacterArmorDataAsset>& A : ArmorGrants)
    {
        if (A) { Roster->AddOwnedArmor(A); Parts.Add(A->DisplayName.ToString()); }
    }
    for (const TObjectPtr<UCharacterChipDataAsset>& C : ChipGrants)
    {
        if (C) { Roster->AddOwnedChip(C); Parts.Add(C->DisplayName.ToString()); }
    }
    if (GoldGrant > 0)
    {
        Roster->AddGold(GoldGrant);
        Parts.Add(FString::Printf(TEXT("%d Gold"), GoldGrant));
    }
    if (MaterialGrant && MaterialGrantCount > 0)
    {
        Roster->AddMaterial(MaterialGrant, MaterialGrantCount);
        Parts.Add(FString::Printf(TEXT("%d %s"), MaterialGrantCount, *MaterialGrant->DisplayName.ToString()));
    }

    return Parts.Num() > 0 ? FString::Join(Parts, TEXT(", ")) : FString(TEXT("(nothing)"));
}
