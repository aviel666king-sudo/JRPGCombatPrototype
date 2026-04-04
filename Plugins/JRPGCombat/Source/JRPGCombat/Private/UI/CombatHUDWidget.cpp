#include "UI/CombatHUDWidget.h"
#include "UI/UnitStatusWidget.h"
#include "UI/TurnInfoWidget.h"
#include "UI/CombatActionPanelWidget.h"
#include "Core/BattleManager.h"
#include "Core/TurnOrderManager.h"
#include "Characters/Base/CombatantBase.h"
#include "Components/AbilityManagerComponent.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"

// -----------------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------------

void UCombatHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // This widget must be focusable so NativeOnKeyDown fires.
    SetIsFocusable(true);
}

void UCombatHUDWidget::NativeDestruct()
{
    if (ABattleManager* BM = BattleManager.Get())
    {
        BM->OnPhaseChanged.RemoveDynamic(this,          &UCombatHUDWidget::OnPhaseChanged);
        BM->OnCombatantTurnStarted.RemoveDynamic(this,  &UCombatHUDWidget::OnCombatantTurnStarted);
        BM->OnActivePlayerChanged.RemoveDynamic(this,   &UCombatHUDWidget::OnActivePlayerChanged);
        BM->OnTargetSelectionChanged.RemoveDynamic(this,&UCombatHUDWidget::OnTargetChanged);
        BM->OnEnemyActingChanged.RemoveDynamic(this,    &UCombatHUDWidget::OnEnemyActingChanged);
        BM->OnBattleEnded.RemoveDynamic(this,           &UCombatHUDWidget::OnBattleEnded);
        BM->OnGunAimChanged.RemoveDynamic(this,         &UCombatHUDWidget::OnGunAimModeChanged);
    }

    Super::NativeDestruct();
}

// -----------------------------------------------------------------------------
//  Initialization
// -----------------------------------------------------------------------------

void UCombatHUDWidget::InitializeHUD(ABattleManager* InBattleManager)
{
    if (!InBattleManager) { return; }

    BattleManager = InBattleManager;

    CollectUnitWidgets(PlayerPartyPanel, PlayerWidgets);
    CollectUnitWidgets(EnemyPartyPanel,  EnemyWidgets);

    InBattleManager->OnPhaseChanged.AddDynamic(this,          &UCombatHUDWidget::OnPhaseChanged);
    InBattleManager->OnCombatantTurnStarted.AddDynamic(this,  &UCombatHUDWidget::OnCombatantTurnStarted);
    InBattleManager->OnActivePlayerChanged.AddDynamic(this,   &UCombatHUDWidget::OnActivePlayerChanged);
    InBattleManager->OnTargetSelectionChanged.AddDynamic(this,&UCombatHUDWidget::OnTargetChanged);
    InBattleManager->OnEnemyActingChanged.AddDynamic(this,    &UCombatHUDWidget::OnEnemyActingChanged);
    InBattleManager->OnBattleEnded.AddDynamic(this,           &UCombatHUDWidget::OnBattleEnded);
    InBattleManager->OnGunAimChanged.AddDynamic(this,         &UCombatHUDWidget::OnGunAimModeChanged);

    // Crosshair starts hidden.
    if (CrosshairWidget)
    {
        CrosshairWidget->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (ActionPanel) { ActionPanel->InitializePanel(InBattleManager, this); }

    RebindUnits();
}

void UCombatHUDWidget::RebindUnits()
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return; }

    for (UUnitStatusWidget* W : PlayerWidgets) { if (W) { W->SetUnit(nullptr); } }
    for (UUnitStatusWidget* W : EnemyWidgets)  { if (W) { W->SetUnit(nullptr); } }

    {
        int32 Idx = 0;
        for (ACombatantBase* C : BM->GetAllCombatants())
        {
            if (!C || C->GetTeam() != ECombatTeam::Player) { continue; }
            if (PlayerWidgets.IsValidIndex(Idx)) { PlayerWidgets[Idx]->SetUnit(C); }
            ++Idx;
        }
    }
    {
        int32 Idx = 0;
        for (ACombatantBase* C : BM->GetAllCombatants())
        {
            if (!C || C->GetTeam() != ECombatTeam::Enemy) { continue; }
            if (EnemyWidgets.IsValidIndex(Idx)) { EnemyWidgets[Idx]->SetUnit(C); }
            ++Idx;
        }
    }

    RefreshAll();
}

