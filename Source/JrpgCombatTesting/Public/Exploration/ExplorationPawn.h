#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ExplorationPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class UUserWidget;
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

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> JumpAction;

    /** Hold RMB to enter aim mode. Required before firing the regular shot. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> AimAction;

    /** LMB to fire the regular gun shot (only works while aiming). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> FireAction;

    /** F to fire the cone shot — quick interrupt that grants player initiative. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> ConeShotAction;

    // -------------------------------------------------------------------------
    //  Gun — tunables
    // -------------------------------------------------------------------------

    /** Shared cooldown between regular fire and cone shot, in seconds. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "0.0"))
    float GunCooldown = 3.f;

    /** Max line-trace range for the regular shot (cm). 50m default. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "0.0"))
    float GunRange = 5000.f;

    /** How long an enemy is stunned after a successful gun hit (seconds). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "0.0"))
    float StunDuration = 2.f;

    /** Cone shot range in cm. 6m default per design spec. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "0.0"))
    float ConeRange = 600.f;

    /** Cone shot half-angle in degrees. 22.5 = 45° total spread. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float ConeHalfAngleDeg = 22.5f;

    // -------------------------------------------------------------------------
    //  Exploration HUD
    //  Assign a UUserWidget Blueprint (e.g. WBP_ExplorationHUD) here. The pawn
    //  spawns it on BeginPlay and removes it on EndPlay. The widget reads
    //  IsAiming() and GetGunCooldownPercent() to draw the crosshair + reload bar.
    // -------------------------------------------------------------------------

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|UI")
    TSubclassOf<UUserWidget> ExplorationHUDClass;

    UPROPERTY(BlueprintReadOnly, Category = "Exploration|UI")
    TObjectPtr<UUserWidget> ExplorationHUD;

    // -------------------------------------------------------------------------
    //  Gun queries — bind these in the HUD widget
    // -------------------------------------------------------------------------

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Gun")
    bool IsAiming() const { return bIsAiming; }

    /** 1.0 = fully reloaded and ready to fire, 0.0 = just fired. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Gun")
    float GetGunCooldownPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Gun")
    bool IsGunReady() const { return CooldownRemaining <= 0.f; }

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void Tick(float DeltaTime) override;
    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    void HandleMove(const FInputActionValue& Value);
    void HandleLook(const FInputActionValue& Value);

    void HandleAimStart();
    void HandleAimEnd();
    void HandleFire();
    void HandleConeShot();

    /** Helper: line trace forward from the camera. Returns the encounter hit, if any. */
    class AEnemyEncounter* TraceForEncounter() const;

    /** Helper: gather all encounters within the cone in front of the pawn. */
    void GatherEncountersInCone(TArray<class AEnemyEncounter*>& Out) const;

    // Runtime state
    bool  bIsAiming         = false;
    float CooldownRemaining = 0.f;
};
