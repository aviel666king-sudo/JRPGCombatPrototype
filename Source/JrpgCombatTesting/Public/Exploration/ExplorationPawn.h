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

    /** C to toggle crouch — reduces enemy detection radius/distance by
     *  CrouchDetectionMultiplier while held / toggled. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> CrouchAction;

    /** Q to attempt stealth assassination on the nearest encounter in front.
     *  Has a direct-key fallback (like crouch) so it works without an IA asset. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> AssassinateAction;

    /** E to interact with the nearest in-range ACheckpoint. Direct-key fallback
     *  to E if no IA asset is wired. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> InteractAction;

    /** H to spend one Healing Protocol charge — fully heals all living party
     *  members to MaxHP. Direct-key fallback to H. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> HealAction;

    /** Hold Tab to open the party panel (HP bars + protocol charges). Heal (H)
     *  only works while this is open. Direct-key fallback to Tab. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Input")
    TObjectPtr<UInputAction> PartyPanelAction;

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
    float StunDuration = 0.4f;

    /** Cone shot range in cm. 6m default per design spec. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "0.0"))
    float ConeRange = 600.f;

    /** Cone shot half-angle in degrees. 22.5 = 45° total spread. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "1.0", ClampMax = "89.0"))
    float ConeHalfAngleDeg = 22.5f;

    /** How long the player is locked in place while casting the cone shot (s). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Gun",
              meta = (ClampMin = "0.0"))
    float ConeCastLockDuration = 0.6f;

    // -------------------------------------------------------------------------
    //  Aim mode tunables
    //  Snapped on aim-start / aim-end. SpringArm camera lag smooths the visual.
    // -------------------------------------------------------------------------

    /** Normal exploration walk speed (cm/s). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Aim",
              meta = (ClampMin = "0.0"))
    float NormalWalkSpeed = 500.f;

    /** Walk speed while aiming — slower so aiming feels deliberate. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Aim",
              meta = (ClampMin = "0.0"))
    float AimWalkSpeed = 200.f;

    /** Spring arm length when not aiming. Standard third-person distance. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Aim",
              meta = (ClampMin = "0.0"))
    float NormalArmLength = 400.f;

    /** Spring arm length when aiming — pulls camera in for over-the-shoulder feel. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Aim",
              meta = (ClampMin = "0.0"))
    float AimArmLength = 200.f;

    /** Camera offset applied while aiming to put the crosshair off the character's
     *  back. Y = right shoulder, Z = head height. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Exploration|Aim")
    FVector AimSocketOffset = FVector(0.f, 50.f, 30.f);

    // -------------------------------------------------------------------------
    //  Stealth — see UEnemyDetectionComponent
    // -------------------------------------------------------------------------

    /** Multiplier applied to enemy DetectionDistance/DetectionRadius while
     *  crouched. 0.5 = enemies see you at half their normal range. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Exploration|Stealth",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CrouchDetectionMultiplier = 0.5f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Stealth")
    bool IsCrouching() const { return bIsCrouching; }

    /** True while the player holds Tab. The HUD binds the party-panel
     *  Visibility to this, and HandleHeal refuses unless it's open. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|UI")
    bool IsPartyPanelOpen() const { return bPartyPanelOpen; }

    // -------------------------------------------------------------------------
    //  Assassination — channeled
    // -------------------------------------------------------------------------

    /** Seconds the player must remain in a valid stealth position for the
     *  assassination to land. Movement is locked during the channel. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Exploration|Assassination",
              meta = (ClampMin = "0.1"))
    float AssassinationChannelTime = 2.5f;

    /** If ANY non-target encounter's detection meter exceeds this fraction
     *  during the channel, the player is "killed" — channel aborts and that
     *  encounter triggers combat with enemy initiative. 0.25 = 25%. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Exploration|Assassination",
              meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CounterDetectionKillThreshold = 0.25f;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Assassination")
    bool IsAssassinating() const { return bIsAssassinating; }

    /** 0..1 — how far through the channel the player is. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Assassination")
    float GetAssassinationProgress() const;

    /** The encounter being assassinated. Null when not channeling. */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Exploration|Assassination")
    class AEnemyEncounter* GetAssassinationTarget() const;

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
    void HandleCrouchToggle();
    void HandleAssassinate();
    void HandleInteract();
    void HandleHeal();
    void HandlePartyPanelOpen();
    void HandlePartyPanelClose();

    /** Helper: line trace forward from the camera. Returns the encounter hit, if any. */
    class AEnemyEncounter* TraceForEncounter() const;

    /** Helper: gather all encounters within the cone in front of the pawn. */
    void GatherEncountersInCone(TArray<class AEnemyEncounter*>& Out) const;

    /** Cleared by a timer started in HandleConeShot. */
    void EndConeCastLock();

    /** Begin a 2.5s channel on Target. Locks movement, drives a progress timer
     *  in Tick, aborts if state changes. Caller has already verified the target
     *  is in Ready state. */
    void StartAssassination(class AEnemyEncounter* Target);

    /** Per-frame work — bumps progress, re-checks target state + counter-detection. */
    void TickAssassination(float DeltaTime);

    /** Restore movement and clear channel flags. Reason is logged for debugging. */
    void CancelAssassination(const FString& Reason);

    /** Channel reached AssassinationChannelTime — execute the kill / combat. */
    void CompleteAssassination();

    // Runtime state
    bool  bIsAiming         = false;
    bool  bIsCastingCone    = false;
    bool  bIsCrouching      = false;
    bool  bPartyPanelOpen   = false;
    float CooldownRemaining = 0.f;

    bool  bIsAssassinating     = false;
    float AssassinationElapsed = 0.f;
    TWeakObjectPtr<class AEnemyEncounter> CurrentAssassinationTarget;

    FTimerHandle ConeCastLockTimer;
};
