#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Checkpoint.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APawn;

/**
 * ACheckpoint
 *
 * Placeable "rest point" actor. The player walks into the interact sphere and
 * presses E to rest. Resting heals the whole party, clears danger, resets all
 * non-boss encounters in the level back to their home location (Souls-like
 * respawn). UI / save / coin-spend tabs come in later commits.
 *
 *  Place one near spawn in GroundZero.umap for first-pass testing.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBATTESTING_API ACheckpoint : public AActor
{
    GENERATED_BODY()

public:

    ACheckpoint();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    TObjectPtr<UStaticMeshComponent> Mesh;

    /** Player must overlap this sphere to be able to rest. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Checkpoint")
    TObjectPtr<USphereComponent> InteractSphere;

    /** Interaction radius in cm. Editable per-instance. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint",
              meta = (ClampMin = "50.0"))
    float InteractRadius = 250.f;

    /** Stable identifier used by the visited-checkpoint registry. Must be
     *  unique within the level. If left blank, GetCheckpointId() falls back to
     *  the actor's name — fine for one-off level layouts, but designers should
     *  set this explicitly so renames in the editor don't break fast-travel
     *  persistence. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
    FName CheckpointId;

    /** True for the single checkpoint inside the Camp level. Changes the
     *  "leave to Open World" affordance into "leave Camp" — i.e. uses the
     *  travel subsystem's saved-transform return instead of routing to a
     *  level's OW portal node. There should be exactly ONE camp checkpoint
     *  across the whole project. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
    bool bIsCampCheckpoint = false;

    /** Designer-friendly display label used in the fast-travel list. Falls
     *  back to CheckpointId.ToString() if blank. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Checkpoint")
    FText DisplayName;

    /** Stable identifier (CheckpointId, falling back to actor name). */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Checkpoint")
    FName GetCheckpointId() const;

    /** True while the player pawn is overlapping the sphere. The pawn reads
     *  this to decide whether E should trigger Rest. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Checkpoint")
    bool IsPlayerInRange() const { return bPlayerInRange; }

    /** Execute the rest action. Caller must already have confirmed the player
     *  is in range. Heals the party, clears danger, resets all non-boss
     *  encounters. Safe to call from C++ or BP. */
    UFUNCTION(BlueprintCallable, Category = "Checkpoint")
    void Rest(APawn* Resting);

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

    UPROPERTY(BlueprintReadOnly, Category = "Checkpoint")
    bool bPlayerInRange = false;

#if !UE_BUILD_SHIPPING
    /** Draws a green "E - Rest" billboard above the actor while in range. */
    void DrawInteractPrompt();
#endif
};
