#include "Exploration/ExplorationPawn.h"
#include "Exploration/EnemyEncounter.h"
#include "Exploration/Checkpoint.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"  // EKeys for the C-key direct fallback
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Engine/Engine.h"  // GEngine for on-screen heal hint
#include "TimerManager.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"
#include "EngineUtils.h"  // TActorIterator
#include "Exploration/JrpgGameMode.h"
#include "Exploration/EnemyDetectionComponent.h"
#include "UI/StatShopWidget.h"
#include "UI/SkillTreeWidget.h"
#include "Kismet/GameplayStatics.h"

AExplorationPawn::AExplorationPawn()
{
    // Tick to count down the gun cooldown timer. Cheap when not on cooldown.
    PrimaryActorTick.bCanEverTick = true;

    // -------------------------------------------------------------------------
    //  Capsule + movement defaults
    // -------------------------------------------------------------------------

    GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

    // Don't rotate the character with the controller — let movement orient it.
    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw   = false;
    bUseControllerRotationRoll  = false;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->bOrientRotationToMovement = true;
        Move->RotationRate              = FRotator(0.f, 540.f, 0.f);
        Move->JumpZVelocity             = 600.f;
        Move->AirControl                = 0.2f;
        Move->MaxWalkSpeed              = 500.f;
        Move->NavAgentProps.bCanCrouch  = true;  // required for Crouch()/UnCrouch()
    }

    // -------------------------------------------------------------------------
    //  Camera rig — third-person follow
    // -------------------------------------------------------------------------

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(RootComponent);
    SpringArm->TargetArmLength       = 400.f;
    SpringArm->bUsePawnControlRotation = true;
    SpringArm->bEnableCameraLag      = true;
    SpringArm->CameraLagSpeed        = 10.f;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
    FollowCamera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
    FollowCamera->bUsePawnControlRotation = false;
}

void AExplorationPawn::BeginPlay()
{
    Super::BeginPlay();

    // Force-enable crouch at runtime so the BP's saved NavAgentProps can't
    // disable it. The constructor already sets this, but existing BP assets
    // serialized before that line have bCanCrouch=false baked in.
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->NavAgentProps.bCanCrouch = true;
        Move->SetCrouchedHalfHeight(48.f);  // standard half-capsule when crouched
    }

    // Register the Enhanced Input mapping context for this pawn.
    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            if (ExplorationMappingContext)
            {
                Subsystem->AddMappingContext(ExplorationMappingContext, 0);
            }
        }

        // Spawn the exploration HUD (crosshair + reload bar). The widget reads
        // IsAiming() and GetGunCooldownPercent() each frame to draw itself.
        if (ExplorationHUDClass)
        {
            ExplorationHUD = CreateWidget<UUserWidget>(PC, ExplorationHUDClass);
            if (ExplorationHUD) { ExplorationHUD->AddToViewport(); }
        }
    }
}

void AExplorationPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ExplorationHUD)
    {
        ExplorationHUD->RemoveFromParent();
        ExplorationHUD = nullptr;
    }
    Super::EndPlay(EndPlayReason);
}

void AExplorationPawn::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (CooldownRemaining > 0.f)
    {
        CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);
    }

    if (bIsAssassinating)
    {
        TickAssassination(DeltaTime);
    }
}

float AExplorationPawn::GetGunCooldownPercent() const
{
    if (GunCooldown <= 0.f) { return 1.f; }
    return FMath::Clamp(1.f - (CooldownRemaining / GunCooldown), 0.f, 1.f);
}

void AExplorationPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction) { EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AExplorationPawn::HandleMove); }
        if (LookAction) { EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &AExplorationPawn::HandleLook); }

        // Jump uses ACharacter's built-in helpers — Started fires once when the
        // key is pressed, Completed fires when released. StopJumping cancels
        // any held jump so a held key doesn't bunny-hop.
        if (JumpAction)
        {
            EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &ACharacter::Jump);
            EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        }

        // Aim is a hold action — Started enters aim mode, Completed leaves it.
        if (AimAction)
        {
            EIC->BindAction(AimAction, ETriggerEvent::Started,   this, &AExplorationPawn::HandleAimStart);
            EIC->BindAction(AimAction, ETriggerEvent::Completed, this, &AExplorationPawn::HandleAimEnd);
        }

        // Fire and ConeShot are single-press actions.
        if (FireAction)
        {
            EIC->BindAction(FireAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleFire);
        }
        if (ConeShotAction)
        {
            EIC->BindAction(ConeShotAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleConeShot);
        }
        // Crouch — toggled on key-down. Prefer the Enhanced Input asset if the
        // designer assigned one in BP defaults; otherwise fall through to the
        // direct-key fallback below so C still works out of the box.
        if (CrouchAction)
        {
            EIC->BindAction(CrouchAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleCrouchToggle);
        }
        if (AssassinateAction)
        {
            EIC->BindAction(AssassinateAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleAssassinate);
        }
        if (InteractAction)
        {
            EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleInteract);
        }
        if (HealAction)
        {
            EIC->BindAction(HealAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleHeal);
        }
        if (PartyPanelAction)
        {
            EIC->BindAction(PartyPanelAction, ETriggerEvent::Started,   this, &AExplorationPawn::HandlePartyPanelOpen);
            EIC->BindAction(PartyPanelAction, ETriggerEvent::Completed, this, &AExplorationPawn::HandlePartyPanelClose);
        }
        if (ShopAction)
        {
            EIC->BindAction(ShopAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleToggleShop);
        }
        if (SkillTreeAction)
        {
            EIC->BindAction(SkillTreeAction, ETriggerEvent::Started, this, &AExplorationPawn::HandleToggleSkillTree);
        }
    }

    // Direct-key fallback for crouch — binds the C key on the raw input
    // component so the toggle works even if no IA_Crouch asset has been
    // wired into IMC_Exploration / BP_ExploartionPawn. Safe to leave in
    // permanently: if both bindings exist, pressing C still toggles once
    // because the Enhanced Input action fires the same handler and we just
    // get one extra call per press (idempotent on the boolean toggle... no,
    // actually it would double-toggle). To avoid double-toggle, this
    // fallback only binds when CrouchAction is unassigned.
    if (PlayerInputComponent && !CrouchAction)
    {
        PlayerInputComponent->BindKey(EKeys::C, IE_Pressed, this, &AExplorationPawn::HandleCrouchToggle);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Crouch bound to C key via direct fallback (no IA_Crouch assigned)"));
    }

    // Same pattern for assassination — Q key fallback if IA_Assassinate isn't
    // wired into IMC_Exploration / BP_ExploartionPawn.
    if (PlayerInputComponent && !AssassinateAction)
    {
        PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AExplorationPawn::HandleAssassinate);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Assassinate bound to Q key via direct fallback (no IA_Assassinate assigned)"));
    }

    // E-key fallback for interact (checkpoints, etc.).
    if (PlayerInputComponent && !InteractAction)
    {
        PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AExplorationPawn::HandleInteract);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Interact bound to E key via direct fallback (no IA_Interact assigned)"));
    }

    // H-key fallback for healing protocol use.
    if (PlayerInputComponent && !HealAction)
    {
        PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AExplorationPawn::HandleHeal);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Heal bound to H key via direct fallback (no IA_Heal assigned)"));
    }

    // Tab-key fallback for the party panel (hold to open).
    if (PlayerInputComponent && !PartyPanelAction)
    {
        PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed,  this, &AExplorationPawn::HandlePartyPanelOpen);
        PlayerInputComponent->BindKey(EKeys::Tab, IE_Released, this, &AExplorationPawn::HandlePartyPanelClose);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Party panel bound to Tab key via direct fallback (no IA_PartyPanel assigned)"));
    }

    // K-key fallback for the stat shop (toggle; opens only at a checkpoint).
    if (PlayerInputComponent && !ShopAction)
    {
        PlayerInputComponent->BindKey(EKeys::K, IE_Pressed, this, &AExplorationPawn::HandleToggleShop);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Stat shop bound to K key via direct fallback (no IA_Shop assigned)"));
    }

    // J-key fallback for the skill tree (toggle; opens anywhere).
    if (PlayerInputComponent && !SkillTreeAction)
    {
        PlayerInputComponent->BindKey(EKeys::J, IE_Pressed, this, &AExplorationPawn::HandleToggleSkillTree);
        UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Skill tree bound to J key via direct fallback (no IA_SkillTree assigned)"));
    }
}

