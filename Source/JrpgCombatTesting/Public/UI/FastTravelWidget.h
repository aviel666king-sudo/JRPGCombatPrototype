#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "FastTravelWidget.generated.h"

class UTextBlock;
class UPanelWidget;
class UVerticalBox;

/**
 * UFastTravelButton
 *
 * Per-entry button that remembers which checkpoint id it teleports to.
 */
UCLASS()
class JRPGCOMBATTESTING_API UFastTravelButton : public UButton
{
    GENERATED_BODY()

public:
    FName TargetCheckpointId;
    TFunction<void(FName)> OnTravelClicked;

    UFUNCTION()
    void HandleClicked();
};

/**
 * UFastTravelWidget
 *
 * Pure-C++ overlay opened from a checkpoint's "[F] Fast Travel" key. Lists
 * every checkpoint the player has rested at in the current level (excluding
 * the one they're standing at), plus a Close button. Click one to teleport.
 *
 * Resolution: the widget asks UVisitedCheckpointRegistry for the visited ids
 * in the current level, then walks the world for ACheckpoint actors with
 * matching ids to grab their transforms.
 *
 * Lifetime: AExplorationPawn creates this on F-press at a checkpoint, sets
 * OriginCheckpointId so the originating checkpoint is filtered out of the
 * list, and provides OnCloseRequested for the Close button.
 */
UCLASS()
class JRPGCOMBATTESTING_API UFastTravelWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    /** Set by the pawn before AddToViewport. The widget hides this id from the
     *  list so the player doesn't fast-travel to where they already are. */
    FName OriginCheckpointId;

    /** Pawn sets this so the in-widget Close button can dismiss the screen. */
    TFunction<void()> OnCloseRequested;

protected:

    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:

    TObjectPtr<UVerticalBox> ListBox;

    void Populate();
    void HandleTravelClicked(FName CheckpointId);

    static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);
};
