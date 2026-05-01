#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ExplorationPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 * AExplorationPawn
 *
 * The character the player controls outside of combat.
 *
 * Phase 1 — single visible character. Walks around the level with WASD,
 * camera follows behind via spring arm. When AEnemyEncounter overlaps this
 * pawn, AJrpgGameMode is notified and combat begins.
 *
 * Phase 2 (later) — add 2 trailing party-member visuals so the whole party
 * appears to travel together. The followers are visual-only; combat still
 * uses the placed BP_PlayerFencer / etc. actors at the arena.
 */
UCLASS(Blueprintable, BlueprintType)
class JRPGCOMBATTESTING_API AExplorationPawn : public ACharacter
{
    GENERATED_BODY()

public:

    AExplorationPawn();

    // -------------------------------------------------------------------------
    //  Camera
    // -------------------------------------------------------------------------

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration|Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Exploration|Camera")
    TObjectPtr<UCameraComponent> FollowCamera;

    // -------------------------------------------------------------------------
    //  Input
    //  Assign Enhanced Input assets in Blueprint defaults, or leave null and
    //  drive movement from a Blueprint-only setup.
    // -------------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputMappingContext> ExplorationMappingContext;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> LookAction;

protected:

    virtual void BeginPlay() override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void HandleMove(const FInputActionValue& Value);
    void HandleLook(const FInputActionValue& Value);
};
