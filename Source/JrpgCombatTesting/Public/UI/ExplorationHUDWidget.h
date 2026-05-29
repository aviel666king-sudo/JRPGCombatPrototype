#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "ExplorationHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UBorder;
class AJrpgGameMode;
class AExplorationPawn;

/**
 * UExplorationHUDWidget
 *
 * C++ parent for WBP_ExplorationHUD. The entire party-panel layout is now built
 * and styled in C++ (NativeConstruct) — the WBP only needs an empty Canvas Panel
 * root. No manual designer widgets, no BindWidget names to match.
 *
 *   Setup in editor:
 *     1. Open WBP_ExplorationHUD, delete every widget under the root canvas
 *        (keep the root Canvas Panel itself).
 *     2. Make sure its parent class is UExplorationHUDWidget (it already is).
 *     3. Ctrl+Alt+F11. The panel draws itself bottom-left.
 *
 * The panel auto-shows while the player holds Tab (reads
 * AExplorationPawn::IsPartyPanelOpen) and fills its rows every tick from the
 * GameMode HUD getters.
 */
UCLASS()
class JRPGCOMBATTESTING_API UExplorationHUDWidget : public UUserWidget
{
    GENERATED_BODY()

protected:

    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:

    static constexpr int32 NumRows = 3;

    // Dynamically created row widgets (one entry per party slot).
    TObjectPtr<UTextBlock>   NameTexts[NumRows];
    TObjectPtr<UTextBlock>   LevelTexts[NumRows];
    TObjectPtr<UProgressBar> HPBars[NumRows];
    TObjectPtr<UTextBlock>   HPTexts[NumRows];

    // Protocol footer counters.
    TObjectPtr<UTextBlock> HealCharges;
    TObjectPtr<UTextBlock> ReviveCharges;
    TObjectPtr<UTextBlock> APCharges;

    // Root container we add to the WBP canvas — toggled visible on Tab.
    TObjectPtr<UBorder> PanelRoot;

    TWeakObjectPtr<AJrpgGameMode>   CachedGameMode;
    TWeakObjectPtr<AExplorationPawn> CachedPawn;

    /** Build the whole panel tree and attach it to the root canvas. */
    void BuildLayout();

    /** Push one party slot's data into its row widgets. */
    void RefreshRow(int32 Index);

    /** Font helper using the engine default typeface. */
    static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);
};