// -----------------------------------------------------------------------------
//  Gun handlers
// -----------------------------------------------------------------------------

void AExplorationPawn::HandleAimStart()
{
    // Block exploration aim/fire while combat owns input — otherwise RMB/LMB in
    // combat also triggers the exploration shot trace (visible as a stray
    // debug-line "laser" coming from the hidden exploration pawn).
    if (AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->GetWorldMode() != EWorldMode::Exploring) { return; }
    }

    bIsAiming = true;

    // Movement penalty — slower walk feels more deliberate while aiming.
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed = AimWalkSpeed;
        // Strafe mode: character faces wherever the camera looks (instead of
        // turning toward movement direction). Lets the player aim sideways
        // while still walking forward, and matches typical TPS aim feel.
        Move->bOrientRotationToMovement = false;
    }
    bUseControllerRotationYaw = true;

    // Camera shift — pull the spring arm in close and push it to the right
    // shoulder so the crosshair stops sitting on the character's back.
    if (SpringArm)
    {
        SpringArm->TargetArmLength = AimArmLength;
        SpringArm->SocketOffset    = AimSocketOffset;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPawn] Aim ON"));
}

void AExplorationPawn::HandleAimEnd()
{
    bIsAiming = false;

    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->MaxWalkSpeed              = NormalWalkSpeed;
        Move->bOrientRotationToMovement = true;
    }
    bUseControllerRotationYaw = false;

    if (SpringArm)
    {
        SpringArm->TargetArmLength = NormalArmLength;
        SpringArm->SocketOffset    = FVector::ZeroVector;
    }

    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPawn] Aim OFF"));
}

void AExplorationPawn::HandleFire()
{
    // Block exploration fire while combat owns input.
    if (AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->GetWorldMode() != EWorldMode::Exploring) { return; }
    }

    // Regular fire requires aim mode — this prevents accidental clicks while
    // running around. Cone shot (F) doesn't need aim because it's a quick
    // close-range interrupt that should be reachable instantly.
    if (!bIsAiming)         { return; }
    if (!IsGunReady())      { return; }

    CooldownRemaining = GunCooldown;

    AEnemyEncounter* Hit = TraceForEncounter();
    if (Hit)
    {
        Hit->Stun(StunDuration);
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Gun shot hit %s — stunned %.1fs"),
            *Hit->GetName(), StunDuration);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Gun shot fired — missed"));
    }
}

void AExplorationPawn::HandleConeShot()
{
    if (!IsGunReady())  { return; }
    if (bIsCastingCone) { return; }

    // Don't trigger a new encounter while one is already starting / underway.
    // Without this, an overlap-triggered combat that fires the same frame as
    // a cone shot would race: the cone shot lands on the same encounter mid-
    // teleport and TriggerCombat → BeginEncounter prints
    // "BeginEncounter ignored — already in combat".
    if (AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        if (GM->GetWorldMode() != EWorldMode::Exploring) { return; }
    }

    CooldownRemaining = GunCooldown;

    // Lock movement for the cast — kill in-flight velocity so the character
    // doesn't slide, then clear the flag via timer. HandleMove early-outs
    // while bIsCastingCone is true.
    bIsCastingCone = true;
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }
    if (ConeCastLockDuration > 0.f && GetWorldTimerManager().IsTimerActive(ConeCastLockTimer) == false)
    {
        GetWorldTimerManager().SetTimer(ConeCastLockTimer, this,
            &AExplorationPawn::EndConeCastLock, ConeCastLockDuration, false);
    }
    else if (ConeCastLockDuration <= 0.f)
    {
        bIsCastingCone = false;
    }

    TArray<AEnemyEncounter*> Targets;
    GatherEncountersInCone(Targets);

    if (Targets.Num() == 0)
    {
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Cone shot fired — no encounter in range"));
        return;
    }

    // Pick the closest encounter inside the cone and start combat with player
    // initiative. The cone is short-range and front-facing so there should
    // usually only be one candidate, but if multiple, closest is fairest.
    AEnemyEncounter* Closest = nullptr;
    float ClosestDistSq = TNumericLimits<float>::Max();
    const FVector MyLoc = GetActorLocation();
    for (AEnemyEncounter* E : Targets)
    {
        if (!E) { continue; }
        const float DistSq = FVector::DistSquared(MyLoc, E->GetActorLocation());
        if (DistSq < ClosestDistSq) { ClosestDistSq = DistSq; Closest = E; }
    }

    if (Closest)
    {
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Cone shot landed on %s — player initiative"),
            *Closest->GetName());
        Closest->TriggerCombat(/*bPlayerHasInitiative=*/true);
    }
}