// -----------------------------------------------------------------------------
//  Full refresh
// -----------------------------------------------------------------------------

void UCombatHUDWidget::RefreshAll()
{
    for (UUnitStatusWidget* W : PlayerWidgets) { if (W) { W->Refresh(); } }
    for (UUnitStatusWidget* W : EnemyWidgets)  { if (W) { W->Refresh(); } }
    RefreshTurnInfo();
}

// -----------------------------------------------------------------------------
//  BattleManager delegate callbacks
// -----------------------------------------------------------------------------

void UCombatHUDWidget::OnPhaseChanged(EBattlePhase NewPhase)
{
    // Only do a full rebind on Initialization — avoids re-flashing every phase.
    if (NewPhase == EBattlePhase::Initialization)
    {
        RebindUnits();
    }

    // Keep the action panel in sync with BM phase.
    if (ActionPanel)
    {
        switch (NewPhase)
        {
            case EBattlePhase::AwaitingInput:
                // Always refresh unit cards so HP/AP values reflect the latest
                // state — this is what makes Gun shots visually update enemy HP.
                RefreshAll();

                if (ActionPanel->IsInGunMode())
                {
                    // Player is in persistent Gun mode — re-enter target selection
                    // for the same Gun ability instead of returning to MainMenu.
                    // First check the player can still afford the shot.
                    ABattleManager* BM    = BattleManager.Get();
                    ACombatantBase* Actor = BM ? BM->GetActiveCombatant() : nullptr;
                    const int32 GunIdx   = ActionPanel->GetGunAbilityIndex();
                    if (BM && Actor && GunIdx >= 0 &&
                        Actor->AbilityManager->CanActivateAbility(GunIdx))
                    {
                        // BeginTargetSelection fires OnPhaseChanged(SelectingTarget)
                        // which sets the panel back to SelectingTarget.
                        BM->BeginTargetSelection(GunIdx);
                    }
                    else
                    {
                        // Ran out of AP or ability unavailable — exit gun mode.
                        // SetMenuState(MainMenu) clears bInGunMode via its own logic.
                        ActionPanel->SetMenuState(ECombatMenuState::MainMenu);
                    }
                }
                else if (ActionPanel->GetMenuState() == ECombatMenuState::Inactive ||
                         ActionPanel->GetMenuState() == ECombatMenuState::SelectingTarget)
                {
                    // Normal return from a non-gun action — go back to MainMenu.
                    ActionPanel->SetMenuState(ECombatMenuState::MainMenu);
                }
                // If we're already in MainMenu/SkillMenu/ProtocolMenu (shouldn't
                // happen normally) leave the state alone.
                break;

            case EBattlePhase::TurnStart:
            case EBattlePhase::TurnEnd:
            case EBattlePhase::ExecutingAction:
            case EBattlePhase::MinigameActive:
                // Don't change menu state mid-action — the panel stays where it was
                // so there's no flicker between Gun shots, and so a running minigame
                // overlay is never torn down by a phase event.
                break;

            case EBattlePhase::SelectingTarget:
                ActionPanel->SetMenuState(ECombatMenuState::SelectingTarget);
                break;

            default:
                // Enemy turn, Idle, Victory, Defeat → go inactive.
                ActionPanel->SetMenuState(ECombatMenuState::Inactive);
                break;
        }
    }
}

