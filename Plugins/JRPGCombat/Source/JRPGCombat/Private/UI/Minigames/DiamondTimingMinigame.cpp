#include "UI/Minigames/DiamondTimingMinigame.h"

#include "Rendering/DrawElements.h"
#include "Fonts/SlateFontInfo.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "InputCoreTypes.h"
#include "Framework/Application/SlateApplication.h"

// =============================================================================
//  Constants
// =============================================================================

namespace DiamondMinigame
{
    static constexpr float Mult_BlueStrip = 1.10f;
    static constexpr float Mult_Red       = 1.00f;
    static constexpr float Mult_Adjacent  = 0.75f;
    static constexpr float Mult_Opposite  = 0.50f;
    static constexpr float Mult_NoPress   = 0.50f;

    // Fixed diamond radius in pixels — no longer a fraction of screen size.
    // Widget is 320×320, so radius 120 gives comfortable margins.
    static constexpr float DiamondRadius = 120.f;

    // Fade-in duration in seconds.
    static constexpr float FadeInDuration = 0.15f;

    // Zone labels
    static const TCHAR* ZoneLabel_Blue     = TEXT("110%  \u2605");
    static const TCHAR* ZoneLabel_Red      = TEXT("100%");
    static const TCHAR* ZoneLabel_Adjacent = TEXT("75%");
    static const TCHAR* ZoneLabel_Opposite = TEXT("50%");

    static const TCHAR* SupportLabel_Red      = TEXT("4 turns");
    static const TCHAR* SupportLabel_Adjacent = TEXT("3 turns");
    static const TCHAR* SupportLabel_Opposite = TEXT("2 turns");

    // Glow: a slightly larger, lower-opacity copy of each line drawn beneath it.
    static constexpr float GlowAlpha     = 0.30f;
    static constexpr float GlowThickness = 6.f;   // extra px per side for the blur feel
}

// =============================================================================
//  Lifecycle
// =============================================================================

void UDiamondTimingMinigame::NativeConstruct()
{
    Super::NativeConstruct();
    // Repaint is driven by Invalidate() in NativeTick.
}

void UDiamondTimingMinigame::StartMinigame_Implementation()
{
    Super::StartMinigame_Implementation(); // sets bIsActive = true

    CurrentAngle       = 0.0f;
    bCursorStopped     = false;
    FadeAlpha          = 0.0f;   // start invisible — fade in over FadeInDuration
    ElapsedFadeTime    = 0.0f;

    RedSideIndex = FMath::RandRange(1, 3);

    const float SideStart = RedSideIndex * 90.0f;
    const float MaxStart  = SideStart + 90.0f - BlueStripDegrees - 10.0f;
    const float MinStart  = SideStart + 10.0f;
    BlueStripStartAngle   = FMath::FRandRange(MinStart, MaxStart);

    if (APlayerController* PC = GetOwningPlayer())
    {
        SetUserFocus(PC);
    }
}

// =============================================================================
//  Input
// =============================================================================

FReply UDiamondTimingMinigame::NativeOnKeyDown(const FGeometry& InGeometry,
                                                const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();

    if (bIsActive && !bCursorStopped)
    {
        if (Key == EKeys::SpaceBar)
        {
            StopCursor();
            return FReply::Handled();
        }

        // Backspace cancels the minigame — player backs out to the skill menu.
        if (Key == EKeys::BackSpace)
        {
            CancelMinigame();
            return FReply::Handled();
        }
    }

    return FReply::Unhandled();
}

void UDiamondTimingMinigame::OnConfirmPressed_Implementation()
{
    if (bIsActive && !bCursorStopped)
    {
        StopCursor();
    }
}

void UDiamondTimingMinigame::StopCursor()
{
    bCursorStopped = true;
    BroadcastResult(ComputeMultiplierForAngle(CurrentAngle));
}

// =============================================================================
//  Tick
// =============================================================================

