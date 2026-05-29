#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Characters/Player/PlayerCombatant.h"   // EUpgradeStat
#include "StatShopWidget.generated.h"

class UTextBlock;
class UPanelWidget;
class AJrpgGameMode;

/**
 * UStatShopButton
 *
 * A UButton that remembers which party member + stat it upgrades, so a single
 * click handler can route to the right purchase. The owning widget binds
 * OnUpgradeClicked.
 */
UCLASS()
class JRPGCOMBATTESTING_API UStatShopButton : public UButton
{
    GENERATED_BODY()

public:
    int32        MemberIndex = 0;
    EUpgradeStat Stat        = EUpgradeStat::MaxHP;

    /** Set by the shop widget; fired with this button's member + stat. */
    TFunction<void(int32, EUpgradeStat)> OnUpgradeClicked;

    UFUNCTION()
    void HandleClicked();
};

/**
 * UStatShopWidget
 *
 * Fully C++-built stat-shop overlay (no WBP). Opened at checkpoints by
 * AExplorationPawn. Shows each party member's level, StatCoin balance and
 * current MaxHP/Attack/Defense/Speed, with a buy button per stat that spends
 * StatCoins via APlayerCombatant::TryUpgradeStat.
 *
 * Create with CreateWidget<UStatShopWidget>(PC) — the tree builds itself in
 * RebuildWidget, so no Blueprint asset is required.
 */
UCLASS()
class JRPGCOMBATTESTING_API UStatShopWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Pawn sets this so the in-widget Close button can dismiss the shop. */
    TFunction<void()> OnCloseRequested;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    static constexpr int32 MaxMembers = 3;
    static constexpr int32 NumStats   = 4;   // matches EUpgradeStat

    TObjectPtr<UPanelWidget> MemberColumns[MaxMembers];
    TObjectPtr<UTextBlock>   NameTexts[MaxMembers];
    TObjectPtr<UTextBlock>   LevelTexts[MaxMembers];
    TObjectPtr<UTextBlock>   CoinTexts[MaxMembers];
    TObjectPtr<UTextBlock>   StatValueTexts[MaxMembers][NumStats];
    TObjectPtr<UStatShopButton> BuyButtons[MaxMembers][NumStats];

    TWeakObjectPtr<AJrpgGameMode> CachedGameMode;

    void BuildLayout(UPanelWidget* Root);
    UPanelWidget* BuildMemberColumn(int32 Index);
    void Refresh();
    void HandleUpgrade(int32 MemberIndex, EUpgradeStat Stat);

    APlayerCombatant* GetMember(int32 Index) const;

    static FSlateFontInfo MakeFont(int32 Size, bool bBold = false);
    static const TCHAR* StatLabel(EUpgradeStat Stat);
};