void UCombatHUDWidget::OnCombatantTurnStarted(ACombatantBase* /*ActiveCombatant*/)
{
    // Refresh cards so HP/AP/status values are current.
    // Only full rebind once per turn (not every phase change).
    RebindUnits();
    ClearAllHighlights();
    RefreshTurnInfo();
}

void UCombatHUDWidget::OnActivePlayerChanged(ACombatantBase* NewActivePlayer)
{
    ApplyActivePlayerHighlight(NewActivePlayer);
}

void UCombatHUDWidget::OnTargetChanged(ACombatantBase* NewTarget)
{
    ApplyTargetHighlight(NewTarget);
}

void UCombatHUDWidget::OnEnemyActingChanged(ACombatantBase* NewActingEnemy)
{
    ApplyEnemyActingHighlight(NewActingEnemy);
}

void UCombatHUDWidget::OnBattleEnded(bool /*bVictory*/)
{
    ClearAllHighlights();
    RefreshAll();
    if (ActionPanel) { ActionPanel->SetMenuState(ECombatMenuState::Inactive); }
}

// -----------------------------------------------------------------------------
//  Input
// -----------------------------------------------------------------------------

FReply UCombatHUDWidget::NativeOnKeyDown(const FGeometry& InGeometry,
                                          const FKeyEvent&  InKeyEvent)
{
    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return Super::NativeOnKeyDown(InGeometry, InKeyEvent); }

    const FKey Key = InKeyEvent.GetKey();

    // ── Free player-cursor (Left / Right Arrow) ───────────────────────────────
    //    Available any time the phase is AwaitingInput, regardless of menu state.
    //    This is a HUD-level concern (viewing party status), not a menu concern.
    if (BM->GetCurrentPhase() == EBattlePhase::AwaitingInput)
    {
        if (Key == EKeys::Left)  { BM->SwitchActivePlayer(-1); return FReply::Handled(); }
        if (Key == EKeys::Right) { BM->SwitchActivePlayer(+1); return FReply::Handled(); }
    }

    // ── All other combat input goes to the action panel ───────────────────────
    if (ActionPanel && ActionPanel->HandleKeyDown(Key))
    {
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UCombatHUDWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry,
                                                   const FPointerEvent& InMouseEvent)
{
    ABattleManager* BM = BattleManager.Get();

    // ── Right mouse button — enter gun aim mode ───────────────────────────────
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        if (BM && BM->GetCurrentPhase() == EBattlePhase::AwaitingInput && !bGunAimInputActive)
        {
            BM->BeginGunAimMode();
        }
        // Capture mouse so NativeOnMouseButtonUp fires even after cursor moves.
        TSharedPtr<SWidget> SafeWidget = GetCachedWidget();
        if (SafeWidget.IsValid())
        {
            return FReply::Handled().CaptureMouse(SafeWidget.ToSharedRef());
        }
        return FReply::Handled();
    }

    // ── Left mouse button during gun aim — fire ───────────────────────────────
    if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && bGunAimInputActive)
    {
        if (BM) { BM->FireGunAimShot(); }
        return FReply::Handled();
    }

    // ── Any other click — recapture keyboard focus ────────────────────────────
    RecaptureKeyboardFocus();
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UCombatHUDWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry,
                                                const FPointerEvent& InMouseEvent)
{
    // ── Right mouse button release — exit gun aim mode ────────────────────────
    if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
    {
        ABattleManager* BM = BattleManager.Get();
        if (BM && bGunAimInputActive) { BM->EndGunAimMode(); }
        return FReply::Handled().ReleaseMouseCapture();
    }

    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

// -----------------------------------------------------------------------------
//  Gun aim tick — mouse-delta → camera rotation
// -----------------------------------------------------------------------------

void UCombatHUDWidget::NativeTick(const FGeometry& AllottedGeometry, float InDeltaTime)
{
    Super::NativeTick(AllottedGeometry, InDeltaTime);

    if (!bGunAimInputActive) { return; }

    ABattleManager* BM = BattleManager.Get();
    if (!BM || !BM->IsGunAimActive()) { return; }

    APlayerController* PC = GetOwningPlayer();
    if (!PC) { return; }

    // Use screen-space cursor position so warping works correctly in PIE.
    if (!GEngine || !GEngine->GameViewport) { return; }

    FVector2D ViewportSize;
    GEngine->GameViewport->GetViewportSize(ViewportSize);

    TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
    if (!Window.IsValid()) { return; }

    const FVector2D ScreenCenter = Window->GetPositionInScreen() + ViewportSize * 0.5f;
    const FVector2D CursorPos    = FSlateApplication::Get().GetCursorPos();

    const float DX = CursorPos.X - ScreenCenter.X;
    const float DY = CursorPos.Y - ScreenCenter.Y;

    if (bSkipNextAimTick)
    {
        bSkipNextAimTick = false;
        return;
    }

    if (FMath::Abs(DX) > 0.5f || FMath::Abs(DY) > 0.5f)
    {
        BM->UpdateGunAimRotation(DX, -DY);
        FSlateApplication::Get().SetCursorPos(ScreenCenter);
        bSkipNextAimTick = true;
    }
}

// -----------------------------------------------------------------------------
//  Gun aim mode changed — crosshair, cursor, action panel
// -----------------------------------------------------------------------------

void UCombatHUDWidget::OnGunAimModeChanged(bool bAiming)
{
    bGunAimInputActive = bAiming;

    // Show / hide crosshair.
    if (CrosshairWidget)
    {
        CrosshairWidget->SetVisibility(bAiming
            ? ESlateVisibility::HitTestInvisible   // visible but doesn't block clicks
            : ESlateVisibility::Collapsed);
    }

    // Dim and lock the action panel while aiming so buttons aren't clickable.
    if (ActionPanel)
    {
        if (bAiming)
        {
            ActionPanel->SetRenderOpacity(0.f);
            ActionPanel->SetIsEnabled(false);
        }
        else
        {
            // Restore panel to the main menu — the player is back in AwaitingInput.
            ActionPanel->SetMenuState(ECombatMenuState::MainMenu);
        }
    }

    // Hide the OS cursor while aiming; center it so the first tick reads zero delta.
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->bShowMouseCursor = !bAiming;
        if (bAiming && GEngine && GEngine->GameViewport)
        {
            FVector2D ViewportSize;
            GEngine->GameViewport->GetViewportSize(ViewportSize);
            TSharedPtr<SWindow> Window = GEngine->GameViewport->GetWindow();
            if (Window.IsValid())
            {
                FSlateApplication::Get().SetCursorPos(
                    Window->GetPositionInScreen() + ViewportSize * 0.5f);
            }
        }
    }
}

