#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VictoryScreenWidget.generated.h"

class UProgressBar;
class UTextBlock;

/**
 * One party member's XP progression for the animated victory bar. Plain struct
 * (C++-only, passed from the game mode). Thresholds[k] is the XP needed to
 * clear level (StartLevel + k), so the bar can roll through several level-ups.
 */
struct FVictoryMemberXP
{
    FText Name;
    int32 StartLevel = 1;
    int32 StartXP    = 0;
    int32 XPGained   = 0;
    TArray<int32> Thresholds;
};

/**
 * UVictoryScreenWidget
 *
 * Post-victory results panel with an animated XP bar per member: the bar fills
 * over ~1.2s, rolls over on each level crossed, and shows a "+N" level-up tag
 * beside the bar. Spoils (gold / materials / loot) render as static rows.
 * Pure C++ UMG — no WBP required.
 */
UCLASS()
class JRPGCOMBATTESTING_API UVictoryScreenWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Animated XP rows (one per party member). Set before AddToViewport. */
    TArray<FVictoryMemberXP> Members;

    /** Static spoils rows (gold / materials / loot). */
    TArray<FString> SpoilLines;

    TFunction<void()> OnContinueRequested;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UFUNCTION()
    void HandleContinue();

private:
    // Per-member widget refs, parallel to Members.
    TArray<TObjectPtr<UProgressBar>> Bars;
    TArray<TObjectPtr<UTextBlock>>   LevelLabels;
    TArray<TObjectPtr<UTextBlock>>   PopupLabels;

    float Elapsed  = 0.f;
    float Duration = 1.2f;

    FTimerHandle AnimTimer;
    void AdvanceAnim();
    void ApplyMemberState(int32 Index, float EasedAlpha);
};
