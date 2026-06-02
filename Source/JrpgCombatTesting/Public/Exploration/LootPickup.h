#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UCharacterWeaponDataAsset;
class UCharacterArmorDataAsset;
class UCharacterChipDataAsset;
class UCraftingMaterialDataAsset;
class URosterSubsystem;

/**
 * ALootPickup
 *
 * A placeable collectable in the world (NOT an enemy drop). The player walks
 * up to it and presses E (handled by AExplorationPawn::HandleInteract) to
 * collect; the configured items are added to the persistent owned inventory
 * (URosterSubsystem), then the pickup disappears. Granting goes through the
 * AddOwned* / AddMaterial APIs, which dedupe — so you can never carry two of
 * the same item.
 *
 * Drop an armor (with its chip already socketed on the armor asset) into
 * ArmorGrants to hand the player a complete piece; or list chips, weapons,
 * gold, materials in any combination.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBATTESTING_API ALootPickup : public AActor
{
    GENERATED_BODY()

public:

    ALootPickup();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Loot")
    TObjectPtr<USphereComponent> PickupSphere;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot",
              meta = (ClampMin = "50.0"))
    float PickupRadius = 150.f;

    /** Stable id for persistence. Once collected, this pickup never reappears
     *  (tracked in the WorldStateSubsystem / save). Leave blank to fall back to
     *  the actor's name — fine for one-off placements, but set it explicitly so
     *  an editor rename doesn't resurrect a collected pickup. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot")
    FName PersistentId;

    // -------------------------------------------------------------------------
    //  What this pickup grants (any combination).
    // -------------------------------------------------------------------------

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Grants")
    TArray<TObjectPtr<UCharacterWeaponDataAsset>> WeaponGrants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Grants")
    TArray<TObjectPtr<UCharacterArmorDataAsset>> ArmorGrants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Grants")
    TArray<TObjectPtr<UCharacterChipDataAsset>> ChipGrants;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Grants",
              meta = (ClampMin = "0"))
    int32 GoldGrant = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Grants")
    TObjectPtr<UCraftingMaterialDataAsset> MaterialGrant;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loot|Grants",
              meta = (ClampMin = "0"))
    int32 MaterialGrantCount = 0;

    /** True while the player pawn is overlapping the interact sphere. The pawn
     *  reads this to decide whether E should collect this pickup. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Loot")
    bool IsPlayerInRange() const { return bPlayerInRange; }

    /** Collect: grant everything to the owned inventory, toast, and destroy.
     *  Called by the pawn on E when in range. */
    UFUNCTION(BlueprintCallable, Category = "Loot")
    void Interact();

protected:

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    UFUNCTION()
    void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent,
                            AActor* OtherActor,
                            UPrimitiveComponent* OtherComp,
                            int32 OtherBodyIndex,
                            bool bFromSweep,
                            const FHitResult& SweepResult);

    UFUNCTION()
    void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent,
                          AActor* OtherActor,
                          UPrimitiveComponent* OtherComp,
                          int32 OtherBodyIndex);

    UPROPERTY(BlueprintReadOnly, Category = "Loot")
    bool bPlayerInRange = false;

    /** Add every configured grant to the owned inventory (dedup), return a
     *  short summary string of what was given. */
    FString GrantTo(URosterSubsystem* Roster);

    /** True if this pickup grants no gold/materials and every item it would
     *  give is already owned — i.e. there's nothing left to collect. */
    bool IsFullyRedundant(URosterSubsystem* Roster) const;

    /** WorldState key identifying this exact placement. */
    FName GetPersistentKey() const;

#if !UE_BUILD_SHIPPING
    void DrawInteractPrompt();
#endif
};