// -----------------------------------------------------------------------------
//  Focus
// -----------------------------------------------------------------------------

void UCombatHUDWidget::RecaptureKeyboardFocus()
{
    // SetUserFocus directs Slate keyboard events to this widget.
    // GetOwningPlayer() returns the local PlayerController — safe during gameplay.
    if (APlayerController* PC = GetOwningPlayer())
    {
        SetUserFocus(PC);
    }
}

// -----------------------------------------------------------------------------
//  Highlight management
// -----------------------------------------------------------------------------

void UCombatHUDWidget::ApplyActivePlayerHighlight(ACombatantBase* NewActive)
{
    if (UUnitStatusWidget* Prev = ActivePlayerWidget.Get()) { Prev->SetIsActivePlayer(false); }
    ActivePlayerWidget = nullptr;

    if (!NewActive) { return; }
    if (UUnitStatusWidget* Next = FindWidgetFor(NewActive))
    {
        Next->SetIsActivePlayer(true);
        ActivePlayerWidget = Next;
    }
}

void UCombatHUDWidget::ApplyTargetHighlight(ACombatantBase* NewTarget)
{
    if (UUnitStatusWidget* Prev = TargetEnemyWidget.Get()) { Prev->SetIsTargeted(false); }
    TargetEnemyWidget = nullptr;

    if (!NewTarget) { return; }
    if (UUnitStatusWidget* Next = FindWidgetFor(NewTarget))
    {
        Next->SetIsTargeted(true);
        TargetEnemyWidget = Next;
    }
}

