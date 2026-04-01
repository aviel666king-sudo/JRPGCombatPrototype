#pragma once

#include "CoreMinimal.h"
#include "UI/Minigames/AbilityMinigameWidget.h"
#include "DiamondTimingMinigame.generated.h"

/**
 * UDiamondTimingMinigame
 *
 * The first skill minigame: a rotated square (diamond) where the player
 * presses SPACE to stop a cursor that travels clockwise around the perimeter.
 *
 * ZONES
 * ──────
 *  Side 0  (top→right)   : Always the first side — NEVER red.  Guarantees
 *                           reaction time before the red zone appears.
 *  Red side (1, 2, or 3) : Landing here = 100% damage.
 *  Blue strip             : A 20° window inside the red side = 110% damage.
 *                           Not shown / not active for support abilities.
 *  Adjacent sides (×2)   : 75% damage (or 3 turns for support).
 *  Opposite side  (×1)   : 50% damage (or 2 turns for support).
 *  No press               : 50% (cursor completes full rotation).
 *
 * CURSOR ROTATION
 * ────────────────
 *  0°  = top vertex.
 *  Moves clockwise: top → right → bottom → left → (full circle in one pass).
 *  If SPACE is not pressed the cursor stops at 360° and fires the 0.50 result.
 *
 * VISUAL LAYOUT
 * ──────────────
 *  Dark overlay background.
 *  Diamond drawn with thick coloured lines.
 *  Side colours:
 *    – red side         : red
 *    – adjacent sides   : mid grey
 *    – opposite side    : dark grey
 *    – blue strip       : bright cyan (not drawn for support abilities)
 *  Yellow square cursor tracks the perimeter.
 *  "SPACE" text centred inside the diamond.
 *  Zone preview label below diamond shows the result if pressed now.
 *
 * USAGE (no UMG Blueprint required — widget is drawn entirely in C++)
 * ────────────────────────────────────────────────────────────────────
 *  1. Set MinigameClass = UDiamondTimingMinigame on a UCombatAbility.
 *  2. CombatActionPanelWidget creates, shows, and manages the widget
 *     automatically when that skill is selected.
 *  3. To tune speed / strip size expose RotationDuration / BlueStripDegrees
 *     via Blueprint defaults on a subclass.
 */
UCLASS(Blueprintable)
class JRPGCOMBAT_API UDiamondTimingMinigame : public UAbilityMinigameWidget
{
    GENERATED_BODY()

public:

    // -------------------------------------------------------------------------
    //  Tunables
    // -------------------------------------------------------------------------

    /** Total seconds for one full rotation. Default: 2.5s. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigame|Diamond")
    float RotationDuration = 2.5f;

    /** Angular width of the blue strip in degrees. Default: 20°. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigame|Diamond")
    float BlueStripDegrees = 20.0f;

    /** Thickness of the diamond perimeter lines in pixels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigame|Diamond")
    float LineThickness = 10.0f;

    /** Size of the cursor square in pixels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Minigame|Diamond")
    float CursorSize = 16.0f;

    // -------------------------------------------------------------------------
    //  UAbilityMinigameWidget interface
    // -------------------------------------------------------------------------

    virtual void StartMinigame_Implementation() override;
    virtual void OnConfirmPressed_Implementation() override;

    // -------------------------------------------------------------------------
    //  UUserWidget / UWidget interface
    // -------------------------------------------------------------------------

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    virtual int32 NativePaint(const FPaintArgs& Args,
                               const FGeometry& AllottedGeometry,
                               const FSlateRect& MyCullingRect,
                               FSlateWindowElementList& OutDrawElements,
                               int32 LayerId,
                               const FWidgetStyle& InWidgetStyle,
                               bool bParentEnabled) const override;

    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry,
                                   const FKeyEvent& InKeyEvent) override;

    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

private:

    // ── Runtime state ────────────────────────────────────────────────────────

    /** Current cursor angle in degrees (0 = top, clockwise). */
    float CurrentAngle = 0.0f;

    /** Which diamond side is red (1, 2, or 3 — never 0). Randomised each run. */
    int32 RedSideIndex = 1;

    /** Start of the blue strip in absolute degrees. Randomised each run. */
    float BlueStripStartAngle = 0.0f;

    /** True once the player presses SPACE (or the rotation completes). */
    bool bCursorStopped = false;

    /** Current opacity for the fade-in animation [0,1]. */
    float FadeAlpha = 0.0f;

    /** Time elapsed since StartMinigame (used for fade-in). */
    float ElapsedFadeTime = 0.0f;

    // ── Drawing helpers ───────────────────────────────────────────────────────

    /**
     * Fill OutVerts[4] with the four diamond vertices in local widget space.
     * Order: 0=Top, 1=Right, 2=Bottom, 3=Left.
     */
    void GetDiamondVertices(const FVector2D& WidgetSize,
                            FVector2D OutVerts[4]) const;

    /** World-space cursor position for CurrentAngle. */
    FVector2D GetCursorPosition(const FVector2D& WidgetSize,
                                float Angle) const;

    /** Which side index (0–3) a given angle falls on. */
    static int32 AngleToSideIndex(float Angle);

    /** Is a given angle inside the blue strip? */
    bool IsAngleInBlueStrip(float Angle) const;

    /** Line colour for a side (accounts for red / adjacent / opposite). */
    FLinearColor GetSideColor(int32 SideIndex) const;

    /** Compute the final multiplier for whatever angle the cursor stopped at. */
    float ComputeMultiplierForAngle(float Angle) const;

    /** Stop the cursor and broadcast the result. */
    void StopCursor();

    // ── Paint helpers ─────────────────────────────────────────────────────────

    /** Draw a thick line between two local-space points. */
    void DrawLine(FSlateWindowElementList& DrawElements,
                  int32 Layer,
                  const FGeometry& Geom,
                  const FVector2D& A,
                  const FVector2D& B,
                  const FLinearColor& Color,
                  float Thickness) const;

    /** Draw a filled rectangle centred on a local-space point. */
    void DrawCentredBox(FSlateWindowElementList& DrawElements,
                        int32 Layer,
                        const FGeometry& Geom,
                        const FVector2D& Centre,
                        const FVector2D& Size,
                        const FLinearColor& Color) const;
};