// -----------------------------------------------------------------------------
//  Assassination — channeled (2.5s)
//
//  Press Q while in a valid stealth position → starts a timed channel during
//  which the player is frozen. Aborts on:
//    - target state change (turns, moves out of range, alerted, chasing)
//    - any OTHER encounter's detection rising above CounterDetectionKillThreshold
//      (the "caught" branch — that detector then attacks with enemy initiative)
//  Completes after AssassinationChannelTime → instant-kill if overleveled,
//  otherwise combat with player initiative.
//
//  Visual feedback is owned entirely by the in-world UI (Q icon + progress bar
//  on the encounter, ground ring for the range). UE_LOG goes to Output Log for
//  debugging; no on-screen text.
// -----------------------------------------------------------------------------

void AExplorationPawn::HandleAssassinate()
{
    // Pressing Q while already channeling is a no-op — players who mash the key
    // shouldn't accidentally reset the timer.
    if (bIsAssassinating) { return; }

    // Find the closest Ready target. UI already tells the player whether they
    // can assassinate (green Q icon), so we don't need to explain failures.
    AEnemyEncounter* ReadyBest    = nullptr;
    float            ReadyBestDsq = TNumericLimits<float>::Max();
    const FVector    MyLoc        = GetActorLocation();

    for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
    {
        AEnemyEncounter* Enc = *It;
        if (!Enc) { continue; }
        if (Enc->GetAssassinationStatus(this) != EAssassinationStatus::Ready) { continue; }

        const float DSq = FVector::DistSquared(MyLoc, Enc->GetActorLocation());
        if (DSq < ReadyBestDsq) { ReadyBestDsq = DSq; ReadyBest = Enc; }
    }

    if (ReadyBest)
    {
        StartAssassination(ReadyBest);
    }
}

float AExplorationPawn::GetAssassinationProgress() const
{
    if (!bIsAssassinating || AssassinationChannelTime <= 0.f) { return 0.f; }
    return FMath::Clamp(AssassinationElapsed / AssassinationChannelTime, 0.f, 1.f);
}

AEnemyEncounter* AExplorationPawn::GetAssassinationTarget() const
{
    return CurrentAssassinationTarget.Get();
}

void AExplorationPawn::StartAssassination(AEnemyEncounter* Target)
{
    if (!Target) { return; }

    bIsAssassinating           = true;
    AssassinationElapsed       = 0.f;
    CurrentAssassinationTarget = Target;

    // Lock movement — character is committed to the strike.
    if (UCharacterMovementComponent* Move = GetCharacterMovement())
    {
        Move->StopMovementImmediately();
    }

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Assassination channel STARTED on %s (%.1fs)"),
           *Target->GetName(), AssassinationChannelTime);
}

void AExplorationPawn::TickAssassination(float DeltaTime)
{
    AEnemyEncounter* Target = CurrentAssassinationTarget.Get();
    if (!Target)
    {
        CancelAssassination(TEXT("Target lost."));
        return;
    }

    // 1. Target itself must remain in Ready state — turning away, alert, chase
    //    all break the channel. UI (Q icon turning grey + ring colour change)
    //    already communicates what happened, so we just log + abort.
    const EAssassinationStatus St = Target->GetAssassinationStatus(this);
    if (St != EAssassinationStatus::Ready)
    {
        CancelAssassination(TEXT("Target state changed."));
        return;
    }

    // 2. Counter-detection — any OTHER encounter spotting you past the threshold
    //    is fatal. That detector then attacks with full initiative.
    for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
    {
        AEnemyEncounter* Other = *It;
        if (!Other || Other == Target || !Other->Detection) { continue; }
        if (Other->Detection->DetectionMeter > CounterDetectionKillThreshold)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[ExplorationPawn] CAUGHT mid-assassination by %s (detect %.0f%%)"),
                *Other->GetName(), Other->Detection->DetectionMeter * 100.f);

            // Reset our flags BEFORE triggering combat — otherwise the new battle
            // start will land while we're still "channeling".
            bIsAssassinating = false;
            CurrentAssassinationTarget = nullptr;
            AssassinationElapsed = 0.f;

            Other->TriggerCombat(/*bPlayerHasInitiative=*/false);
            return;
        }
    }

    // 3. Tick the channel timer.
    AssassinationElapsed += DeltaTime;
    if (AssassinationElapsed >= AssassinationChannelTime)
    {
        CompleteAssassination();
    }
}