void UCombatHUDWidget::ApplyEnemyActingHighlight(ACombatantBase* NewActingEnemy)
{
    // Clear previous orange highlight.
    if (UUnitStatusWidget* Prev = EnemyActingWidget.Get()) { Prev->SetIsEnemyActing(false); }
    EnemyActingWidget = nullptr;

    if (!NewActingEnemy) { return; }

    if (UUnitStatusWidget* Next = FindWidgetFor(NewActingEnemy))
    {
        Next->SetIsEnemyActing(true);
        EnemyActingWidget = Next;
    }
}

void UCombatHUDWidget::ClearAllHighlights()
{
    if (UUnitStatusWidget* W = ActivePlayerWidget.Get()) { W->SetIsActivePlayer(false); }
    if (UUnitStatusWidget* W = TargetEnemyWidget.Get())  { W->SetIsTargeted(false); }
    if (UUnitStatusWidget* W = EnemyActingWidget.Get())  { W->SetIsEnemyActing(false); }

    ActivePlayerWidget = nullptr;
    TargetEnemyWidget  = nullptr;
    EnemyActingWidget  = nullptr;

    // Belt-and-suspenders clear in case of stale pointers.
    for (UUnitStatusWidget* W : PlayerWidgets)
    {
        if (W) { W->SetIsActivePlayer(false); W->SetIsTargeted(false); W->SetIsEnemyActing(false); }
    }
    for (UUnitStatusWidget* W : EnemyWidgets)
    {
        if (W) { W->SetIsActivePlayer(false); W->SetIsTargeted(false); W->SetIsEnemyActing(false); }
    }
}

// -----------------------------------------------------------------------------
//  Turn info
// -----------------------------------------------------------------------------

void UCombatHUDWidget::RefreshTurnInfo()
{
    if (!TurnInfoWidget) { return; }

    ABattleManager* BM = BattleManager.Get();
    if (!BM) { return; }

    ACombatantBase* NextActor = nullptr;
    if (const UTurnOrderManager* TOM = BM->GetTurnOrderManager())
    {
        for (const FTurnEntry& Entry : TOM->GetCurrentQueue())
        {
            if (Entry.Combatant && !Entry.Combatant->IsDead())
            {
                NextActor = Entry.Combatant.Get();
                break;
            }
        }
    }

    TurnInfoWidget->Refresh(BM->GetTurnNumber(), NextActor);
}

// -----------------------------------------------------------------------------
//  Helpers
// -----------------------------------------------------------------------------

void UCombatHUDWidget::CollectUnitWidgets(UPanelWidget* Panel,
                                           TArray<TObjectPtr<UUnitStatusWidget>>& OutWidgets)
{
    OutWidgets.Empty();
    if (!Panel) { return; }

    for (int32 i = 0; i < Panel->GetChildrenCount(); ++i)
    {
        if (UUnitStatusWidget* W = Cast<UUnitStatusWidget>(Panel->GetChildAt(i)))
        {
            OutWidgets.Add(W);
        }
    }
}

UUnitStatusWidget* UCombatHUDWidget::FindWidgetFor(const ACombatantBase* Combatant) const
{
    if (!Combatant) { return nullptr; }

    const TArray<TObjectPtr<UUnitStatusWidget>>& Pool =
        (Combatant->GetTeam() == ECombatTeam::Player) ? PlayerWidgets : EnemyWidgets;

    for (const TObjectPtr<UUnitStatusWidget>& W : Pool)
    {
        if (W && W->GetUnit() == Combatant) { return W; }
    }
    return nullptr;
}