void UDiamondTimingMinigame::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (!bIsActive || bCursorStopped) { return; }

    // Fade-in
    if (FadeAlpha < 1.0f)
    {
        ElapsedFadeTime += InDeltaTime;
        FadeAlpha = FMath::Clamp(ElapsedFadeTime / DiamondMinigame::FadeInDuration, 0.f, 1.f);
    }

    const float DegreesPerSecond = 360.0f / FMath::Max(RotationDuration, 0.1f);
    CurrentAngle += DegreesPerSecond * InDeltaTime;

    Invalidate(EInvalidateWidgetReason::Paint);

    if (CurrentAngle >= 360.0f)
    {
        bCursorStopped = true;
        BroadcastResult(DiamondMinigame::Mult_NoPress);
    }
}

// =============================================================================
//  Paint
// =============================================================================

int32 UDiamondTimingMinigame::NativePaint(const FPaintArgs& Args,
                                           const FGeometry& AllottedGeometry,
                                           const FSlateRect& MyCullingRect,
                                           FSlateWindowElementList& OutDrawElements,
                                           int32 LayerId,
                                           const FWidgetStyle& InWidgetStyle,
                                           bool bParentEnabled) const
{
    LayerId = Super::NativePaint(Args, AllottedGeometry, MyCullingRect,
                                 OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);

    const float A = FadeAlpha;   // global opacity for fade-in
    if (A <= 0.f) { return LayerId; }

    const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
    const FVector2D Centre    = LocalSize * 0.5f;

    // ── 1. Subtle dark card behind the diamond (not full-screen) ─────────────
    //    Rounded-looking semi-transparent panel — 240×240 centred.
    {
        const FVector2D CardSize(240.f, 240.f);
        const FVector2D CardPos = Centre - CardSize * 0.5f;
        const FSlateColorBrush CardBrush(FLinearColor(0.f, 0.f, 0.f, 0.55f * A));
        FSlateDrawElement::MakeBox(
            OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(CardSize, FSlateLayoutTransform(CardPos)),
            &CardBrush);
        ++LayerId;
    }

    // ── 2. Diamond vertices ───────────────────────────────────────────────────
    FVector2D Verts[4];
    GetDiamondVertices(LocalSize, Verts);

    // ── 3. Glow pass — wider, dimmer lines drawn first ────────────────────────
    for (int32 i = 0; i < 4; ++i)
    {
        FLinearColor GlowColor = GetSideColor(i);
        GlowColor.A = DiamondMinigame::GlowAlpha * A;
        DrawLine(OutDrawElements, LayerId, AllottedGeometry,
                 Verts[i], Verts[(i+1)%4], GlowColor,
                 LineThickness + DiamondMinigame::GlowThickness);
    }
    ++LayerId;

    // ── 4. Diamond sides ──────────────────────────────────────────────────────
    for (int32 i = 0; i < 4; ++i)
    {
        FLinearColor Col = GetSideColor(i);
        Col.A *= A;
        DrawLine(OutDrawElements, LayerId, AllottedGeometry,
                 Verts[i], Verts[(i+1)%4], Col, LineThickness);
    }
    ++LayerId;

    // ── 5. Blue strip overlay (damage skills) ─────────────────────────────────
    if (!bIsSupportAbility)
    {
        const FVector2D& RV1 = Verts[RedSideIndex];
        const FVector2D& RV2 = Verts[(RedSideIndex + 1) % 4];
        const float SideStart = RedSideIndex * 90.0f;
        const float t1 = (BlueStripStartAngle - SideStart) / 90.0f;
        const float t2 = t1 + BlueStripDegrees / 90.0f;
        const FVector2D BlueA = FMath::Lerp(RV1, RV2, FMath::Clamp(t1, 0.f, 1.f));
        const FVector2D BlueB = FMath::Lerp(RV1, RV2, FMath::Clamp(t2, 0.f, 1.f));

        // Glow
        DrawLine(OutDrawElements, LayerId, AllottedGeometry, BlueA, BlueB,
                 FLinearColor(0.2f, 0.55f, 1.0f, DiamondMinigame::GlowAlpha * A),
                 LineThickness + DiamondMinigame::GlowThickness + 4.f);
        // Solid strip
        DrawLine(OutDrawElements, LayerId, AllottedGeometry, BlueA, BlueB,
                 FLinearColor(0.2f, 0.55f, 1.0f, A), LineThickness + 4.f);
        ++LayerId;
    }

    // ── 6. Cursor with glow ───────────────────────────────────────────────────
    {
        const FVector2D CursorPos = GetCursorPosition(LocalSize, CurrentAngle);

        // Glow: larger, lower-opacity yellow square behind cursor
        const float GlowSize = CursorSize + DiamondMinigame::GlowThickness;
        DrawCentredBox(OutDrawElements, LayerId, AllottedGeometry,
                       CursorPos, FVector2D(GlowSize, GlowSize),
                       FLinearColor(1.f, 0.92f, 0.f, DiamondMinigame::GlowAlpha * A));
        ++LayerId;

        // Solid cursor
        DrawCentredBox(OutDrawElements, LayerId, AllottedGeometry,
                       CursorPos, FVector2D(CursorSize, CursorSize),
                       FLinearColor(1.f, 0.92f, 0.f, A));
        ++LayerId;
    }

    // ── 7. "SPACE" centre label ───────────────────────────────────────────────
    {
        const FSlateFontInfo SpaceFont = FCoreStyle::GetDefaultFontStyle("Bold", 20);
        const FVector2D TextSize(120.f, 28.f);
        const FVector2D TextPos = Centre - TextSize * 0.5f;
        FSlateDrawElement::MakeText(
            OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(TextSize, FSlateLayoutTransform(TextPos)),
            FText::FromString(TEXT("SPACE")),
            SpaceFont, ESlateDrawEffect::None,
            FLinearColor(1.f, 1.f, 1.f, 0.85f * A));
        ++LayerId;
    }

    // ── 8. Zone preview label ─────────────────────────────────────────────────
    if (bIsActive && !bCursorStopped)
    {
        const float Mult = ComputeMultiplierForAngle(CurrentAngle);
        FString Label;
        FLinearColor LabelColor = FLinearColor::White;

        if (bIsSupportAbility)
        {
            if      (Mult >= 1.0f) { Label = DiamondMinigame::SupportLabel_Red;      LabelColor = FLinearColor(1.f, 0.3f, 0.3f); }
            else if (Mult >= 0.75f){ Label = DiamondMinigame::SupportLabel_Adjacent; LabelColor = FLinearColor(0.8f,0.8f,0.8f); }
            else                   { Label = DiamondMinigame::SupportLabel_Opposite; LabelColor = FLinearColor(0.4f,0.4f,0.4f); }
        }
        else
        {
            if      (Mult > 1.0f)  { Label = DiamondMinigame::ZoneLabel_Blue;     LabelColor = FLinearColor(0.2f,0.6f,1.f);  }
            else if (Mult >= 1.0f) { Label = DiamondMinigame::ZoneLabel_Red;      LabelColor = FLinearColor(1.f, 0.3f,0.3f); }
            else if (Mult >= 0.75f){ Label = DiamondMinigame::ZoneLabel_Adjacent; LabelColor = FLinearColor(0.8f,0.8f,0.8f); }
            else                   { Label = DiamondMinigame::ZoneLabel_Opposite; LabelColor = FLinearColor(0.4f,0.4f,0.4f); }
        }

        LabelColor.A *= A;

        const FSlateFontInfo PreviewFont = FCoreStyle::GetDefaultFontStyle("Bold", 16);
        const FVector2D PreviewSize(120.f, 24.f);
        const FVector2D PreviewPos = Centre + FVector2D(-60.f, 22.f);
        FSlateDrawElement::MakeText(
            OutDrawElements, LayerId,
            AllottedGeometry.ToPaintGeometry(PreviewSize, FSlateLayoutTransform(PreviewPos)),
            FText::FromString(Label),
            PreviewFont, ESlateDrawEffect::None,
            LabelColor);
        ++LayerId;
    }

    return LayerId;
}

