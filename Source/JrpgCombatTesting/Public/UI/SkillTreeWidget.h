#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "SkillTreeWidget.generated.h"

class UTextBlock;
class UCanvasPanel;
class APlayerCombatant;

/**
 * USkillNodeButton
 *
 * A UButton that remembers which skill-tree node it represents so a single
 * click handler can route the unlock attempt.
 */
UCLASS()
class JRPGCOMBATTESTING_API USkillNodeButton : public UButton
{
    GENERATED_BODY()

public:
    FName NodeId;
    TFunction<void(FName)> OnNodeClicked;

    UFUNCTION()
    void HandleClicked();
};

/**
 * USkillTreeWidget
 *
 * Fully C++-built skill-tree overlay (no WBP). Shows the first party fencer's
 * branching tree as a grid of node buttons placed by FSkillNode::GridPos.
 * Clicking an unlockable node spends SkillCoins via TryUnlockNode; locked
 * (prereq-unmet) and unaffordable nodes are dimmed. Opened by AExplorationPawn.
 */
UCLASS()
class JRPGCOMBATTESTING_API USkillTreeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Pawn sets this so the Close button can dismiss the tree. */
    TFunction<void()> OnCloseRequested;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    TObjectPtr<UCanvasPanel> NodeCanvas;
    TObjectPtr<UTextBlock>   CoinText;
    TObjectPtr<UTextBlock>   EquippedText;

    // Parallel arrays: one entry per node in the tree.
    TArray<FName>                    NodeIds;
    TArray<TObjectPtr<USkillNodeButton>> NodeButtons;
    TArray<TObjectPtr<UTextBlock>>   NodeStateTexts;

    TWeakObjectPtr<APlayerCombatant> CachedFencer;

    void BuildLayout(class UPanelWidget* Root);
    void BuildNodes();
    void Refresh();
    void HandleNodeClicked(FName NodeId);

    APlayerCombatant* ResolveFencer() const;

    static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);
};