void AExplorationPawn::CompleteAssassination()
{
    AEnemyEncounter* Target = CurrentAssassinationTarget.Get();
    bIsAssassinating           = false;
    AssassinationElapsed       = 0.f;
    CurrentAssassinationTarget = nullptr;

    if (!Target) { return; }

    if (Target->IsOverleveledForAssassination())
    {
        UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Assassinated %s — instant kill."),
               *Target->GetName());
        Target->Assassinate(this);
    }
    else
    {
        UE_LOG(LogTemp, Log,
               TEXT("[ExplorationPawn] Stealth strike on %s — combat (player initiative)."),
               *Target->GetName());
        Target->TriggerCombat(/*bPlayerHasInitiative=*/true);
    }
}

void AExplorationPawn::CancelAssassination(const FString& Reason)
{
    if (!bIsAssassinating) { return; }

    bIsAssassinating           = false;
    AssassinationElapsed       = 0.f;
    CurrentAssassinationTarget = nullptr;

    UE_LOG(LogTemp, Log, TEXT("[ExplorationPawn] Assassination cancelled — %s"), *Reason);
}

void AExplorationPawn::HandleCrouchToggle()
{
    bIsCrouching = !bIsCrouching;

    // Visual crouch — lowers the capsule via ACharacter's built-in system.
    // Requires Move->NavAgentProps.bCanCrouch = true (set in the constructor).
    if (bIsCrouching) { Crouch();   }
    else              { UnCrouch(); }

    // Push the new stealth multiplier to UEnemyDetectionComponent — every
    // detection component reads it each tick when computing effective
    // distance/radius.
    const float Mult = bIsCrouching ? CrouchDetectionMultiplier : 1.f;
    UEnemyDetectionComponent::SetPlayerStealthMultiplier(this, Mult);

    UE_LOG(LogTemp, Warning, TEXT("[ExplorationPawn] Crouch %s (stealth multiplier x%.2f)"),
        bIsCrouching ? TEXT("ON") : TEXT("OFF"), Mult);
}

// -----------------------------------------------------------------------------
//  Trace helpers
// -----------------------------------------------------------------------------

AEnemyEncounter* AExplorationPawn::TraceForEncounter() const
{
    // Trace from the camera viewpoint so the bullet path matches the
    // screen-center crosshair exactly. Using actor location instead would
    // diverge from the crosshair once the camera is over-the-shoulder
    // (right-offset spring arm during aim).
    FVector CamLoc;
    FRotator CamRot;
    if (const AController* C = GetController())
    {
        C->GetPlayerViewPoint(CamLoc, CamRot);
    }
    else
    {
        CamLoc = GetActorLocation() + FVector(0.f, 0.f, 50.f);
        CamRot = GetControlRotation();
    }
    const FVector Start = CamLoc;
    const FVector End   = Start + CamRot.Vector() * GunRange;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(ExplorationGunTrace), false, this);

    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(
        Hit, Start, End, ECC_Visibility, Params);

#if !UE_BUILD_SHIPPING
    // Visual ray leaves the character's right hand (or chest as fallback) so
    // it looks like the bullet comes from the gun, while the gameplay trace
    // above still goes through the crosshair for accuracy.
    FVector VisualStart;
    if (USkeletalMeshComponent* MeshComp = GetMesh();
        MeshComp && MeshComp->DoesSocketExist(TEXT("hand_r")))
    {
        VisualStart = MeshComp->GetSocketLocation(TEXT("hand_r"));
    }
    else
    {
        VisualStart = GetActorLocation() + FVector(0.f, 0.f, 50.f);
    }
    const FVector VisualEnd = bHit ? Hit.ImpactPoint : End;
    DrawDebugLine(GetWorld(), VisualStart, VisualEnd,
        bHit ? FColor::Green : FColor::Red, false, 1.5f, 0, 1.f);