// =============================================================================
//  Geometry helpers
// =============================================================================

void UDiamondTimingMinigame::GetDiamondVertices(const FVector2D& WidgetSize,
                                                  FVector2D OutVerts[4]) const
{
    // Use fixed radius — no longer dependent on widget size fraction.
    const float R  = DiamondMinigame::DiamondRadius;
    const FVector2D C = WidgetSize * 0.5f;

    OutVerts[0] = C + FVector2D(0.f,  -R);  // Top
    OutVerts[1] = C + FVector2D(R,   0.f);  // Right
    OutVerts[2] = C + FVector2D(0.f,   R);  // Bottom
    OutVerts[3] = C + FVector2D(-R,  0.f);  // Left
}

FVector2D UDiamondTimingMinigame::GetCursorPosition(const FVector2D& WidgetSize, float Angle) const
{
    FVector2D Verts[4];
    GetDiamondVertices(WidgetSize, Verts);
    const int32 SideIdx = AngleToSideIndex(Angle);
    const float t       = FMath::Fmod(Angle, 90.0f) / 90.0f;
    return FMath::Lerp(Verts[SideIdx], Verts[(SideIdx + 1) % 4], t);
}

int32 UDiamondTimingMinigame::AngleToSideIndex(float Angle)
{
    const float Clamped = FMath::Fmod(FMath::Max(Angle, 0.f), 360.f);
    return FMath::FloorToInt(Clamped / 90.f) % 4;
}

