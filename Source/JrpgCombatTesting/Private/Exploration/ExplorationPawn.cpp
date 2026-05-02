#include "Exploration/ExplorationPawn.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

AExplorationPawn::AExplorationPawn()
{
    PrimaryActorTick.bCanEverTick = false;

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
    }
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
    }
}

void AExplorationPawn::HandleMove(const FInputActionValue& Value)
{
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