#endif

    if (!bHit) { return nullptr; }
    return Cast<AEnemyEncounter>(Hit.GetActor());
}

// -----------------------------------------------------------------------------
//  Interact (checkpoints / rest points)
// -----------------------------------------------------------------------------

void AExplorationPawn::HandleInteract()
{
    // Don't try to interact mid-channel — assassination has its own state machine.
    if (bIsAssassinating || bIsCastingCone) { return; }

    UWorld* World = GetWorld();
    if (!World) { return; }

    // Pick the closest in-range checkpoint. Multiple overlapping is unlikely
    // but handle it gracefully.
    ACheckpoint* Best     = nullptr;
    float        BestDist = TNumericLimits<float>::Max();
    for (TActorIterator<ACheckpoint> It(World); It; ++It)
    {
        ACheckpoint* CP = *It;
        if (!CP || !CP->IsPlayerInRange()) { continue; }
        const float D = FVector::DistSquared(CP->GetActorLocation(), GetActorLocation());
        if (D < BestDist)
        {
            BestDist = D;
            Best     = CP;
        }
    }

    if (Best)
    {
        Best->Rest(this);
    }
}

// -----------------------------------------------------------------------------
//  Stat shop (opened at checkpoints)
// -----------------------------------------------------------------------------

void AExplorationPawn::HandleToggleShop()
{
    if (bShopOpen) { CloseStatShop(); }
    else           { OpenStatShop(); }
}

void AExplorationPawn::OpenStatShop()
{
    if (bShopOpen || bIsAssassinating || bIsCastingCone) { return; }
    if (bSkillTreeOpen) { CloseSkillTree(); }   // mutually exclusive menus

    UWorld* World = GetWorld();
    if (!World) { return; }

    // Gate: only at a checkpoint.
    bool bAtCheckpoint = false;
    for (TActorIterator<ACheckpoint> It(World); It; ++It)
    {
        if (*It && (*It)->IsPlayerInRange()) { bAtCheckpoint = true; break; }
    }
    if (!bAtCheckpoint)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
                TEXT("Rest at a checkpoint to access the stat shop"));
        }
        return;
    }

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) { return; }

    UClass* WidgetClass = StatShopClass ? StatShopClass.Get() : UStatShopWidget::StaticClass();
    StatShopWidget = CreateWidget<UStatShopWidget>(PC, WidgetClass);
    if (!StatShopWidget) { return; }

    StatShopWidget->OnCloseRequested = [this]() { CloseStatShop(); };
    StatShopWidget->AddToViewport(50);
    bShopOpen = true;

    PC->SetShowMouseCursor(true);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(StatShopWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);
}

void AExplorationPawn::CloseStatShop()
{
    if (StatShopWidget)
    {
        StatShopWidget->RemoveFromParent();
        StatShopWidget = nullptr;
    }
    bShopOpen = false;

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

// -----------------------------------------------------------------------------
//  Skill tree (opens anywhere via J)
// -----------------------------------------------------------------------------

void AExplorationPawn::HandleToggleSkillTree()
{
    if (bSkillTreeOpen) { CloseSkillTree(); }
    else                { OpenSkillTree(); }
}

void AExplorationPawn::OpenSkillTree()
{
    if (bSkillTreeOpen || bIsAssassinating || bIsCastingCone) { return; }
    if (bShopOpen) { CloseStatShop(); }   // mutually exclusive menus

    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) { return; }

    UClass* WidgetClass = SkillTreeClass ? SkillTreeClass.Get() : USkillTreeWidget::StaticClass();
    SkillTreeWidget = CreateWidget<USkillTreeWidget>(PC, WidgetClass);
    if (!SkillTreeWidget) { return; }

    SkillTreeWidget->OnCloseRequested = [this]() { CloseSkillTree(); };
    SkillTreeWidget->AddToViewport(50);
    bSkillTreeOpen = true;

    PC->SetShowMouseCursor(true);
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(SkillTreeWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(Mode);
}

void AExplorationPawn::CloseSkillTree()
{
    if (SkillTreeWidget)
    {
        SkillTreeWidget->RemoveFromParent();
        SkillTreeWidget = nullptr;
    }
    bSkillTreeOpen = false;

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        PC->SetShowMouseCursor(false);
        PC->SetInputMode(FInputModeGameOnly());
    }
}