bool UDiamondTimingMinigame::IsAngleInBlueStrip(float Angle) const
{
    return !bIsSupportAbility
        && Angle >= BlueStripStartAngle
        && Angle <  BlueStripStartAngle + BlueStripDegrees;
}

FLinearColor UDiamondTimingMinigame::GetSideColor(int32 SideIndex) const
{
    if (SideIndex == RedSideIndex)         { return FLinearColor(0.90f, 0.15f, 0.15f); }
    const int32 Opp = (RedSideIndex + 2) % 4;
    if (SideIndex == Opp)                  { return FLinearColor(0.25f, 0.25f, 0.25f); }
    return                                   FLinearColor(0.55f, 0.55f, 0.55f);
}

float UDiamondTimingMinigame::ComputeMultiplierForAngle(float Angle) const
{
    if (IsAngleInBlueStrip(Angle))                          { return DiamondMinigame::Mult_BlueStrip; }
    if (AngleToSideIndex(Angle) == RedSideIndex)            { return DiamondMinigame::Mult_Red; }
    if (AngleToSideIndex(Angle) == (RedSideIndex + 2) % 4) { return DiamondMinigame::Mult_Opposite; }
    return DiamondMinigame::Mult_Adjacent;
}

// =============================================================================
//  Draw utilities
// =============================================================================

void UDiamondTimingMinigame::DrawLine(FSlateWindowElementList& DrawElements,
                                       int32 Layer, const FGeometry& Geom,
                                       const FVector2D& A, const FVector2D& B,
                                       const FLinearColor& Color, float Thickness) const
{
    TArray<FVector2D> Points = { A, B };
    FSlateDrawElement::MakeLines(
        DrawElements, Layer, Geom.ToPaintGeometry(), Points,
        ESlateDrawEffect::None, Color, true, Thickness);
}

void UDiamondTimingMinigame::DrawCentredBox(FSlateWindowElementList& DrawElements,
                                             int32 Layer, const FGeometry& Geom,
                                             const FVector2D& Centre,
                                             const FVector2D& Size,
                                             const FLinearColor& Color) const
{
    const FVector2D TopLeft = Centre - Size * 0.5f;
    const FSlateColorBrush Brush(Color);
    FSlateDrawElement::MakeBox(
        DrawElements, Layer,
        Geom.ToPaintGeometry(Size, FSlateLayoutTransform(TopLeft)),
        &Brush);
}
