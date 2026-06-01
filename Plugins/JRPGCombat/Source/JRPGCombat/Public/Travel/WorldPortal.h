#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WorldPortal.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class APawn;

/**
 * AWorldPortal
 *
 * Designer-placeable travel trigger. Drop in a level, configure TargetLevel +
 * TargetArrivalTag in the details panel, and on player interact it calls the
 * UJrpgTravelSubsystem to open the target level and route the pawn to the
 * matching ATravelArrivalPoint.
 *
 * Modeled after ACheckpoint: a static mesh + an overlap sphere. The player
 * pawn polls IsPlayerInRange() each tick and binds the travel key to Use().
 * (The pawn-side keybind is added by the integration task that places these
 * actors — this class is a pure trigger.)
 *
 * Typical placements:
 *   - At the entry area of every Level: TargetLevel = L_Overworld,
 *     TargetArrivalTag = "Portal_<ThisLevelName>".
 *   - At each Level node in the Open World: TargetLevel = <that Level>,
 *     TargetArrivalTag = "FromOverworld".
 *
 * For Camp entry — DON'T use this actor. Camp is entered by a global keybind
 * anywhere in the Open World; see UJrpgTravelSubsystem::EnterCampFromOpenWorld.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBAT_API AWorldPortal : public AActor
{
    GENERATED_BODY()

public:

    AWorldPortal();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    TObjectPtr<UStaticMeshComponent> Mesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Portal")
    TObjectPtr<USphereComponent> InteractSphere;

    /** Interact radius in cm. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal",
              meta = (ClampMin = "50.0"))
    float InteractRadius = 250.f;

    /** Level to open when the player interacts. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FName TargetLevel;

    /** Arrival tag inside TargetLevel — must match an ATravelArrivalPoint
     *  placed there. NAME_None falls back to the level's PlayerStart. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FName TargetArrivalTag;

    /** Prompt label rendered above the portal when the player is in range. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString PromptLabel = TEXT("[T] Travel");

    /** True while the player pawn is overlapping the interact sphere. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Portal")
    bool IsPlayerInRange() const { return bPlayerInRange; }

    /** Trigger travel. Safe to call from C++ or BP — pawn-side keybind should
     *  gate on IsPlayerInRange before calling. */
    UFUNCTION(BlueprintCallable, Category = "Portal")
    void Use(APawn* User);

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

    UPROPERTY(BlueprintReadOnly, Category = "Portal")
    bool bPlayerInRange = false;

#if !UE_BUILD_SHIPPING
    /** Draws a cyan prompt billboard while in range. */
    void DrawInteractPrompt();
#endif
};