// -----------------------------------------------------------------------------
//  Healing protocol use (overworld)
// -----------------------------------------------------------------------------

void AExplorationPawn::HandleHeal()
{
    if (bIsAssassinating || bIsCastingCone) { return; }

    // Heal only works while the party panel is open (hold Tab).
    if (!bPartyPanelOpen)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.f, FColor::Yellow,
                TEXT("Hold Tab to open the party panel, then press H to heal"));
        }
        return;
    }

    if (AJrpgGameMode* GM = Cast<AJrpgGameMode>(UGameplayStatics::GetGameMode(this)))
    {
        GM->UseHealingProtocolOverworld();
    }
}

void AExplorationPawn::HandlePartyPanelOpen()
{
    bPartyPanelOpen = true;
}

void AExplorationPawn::HandlePartyPanelClose()
{
    bPartyPanelOpen = false;
}

void AExplorationPawn::GatherEncountersInCone(TArray<AEnemyEncounter*>& Out) const
{
    Out.Reset();

    const FVector Origin  = GetActorLocation();
    // Use the controller forward (camera/look direction) so the cone aims
    // where the player is mouse-looking, not where the character body faces.
    const FVector Forward = GetControlRotation().Vector();
    const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDeg));

    // Direct actor iteration is simpler and more reliable than a sphere
    // overlap query — encounters are rare (a handful per level) and we sidestep
    // any collision-channel weirdness with their trigger sphere. Filter by
    // distance, then by cone half-angle.
    int32 Inspected = 0;
    for (TActorIterator<AEnemyEncounter> It(GetWorld()); It; ++It)
    {
        AEnemyEncounter* Encounter = *It;
        if (!Encounter) { continue; }
        ++Inspected;

        const FVector ToTarget = Encounter->GetActorLocation() - Origin;
        const float Dist = ToTarget.Size();
        if (Dist > ConeRange) { continue; }

        // If the encounter is essentially on top of us, count it without
        // doing the angle check (avoids dividing a near-zero vector).
        if (Dist > KINDA_SMALL_NUMBER)
        {
            const FVector Dir = ToTarget / Dist;
            if (FVector::DotProduct(Forward, Dir) < CosHalfAngle) { continue; }
        }

        Out.Add(Encounter);
    }

    UE_LOG(LogTemp, Verbose, TEXT("[ExplorationPawn] Cone scan: %d encounters inspected, %d in cone"),
        Inspected, Out.Num());

#if !UE_BUILD_SHIPPING
    // Yellow wireframe cone — visualises range + spread. Draw from the
    // character's right hand so it looks like it leaves the gun, not the
    // actor pivot at their feet.
    FVector ConeVisualOrigin = Origin;
    if (USkeletalMeshComponent* MeshComp = GetMesh();
        MeshComp && MeshComp->DoesSocketExist(TEXT("hand_r")))
    {
        ConeVisualOrigin = MeshComp->GetSocketLocation(TEXT("hand_r"));
    }
    DrawDebugCone(GetWorld(), ConeVisualOrigin, Forward, ConeRange,
        FMath::DegreesToRadians(ConeHalfAngleDeg),
        FMath::DegreesToRadians(ConeHalfAngleDeg),
        12, FColor::Yellow, false, 1.5f, 0, 1.f);
#endif
}

void AExplorationPawn::HandleMove(const FInputActionValue& Value)
{
    // Cone-shot cast and assassination channel both lock the character in place.
    if (bIsCastingCone)    { return; }
    if (bIsAssassinating)  { return; }
    if (bShopOpen)         { return; }   // frozen while the stat shop is open
    if (bSkillTreeOpen)    { return; }   // frozen while the skill tree is open

    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller || Axis.IsNearlyZero()) { return; }

    // Use the controller yaw so movement is camera-relative.
    const FRotator YawOnly(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector Forward = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::X);
    const FVector Right   = FRotationMatrix(YawOnly).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Axis.Y);
    AddMovementInput(Right,   Axis.X);
}

void AExplorationPawn::HandleLook(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(-Axis.Y);
}

void AExplorationPawn::EndConeCastLock()
{
    bIsCastingCone = false;
}
